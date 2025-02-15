# KARV: Kerbal Avionics RISC-V
Largely inspired by [KOS](https://github.com/KSP-KOS/KOS), KARV aims to add a fully functional RISC-V computer to KSP so that your rockets can run Linux, and you'll be able to program them in whatever language you can get to run in said Linux.  
Uses CNLohr's wonderful [mini-rv32ima](https://github.com/cnlohr/mini-rv32ima) library.

Currently early in development, missing many features, and rather broken.

All code in this repository is, unless otherwise specified, under the MIT license.
`mini-rv32ima.h` has very minor modifications.

## Building
On Linux, I think you just need mono and gcc (other C compilers should work if you swap the line in `AfterBuild.sh`).
Set the `$KSP_DEV_FOLDER` environment variable to point to your KSP folder (the one with KSP.x86_64), then just run `msbuild` in the root folder of this repository.

Building on other platforms is not supported at the moment, though at least building on Windows is planned.
