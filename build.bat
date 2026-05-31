@echo off
g++ -o ping_app.exe main.cpp -lwinhttp -static -s
if %errorlevel% equ 0 (
    echo Build successful! Run ping_app.exe to start.
) else (
    echo Build failed.
)
