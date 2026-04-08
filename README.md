<img alt="banner" src="https://github.com/user-attachments/assets/c3d1fa1e-30da-42f7-803f-1a4eb08cacb5" />
<div align="center">
  <img alt="wiiu" height="56" src="https://github.com/user-attachments/assets/d9074ba2-455c-4083-b8a8-da9150814065">
  <a href="https://go.nsmbu.net/discord">
    <img alt="discord" height="56" src="https://github.com/user-attachments/assets/9c31e6cc-e2f8-4d53-9507-15a17b368088">
  </a>
  <a href="https://zenith.nsmbu.net/wiki/TuneBloom">
    <img alt="docs" height="56" src="https://github.com/user-attachments/assets/67b764c4-f2d0-470a-8f12-206d3d0203fc">
  </a>
</div>

## Overview
TuneBloom is an extremely fast & stable editor for editing NintendoWare audio formats, primarily for the Wii U. It features high performance, low memory usage, and a high degree of data accuracy, allowing you to modify everything throughout the archive while ensuring that nothing will go wrong in the process. Support is included for every component of sound archives including waves, sequences, streams, banks, and more. Live playback allows quick previews of your work resulting in a highly effective and streamlined workflow. This is TuneBloom.

## Features
- Playback for every type of sound!
- Sequence (`BFSEQ`) editor!
- Bank (`BFBNK`) editor!
- Edit every single property of the BFSAR!
- And more!

## Screenshots
<div align="center">
  <img width="400" alt="TuneBloom_PBCw2isamn" src="https://github.com/user-attachments/assets/94a97581-4367-4f2d-b4da-c75b59e53aeb" />
  <img width="400" alt="TuneBloom_itmqEg2BRg" src="https://github.com/user-attachments/assets/c91e57e4-7f56-4f61-bc1d-723a3bcf2e7f" />
  <img width="400" alt="TuneBloom_D5QeeacBr2" src="https://github.com/user-attachments/assets/4cfd9de4-1e5c-4fc9-9a25-60ac6f400c0c" />
  <img width="400" alt="TuneBloom_Ot0x2Fng9e" src="https://github.com/user-attachments/assets/e84fc549-f740-4888-ae22-bd142aef731a" />
</div>

## Future Goals
- `BCSAR` support. (3DS)
- `BFSAR` support. (Switch)

## Limitations
- External groups (`BFGRP`)
  - Support for external groups is still WIP (read-only), that means games like SM3DW, CTTT and any other game which makes use of external groups only supports playback.
- Import/Export wave files
  - The tool can only make use of .wav files for importing/replacing and exporting.

## Compiling
- Premake is used as the build system which makes setting up a development environment very straightforward.
- Simply run the `setupVS.bat` script in the root of the repository and then open the generated `.sln` file in Visual Studio to edit & build.
- Currently, only Windows is supported with MSVC & Clang, but support for other platforms is upcoming.

## Contributing
External contributions & Pull Requests are welcome! We understand that the quality of the code may not be fully up-to-bar, so we appreciate your patience. Help and developer discussions are always available in our [Discord](https://go.nsmbu.net/discord) server.

## Credits
- [STUPID Modder](https://github.com/stupidestmodder) - Development lead
- [AboodXD](https://github.com/aboood40091) - Research & algorithms
- [Luminyx1](https://github.com/Luminyx1) - Build system & testing
- [Alex Barney](https://github.com/Thealexbarney) - DspTool (Adpcm Encoding)
- [omar](https://github.com/ocornut) - ImGui
- [Vladimir Shatrov](https://github.com/frowrik) - ImGui piano keyboard
