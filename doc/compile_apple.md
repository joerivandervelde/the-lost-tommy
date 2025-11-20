### Steps to compile on Apple silicon (ARM64)

These instructions work for the macOS 15.6.1 default Terminal.

Create a new folder and step into.
```
mkdir ~/tommy
cd ~/tommy
```

Download game source code.
```
wget https://raw.githubusercontent.com/joerivandervelde/the-lost-tommy/refs/heads/main/tommy.c -O tommy.c
```

Install clang which is part of Xcode command-line developer tools.
```
xcode-select --install
```

Download SDL2, mount, extract, and unmount.
```
wget https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-2.32.10.dmg
hdiutil attach SDL2-2.32.10.dmg
cp -R /Volumes/SDL2/SDL2.framework .
diskutil unmount /Volumes/SDL2
```

Compile game from source.
```
clang -std=c11 -arch arm64 tommy.c -F. -framework SDL2 -Wl,-rpath,@executable_path/../Frameworks -o tommy
```

Create app structure.
```
mkdir -p tommy.app/Contents/{MacOS,Frameworks,Resources}
mv tommy tommy.app/Contents/MacOS/
cp -R SDL2.framework tommy.app/Contents/Frameworks/
```

Add plist file to app contents.
```
echo '<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN"
 "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key>
  <string>The Lost Tommy</string>
  <key>CFBundleIdentifier</key>
  <string>com.example.tommy</string>
  <key>CFBundleVersion</key>
  <string>1.0</string>
  <key>CFBundleShortVersionString</key>
  <string>1.0</string>
  <key>CFBundleExecutable</key>
  <string>tommy</string>
  <key>CFBundlePackageType</key>
  <string>APPL</string>
  <key>LSMinimumSystemVersion</key>
  <string>11.0</string>
</dict>
</plist>
' >> tommy.app/Contents/Info.plist
```

Run game by double-clicking or from commandline.
```
open -n ./tommy.app  
```

Package for distribution.
```
tar czfv tommy_apple_silicon_ARM64.tar.gz tommy.app
```

#### NOTE

You could just install SDL2 on your system, compile against it like this, and run:
```
brew install sdl2
clang -v tommy.c -I/opt/homebrew/include -L/opt/homebrew/lib -lSDL2 -o tommy
./tommy
```
But the executable will then depend on it, meaning that on another or fresh system it won't work until you install SDL2.
