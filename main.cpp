#include "psyqo/application.hh"
#include "psyqo/font.hh"
#include "psyqo/gpu.hh"
#include "psyqo/scene.hh"
#include "psyqo/simplepad.hh"

#include "tmp/bios_a.h"
#include "tmp/bios_b.h"
#include "tmp/cart.h"

#include "common/hardware/dma.h"
#include "common/hardware/spu.h"

#include <cstring>

extern "C"
{
  #include "libpressf/src/emu.h"
  #include "libpressf/src/input.h"
  #include "libpressf/src/screen.h"
  #include "libpressf/src/hw/beeper.h"
  #include "libpressf/src/hw/system.h"
  #include "libpressf/src/hw/vram.h"
};

static f8_system_t m_System;

namespace
{
  class Hello final : public psyqo::Application
  {
    void prepare() override;
    void createScene() override;

  public:
    psyqo::Font<> m_systemFont;
    psyqo::SimplePad pad;
  };

  class HelloScene final : public psyqo::Scene
  {
    void frame() override;

    uint8_t m_anim = 0;
    bool m_direction = true;
  };

  Hello hello;
  HelloScene helloScene;
}

void Hello::prepare()
{
  /* Init video */
  psyqo::GPU::Configuration config;
  config.set(psyqo::GPU::Resolution::W320)
        .set(psyqo::GPU::VideoMode::AUTO)
        .set(psyqo::GPU::ColorMode::C15BITS)
        .set(psyqo::GPU::Interlace::PROGRESSIVE);
  gpu().initialize(config);

  /* Init input */
  pad.initialize();
}

void Hello::createScene()
{
  m_systemFont.uploadSystemFont(gpu());
  pushScene(&helloScene);
}

#define SPU_DMA_CHUNK_SIZE 16
#define SPU_PTR_UNIT 8

/**
 * Play Channel F audio through the PSX, if enabled.
 * 
 * As the PSX does not generally support an uncompressed PCM audio stream, we
 * use an interesting hardware hack to trick it into playing our sound.
 * 
 * The SPU has the ability to generate reverb effects from the audio it is
 * supplied. However, even if the reverb system is disabled, the samples in
 * its workspace still get output to the speakers. By abusing the ability to
 * manually set the reverb buffer location address and write directly into it,
 * we can "play" silence and stream our actual uncompressed audio into the
 * reverb buffer.
 */
static void pfx_audio(void)
{
#if PF_AUDIO_ENABLE
  int16_t *samples = ((f8_beeper_t*)m_System.f8devices[7].device)->samples;
  unsigned size = PF_SOUND_SAMPLES * 2 * sizeof(int16_t);
  unsigned bcr = ((size / (SPU_DMA_CHUNK_SIZE * sizeof(uint32_t))) << 16) | SPU_DMA_CHUNK_SIZE;
  unsigned transfer_addr = (0x80000 - size) / SPU_PTR_UNIT;

  /* Ensure audio is playing and reverb is disabled */
  SPU_CTRL = 0xA002;

  /* Ensure SPU DMA access is enabled and highest priority */
  DPCR = (DPCR & ~0x00070000) | 0x00080000;

  /* Setup volume settings -- perhaps only do once? */
  SPU_VOL_MAIN_LEFT = 0x3800;
  SPU_VOL_MAIN_RIGHT = 0x3800;
  SPU_KEY_ON_LOW = 0;
  SPU_KEY_ON_HIGH = 0;
  SPU_KEY_OFF_LOW = 0xffff;
  SPU_KEY_OFF_HIGH = 0xffff;
  SPU_RAM_DTC = 4;
  SPU_VOL_CD_LEFT = 0;
  SPU_VOL_CD_RIGHT = 0;
  SPU_PITCH_MOD_LOW = 0;
  SPU_PITCH_MOD_HIGH = 0;
  SPU_NOISE_EN_LOW = 0;
  SPU_NOISE_EN_HIGH = 0;
  SPU_REVERB_EN_LOW = 0;
  SPU_REVERB_EN_HIGH = 0;
  SPU_VOL_EXT_LEFT = 0;
  SPU_VOL_EXT_RIGHT = 0;
  SPU_REVERB_LEFT = 0x3fff;
  SPU_REVERB_RIGHT = 0x3fff;

  /* From instructions on setting up an SPU DMA transfer here: @todo */
  /* Be sure that [1F801DACh] is set to 0004h */
  SPU_RAM_DTC = 4;

  /* Set SPUCNT to "Stop" (and wait until it is applied in SPUSTAT) */
  SPU_CTRL = SPU_CTRL & ~0x0030;
  while ((SPU_STATUS & 0x0030) != 0);

  /* Set the transfer address */
  SPU_RAM_DTA = transfer_addr;

  /* # Extra step: Also set the reverb buffer work area start to this address -- perhaps only once or if changed? */
  HW_U16(0x1F801DA2) = transfer_addr;

  /* Set SPUCNT to "DMA Write" (and wait until it is applied in SPUSTAT) */
  SPU_CTRL = SPU_CTRL | 0x0020;
  while ((SPU_STATUS & 0x0030) != 0x0020);

  /* Start DMA4 at CPU Side (blocksize=10h, control=01000201h) */
  DMA_CTRL[DMA_SPU].MADR = reinterpret_cast<uint32_t>(samples);
  DMA_CTRL[DMA_SPU].BCR = bcr;
  DMA_CTRL[DMA_SPU].CHCR = 0x01000201;

  /* Wait until DMA4 finishes (at CPU side) */
  while ((DMA_CTRL[DMA_SPU].CHCR & 0x01000000) != 0);
#endif
}

