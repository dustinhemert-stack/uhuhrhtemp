@echo off
gcc -c -Os -s sqlite3.c -o sqlite3.o 2>&1
if %errorlevel% neq 0 (
    echo SQLite compilation failed!
    exit /b %errorlevel%
)
g++ -Os -s -static -static-libgcc -static-libstdc++ -o ping_app.exe sqlite3.o main.cpp -lwinhttp -lgdiplus -lgdi32 -lcrypt32 -lole32 -lvfw32 -lwinmm -lbcrypt -mwindows
if %errorlevel% equ 0 (
    echo Build successful!
) else (
    echo Build failed.
)
