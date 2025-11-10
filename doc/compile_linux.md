### Steps to compile on Linux

Create a new folder and cd into
```
mkdir ~/tommy
cd ~/tommy
```

Download game source code
```
wget https://raw.githubusercontent.com/joerivandervelde/the-lost-tommy/refs/heads/main/tommy.c -O tommy.c
```

Install SLD2
```
wget https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-2.32.10.tar.gz -O sdl.tar.gz
tar xzfv sdl.tar.gz
cd SDL2-2.32.10
./configure # try configure, if this fails, see below
make -j4 # increase to use more cores
cd ~/tommy # step out
mv SDL2-2.32.10/include/ SDL2-2.32.10/SDL2 # rename folder to match source code path
```

If you have problem building SDL2 like `*** Missing Xext.h, maybe you need to install the libxext-dev package?` missing dependencies must be added, e.g.
```
# still inside the SDL2-2.32.10 folder
apt download libxext-dev #-> depending on linux distro, might also be yum download libXext-devel
dpkg -x libxext-dev_*.deb . #-> depending on linux distro, might also be rpm2cpio libXext-devel-1.3.4-8.el9.x86_64.rpm | cpio -idmv
./configure CPPFLAGS="-Iusr/include" LDFLAGS="-Lusr/lib64" # try configure including libxext
```

Compile
```
gcc tommy.c -ISDL2-2.32.10 -LSDL2-2.32.10/build/.libs -lSDL2 -lm -Wl,-rpath,"." -o tommy
```

Add shared library, must be in same folder as executable (via rpath)
```
cp SDL2-2.32.10/build/.libs/libSDL2-2.0.so.0 .
```

Run
```
./tommy
```
