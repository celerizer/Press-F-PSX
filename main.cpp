/*

MIT License

Copyright (c) 2022 PCSX-Redux authors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include "psyqo/application.hh"
#include "psyqo/font.hh"
#include "psyqo/gpu.hh"
#include "psyqo/scene.hh"
#include "psyqo/strings-helpers.hh"

#include "tmp/bios_a.h"
#include "tmp/bios_b.h"

extern "C"
{
  #include "libpressf/src/emu.h"
  #include "libpressf/src/hw/system.h"
};

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#if PF_ROMC
#error "Makefile is fucked up"
#endif

static f8_system_t m_System;

namespace {

// A PSYQo software needs to declare one `Application` object.
// This is the one we're going to do for our hello world.
class Hello final : public psyqo::Application {
    void prepare() override;
    void createScene() override;

  public:
    psyqo::Font<> m_systemFont;
    psyqo::Font<> m_romFont;
};

// And we need at least one scene to be created.
// This is the one we're going to do for our hello world.
class HelloScene final : public psyqo::Scene {
    void frame() override;

    // We'll have some simple animation going on, so we
    // need to keep track of our state here.
    uint8_t m_anim = 0;
    bool m_direction = true;

    
};

// We're instantiating the two objects above right now.
Hello hello;
HelloScene helloScene;

}  // namespace

void Hello::prepare()
{
  psyqo::GPU::Configuration config;

  /* Initialize GPU */
  config.set(psyqo::GPU::Resolution::W320)
        .set(psyqo::GPU::VideoMode::AUTO)
        .set(psyqo::GPU::ColorMode::C15BITS)
        .set(psyqo::GPU::Interlace::PROGRESSIVE);
  gpu().initialize(config);
}

void Hello::createScene() {
    // We're going to use two fonts, one from the system, and one from the kernel rom.
    // We need to upload them to VRAM first. The system font is 256x48x4bpp, and the
    // kernel rom font is 256x90x4bpp. We're going to upload them to the same texture
    // page, so we need to make sure they don't overlap. The default location for the
    // system font is {{.x = 960, .y = 464}}, and the default location for the kernel
    // rom font is {{.x = 960, .y = 422}}, so we need to nudge the kernel rom
    // font up a bit.
    m_systemFont.uploadSystemFont(gpu());
    m_romFont.uploadKromFont(gpu(), {{.x = 960, .y = int16_t(512 - 48 - 90)}});
    pushScene(&helloScene);
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
  
  pressf_step(&m_System);

  psyqo::Color c = {{.r = 255, .g = 255, .b = uint8_t(255 - m_anim)}};
  hello.m_systemFont.print(hello.gpu(), "Channel F is here and now...", {{.x = 16, .y = 16}}, c);
  hello.m_systemFont.print(hello.gpu(), "...on Sony PlayStation!", {{.x = 16, .y = 32}}, c);
  
  hello.m_systemFont.printf(hello.gpu(), {{.x = 16, .y = 48}}, c, "PC0:%04X 1:%04X DC0:%04X 1:%04X",
    m_System.pc0.u, m_System.pc1.u, m_System.dc0.u, m_System.dc1.u);

  hello.m_systemFont.printf(hello.gpu(), {{.x = 16, .y = 64}}, c, "%02X %02X %02X %02X",
    m_System.memory[0].u, m_System.memory[1].u, m_System.memory[2].u, m_System.memory[3].u);

  hello.m_systemFont.printf(hello.gpu(), {{.x = 16, .y = 80}}, c, "%02X %02X %02X %02X",
    sl31253_bin[0], sl31253_bin[1], sl31253_bin[2], sl31253_bin[3]);
}

int main(void)
{ 
  /* Initialize emulator */
  pressf_init(&m_System);
  f8_system_init(&m_System, &pf_systems[F8_SYSTEM_CHANNEL_F]);

  f8_write(&m_System, 0x0000, sl31253_bin, sl31253_bin_len);
  f8_write(&m_System, 0x0400, sl31254_bin, sl31254_bin_len);

  return hello.run();
}
