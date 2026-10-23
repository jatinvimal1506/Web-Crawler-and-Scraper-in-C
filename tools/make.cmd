@echo off
where mingw32-make.exe >nul 2>nul
if errorlevel 1 (
    echo mingw32-make.exe was not found. Configure MSYS2 UCRT64 on PATH.
    exit /b 1
)
mingw32-make.exe %*
