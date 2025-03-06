# Press F PSX

![image](https://github.com/user-attachments/assets/99c67523-5ab8-4032-9a82-c9f75babbcdc)

**Press F PSX** is a work-in-progress Fairchild Channel F emulator for Sony PlayStation, utilizing the **[libpressf](https://github.com/celerizer/libpressf)** emulation library.

## Controls

| | PlayStation | Channel F |
|-|-|-|
| . | Control Pad | Directional movement |
| . | Square | Rotate counter-clockwise |
| . | Circle | Rotate clockwise |
| . | Triangle | Pull up |
| . | Cross | Plunge down |
| . | L1 | 1 / TIME |
| . | L2 | 2 / MODE |
| . | R1 | 3 / HOLD |
| . | R2 | 4 / START |

## Building
- Set up a [PSYQo environment](https://github.com/grumpycoders/pcsx-redux/blob/main/src/mips/psyqo/GETTING_STARTED.md).
- Clone the project and the required submodules:
```sh
git clone https://github.com/celerizer/Press-F-PSX.git --recurse-submodules
```
- Run `make`.
- Optionally, package the output ps-exe into a disc image using BUILDCD and PSXLICENSE, available [here](https://www.psxdev.net/downloads.html).

## Build information

Press F PSX has been configured to use the following **libpressf** settings:

| Field | Value | Reason |
|--|--|--|
| PF_FLOATING_POINT | 0 | The PSX does not have an FPU |
| PF_SOUND_FREQUENCY | 22050 | The reverb buffer hack used for audio requires this frequency |
| PF_NO_DMA / PF_NO_DMA_SIZE | 1 / 40960 | The PSX has inconsistent handling of alloc/free |

## License
**Press F PSX**, **libpressf**, and **nugget** are distributed under the MIT license. See LICENSE for information.
