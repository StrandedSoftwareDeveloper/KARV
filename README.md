# KARV: <Kerbal|Kitten> Avionics RISC-V
Largely inspired by [KOS](https://github.com/KSP-KOS/KOS), KARV aims to add a fully functional RISC-V computer to KSP (see `KARV_KSP/`) and KSA (see `KARV_KSA/`) so that your rockets can run Linux, and you'll be able to program them in whatever language you can get to run in said Linux.  
Uses CNLohr's wonderful [mini-rv32ima](https://github.com/cnlohr/mini-rv32ima) library.

Currently early in development, missing many features, and rather broken.

All code in this repository is, unless otherwise specified, under the MIT license.  
`default64mbdtc.h` is from [mini-rv32ima](https://github.com/cnlohr/mini-rv32ima/blob/master/mini-rv32ima/default64mbdtc.h), Copyright (c) 2022 CNLohr  
`mini-rv32ima.h` has very minor modifications.