/* Update the Channel F system buttons and controllers with PSX pads */
static void pfx_input()
{
  using psyqo::SimplePad;
  const auto pad_1 = SimplePad::Pad1;
  const auto pad_2 = SimplePad::Pad2;

  set_input_button(0, INPUT_TIME, hello.pad.isButtonPressed(pad_1, SimplePad::Button::L1));
  set_input_button(0, INPUT_MODE, hello.pad.isButtonPressed(pad_1, SimplePad::Button::L2));
  set_input_button(0, INPUT_HOLD, hello.pad.isButtonPressed(pad_1, SimplePad::Button::R1));
  set_input_button(0, INPUT_START, hello.pad.isButtonPressed(pad_1, SimplePad::Button::R2));

  /* Handle player 1 input */
  set_input_button(4, INPUT_RIGHT, hello.pad.isButtonPressed(pad_1, SimplePad::Button::Right));
  set_input_button(4, INPUT_LEFT, hello.pad.isButtonPressed(pad_1, SimplePad::Button::Left));
  set_input_button(4, INPUT_BACK, hello.pad.isButtonPressed(pad_1, SimplePad::Button::Down));
  set_input_button(4, INPUT_FORWARD, hello.pad.isButtonPressed(pad_1, SimplePad::Button::Up));
  set_input_button(4, INPUT_ROTATE_CCW, hello.pad.isButtonPressed(pad_1, SimplePad::Button::Square));
  set_input_button(4, INPUT_ROTATE_CW, hello.pad.isButtonPressed(pad_1, SimplePad::Button::Circle));
  set_input_button(4, INPUT_PULL, hello.pad.isButtonPressed(pad_1, SimplePad::Button::Triangle));
  set_input_button(4, INPUT_PUSH, hello.pad.isButtonPressed(pad_1, SimplePad::Button::Cross));

  /* Handle player 2 input */
  set_input_button(1, INPUT_RIGHT, hello.pad.isButtonPressed(pad_2, SimplePad::Button::Right));
  set_input_button(1, INPUT_LEFT, hello.pad.isButtonPressed(pad_2, SimplePad::Button::Left));
  set_input_button(1, INPUT_BACK, hello.pad.isButtonPressed(pad_2, SimplePad::Button::Down));
  set_input_button(1, INPUT_FORWARD, hello.pad.isButtonPressed(pad_2, SimplePad::Button::Up));
  set_input_button(1, INPUT_ROTATE_CCW, hello.pad.isButtonPressed(pad_2, SimplePad::Button::Square));
  set_input_button(1, INPUT_ROTATE_CW, hello.pad.isButtonPressed(pad_2, SimplePad::Button::Circle));
  set_input_button(1, INPUT_PULL, hello.pad.isButtonPressed(pad_2, SimplePad::Button::Triangle));
  set_input_button(1, INPUT_PUSH, hello.pad.isButtonPressed(pad_2, SimplePad::Button::Cross));
}

