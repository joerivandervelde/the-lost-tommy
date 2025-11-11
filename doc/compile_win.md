### Steps to compile on Microsoft Windows x86_64

Use PowerShell. Create a new folder and cd into
```
mkdir ~/tommy
cd ~/tommy
```

Download game source code
```
wget https://raw.githubusercontent.com/joerivandervelde/the-lost-tommy/refs/heads/main/tommy.c -O tommy.c
```

Download and extract gcc-win64
* Note 1: look for  gcc-win64 on SourceForge if the URL changes
* Note 2: install [7-Zip](https://www.7-zip.org/) if you don't have it yet
```
wget https://unlimited.dl.sourceforge.net/project/gcc-win64/15.2.0/gcc-15.2.0-gdb-16.3.90.20250511-binutils-2.45-mingw-w64-v13.0.0-ucrt.7z?viasf=1 -O gcc.7z
& "${env:ProgramFiles}\7-Zip\7z.exe" x gcc.7z -ogcc -y
```

Download and extract SDL2
```
wget https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-devel-2.32.10-mingw.tar.gz -O sdl.tar.gz
tar xzfv sdl.tar.gz
```

Compile
```
.\gcc\bin\gcc.exe tommy.c -I SDL2-2.32.10\x86_64-w64-mingw32\include -L SDL2-2.32.10\x86_64-w64-mingw32\lib -lmingw32 -lSDL2main -lSDL2 -mwindows -o tommy.exe
```

Add DLL, must be in same folder as executable
```
cp .\SDL2-2.32.10\x86_64-w64-mingw32\bin\SDL2.dll .
```

All done. Run by double click or from commandline:
```
.\tommy.exe
```

Cleanup (optional)
```
rm *.7z
rm *.tar.gz
```
