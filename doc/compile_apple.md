### Steps to compile on Apple silicon

Create a new folder and cd into
```
mkdir ~/tommy
cd ~/tommy
```

Install Xcode 

```
xcode-select --install
```

Install SDL2
```
brew install sdl2
```

Download source code
```
wget https://raw.githubusercontent.com/joerivandervelde/the-lost-tommy/refs/heads/main/tommy.c -O tommy.c
```

Compile
```
clang -v tommy.c -I/opt/homebrew/include -L/opt/homebrew/lib -lSDL2 -o tommy
```

Run
```
./tommy
```
TODO: rework to only use minimal setup e.g. download gcc/sdl and complile

