### Steps to compile on Linux x86_64

These instructions work for the Ubuntu 24.04 LTS terminal. Adjust for your Linux distro accordingly.

Create a new folder and step into.
```
mkdir ~/tommy
cd ~/tommy
```

Download game source code.
```
wget https://raw.githubusercontent.com/joerivandervelde/the-lost-tommy/refs/heads/main/tommy.c -O tommy.c
```

Install gcc, git and make.
```
sudo apt install gcc
sudo apt install git
sudo apt install make
```

Install x11 and wayland dev packages for their headers. Without these, SDL2 will compile for offscreen rendering only.
```
sudo apt install \
build-essential \
libx11-dev libxext-dev libxrandr-dev libxrender-dev libxfixes-dev \
libxinerama-dev libxcursor-dev libxi-dev \
libxkbcommon-dev libxkbcommon-x11-dev \
libwayland-dev wayland-protocols \
libdrm-dev libgbm-dev \
libegl1-mesa-dev libgles2-mesa-dev
```

Download and compile SDL2.
```
git clone https://github.com/libsdl-org/SDL.git -b SDL2
cd SDL
mkdir build
cd build
../configure
make
```

Step back and rename path for compilation.
```
cd ~/tommy
mv SDL/include SDL/SDL2
```

Compile game from source.
```
gcc tommy.c -ISDL -LSDL/build/build/.libs -lSDL2 -lm -Wl,-rpath,"." -o tommy
```

Place shared library next to executable, linked via rpath in compilation.
```
cp SDL/build/build/.libs/libSDL2-2.0.so.0 .
```

Run game from commandline.
```
./tommy
```

Package for distribution.
```
tar czfv tommy_linux_x86_64.tar.gz tommy libSDL2-2.0.so.0
```

#### NOTE

You could just install SDL2 on your system and compile against it like this:
```
sudo apt install  libsdl2-dev
gcc tommy.c -o tommy `sdl2-config --cflags --libs` -lm
```
But the executable will then depend on it, meaning that on another or fresh system it won't work until you install SDL2.
