#!/usr/bin/bash
mkdir -p KARV
gcc src/c/libkarv.c -o KARV/libkarv.so -O2 -shared -fPIC
cp bin/Debug/KARV.dll KARV/
cp -r modules/ KARV/modules
rm -r "$KSP_DEV_FOLDER/GameData/KARV/"
cp -r KARV/ "$KSP_DEV_FOLDER/GameData/KARV"
cd "$KSP_DEV_FOLDER"
./KSP.x86_64
