# KARV: Kitten Avionics RISC-V
Largely inspired by [KOS](https://github.com/KSP-KOS/KOS), KARV aims to add a fully functional RISC-V computer to KSA so that your rockets can run Linux, and you'll be able to program them in whatever language you can get to run in said Linux.  
Uses CNLohr's wonderful [mini-rv32ima](https://github.com/cnlohr/mini-rv32ima) library.

Currently early in development, missing many features, and rather broken.

All code in this repository is, unless otherwise specified, under the MIT license.  
`default64mbdtc.h` is from [mini-rv32ima](https://github.com/cnlohr/mini-rv32ima/blob/master/mini-rv32ima/default64mbdtc.h), Copyright (c) 2022 CNLohr  
`mini-rv32ima.h` has very minor modifications.

## Building
To build the C# half, set the `$KSA_DEV_FOLDER` environment variable to point to your KSA folder (the one with KSA.exe), then run `dotnet build` in the root folder of this repository. It copies `KARV.dll` to `$KSA_DEV_FOLDER/Content/KARV/KARV.dll`, but currently only if that folder already exists.

To build the native C half, I'm using zig 0.15.2 at the moment:  
`zig 0.15.2 cc --target=x86_64-windows libkarv/libkarv.c -o libkarv.dll -shared -fPIC`  
At the moment you have to manually copy this to the mod folder, next to `KARV.dll`

### Visual Studio? Rider? $MY_FAVORITE_DOTNET_IDE?
There's probably a way. Maybe it'll work out of the box, maybe everything will explode, who knows!  
Let me know about your results if you try it.

## Running
You'll need a `linux.bin`, I got mine from here:
https://github.com/cnlohr/mini-rv32ima-images/raw/master/images/linux-6.1.14-rv32nommu-cnl-1.zip  
Unzip it and rename `Image` to `linux.bin`.
`linux.bin` and the `Codepage-437.png` in the root of this repository (yes, it's still needed despite no visuals in this version yet) both need to go in `$KSA_DEV_FOLDER`, *not* the mod folder.  
I understand this is bad practice, I'll get it fixed at some point
