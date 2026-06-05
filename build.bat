@echo off
g++ -Os -s -static -static-libgcc -static-libstdc++ -o ping_app.exe main.cpp -lwinhttp -lgdiplus -lgdi32 -lcrypt32 -lole32 -lvfw32 -lwinmm -lbcrypt -mwindows
if %errorlevel% equ 0 (
    echo Build successful!
) else (
    echo Build failed.
)
