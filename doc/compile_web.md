### Steps to compile for WebAssembly wasm32

These instructions work for the macOS 15.6.1 default Terminal but should work for other systems as well.

Create a new folder and step into.
```
mkdir ~/tommy
cd ~/tommy
```

Download game source code.
```
wget https://raw.githubusercontent.com/joerivandervelde/the-lost-tommy/refs/heads/main/tommy.c -O tommy.c
```

Download, install and activate Emscripten.
```
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh   # on Windows: emsdk_env.bat
cd ..
```

Compile game from source.
```
emcc tommy.c -O3 -s USE_SDL=2 -s ALLOW_MEMORY_GROWTH=1 -o index.html
```

Run game via simple web server.
```
emrun --no_browser --port 8080 index.html
# or: python -m http.server 8080
```