static u16 frame[VRAM_WIDTH * VRAM_HEIGHT];

static void pfx_video(void)
{
  u8 *vram_data = ((vram_t*)m_System.f8devices[3].device)->data;

  draw_frame_rgb5551(vram_data, frame);

  psyqo::Rect region = { .pos={{ .x=16, .y=(128 + (hello.gpu().getParity() ? 256 : 0)) }}, .size={{ .w=SCREEN_WIDTH, .h=SCREEN_HEIGHT }} };
  hello.gpu().uploadToVRAM(frame, region);
}

void HelloScene::frame()
{
  if (m_anim == 0)
    m_direction = true;
  else if (m_anim == 255)
    m_direction = false;

  psyqo::Color bg{{.r = 0, .g = 64, .b = 91}};
  bg.r = m_anim;
  hello.gpu().clear(bg);
  if (m_direction)
    m_anim++;
  else
    m_anim--;
  
  pfx_input();

  pressf_run(&m_System);

  pfx_audio();

  pfx_video();

  psyqo::Color c = {{.r = 255, .g = 255, .b = uint8_t(255 - m_anim)}};
  hello.m_systemFont.print(hello.gpu(), "Channel F is here and now...", {{.x = 16, .y = 16}}, c);
  hello.m_systemFont.print(hello.gpu(), "...on Sony PlayStation!", {{.x = 16, .y = 32}}, c);
  
  hello.m_systemFont.printf(hello.gpu(), {{.x = 16, .y = 64}}, c, "PC0:%04X 1:%04X DC0:%04X 1:%04X",
    m_System.pc0.u, m_System.pc1.u, m_System.dc0.u, m_System.dc1.u);

  hello.m_systemFont.printf(hello.gpu(), {{.x = 16, .y = 80}}, c, "Clock %d", m_System.settings.f3850_clock_speed);
}

int main(void)
{ 
  /* Initialize emulator */
  pressf_init(&m_System);
  f8_system_init(&m_System, F8_SYSTEM_CHANNEL_F);

  f8_write(&m_System, 0x0000, sl31253_bin, sl31253_bin_len);
  f8_write(&m_System, 0x0400, sl31254_bin, sl31254_bin_len);
  f8_write(&m_System, 0x0800, cart_bin, cart_bin_len);

  SPU_VOL_MAIN_LEFT = 0x3800;
  SPU_VOL_MAIN_RIGHT = 0x3800;
  SPU_KEY_ON_LOW = 0;
  SPU_KEY_ON_HIGH = 0;
  SPU_KEY_OFF_LOW = 0xffff;
  SPU_KEY_OFF_HIGH = 0xffff;
  SPU_RAM_DTC = 4;
  SPU_VOL_CD_LEFT = 0;
  SPU_VOL_CD_RIGHT = 0;
  SPU_PITCH_MOD_LOW = 0;
  SPU_PITCH_MOD_HIGH = 0;
  SPU_NOISE_EN_LOW = 0;
  SPU_NOISE_EN_HIGH = 0;
  SPU_REVERB_EN_LOW = 0;
  SPU_REVERB_EN_HIGH = 0;
  SPU_VOL_EXT_LEFT = 0;
  SPU_VOL_EXT_RIGHT = 0;
  SPU_REVERB_LEFT = 0x3fff;
  SPU_REVERB_RIGHT = 0x3fff;

  SBUS_DEV4_CTRL = 0
		| ( 1 << 0) // Write delay
		| (14 << 4) // Read delay
		| ( 1 << 8) // Control recovery
		| ( 1 << 12) // Width 16
		| ( 1 << 13) // Auto increment
		| ( 9 << 16) // Number of address lines
		| ( 0 << 24) // DMA read/write delay
		| ( 1 << 29); // DMA delay

  return hello.run();
}
