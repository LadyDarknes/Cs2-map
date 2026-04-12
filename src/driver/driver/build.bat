@echo off
setlocal enabledelayedexpansion

echo ============================================
echo   Lem1337 Kernel Driver Build Script
echo ============================================
echo.

:: Use vswhere to find VS
for /f "tokens=*" %%i in ('"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath 2^>nul') do set "VS_PATH=%%i"

if not defined VS_PATH (
    echo [!] Visual Studio not found!
    pause
    exit /b 1
)
echo [+] VS: %VS_PATH%

:: Find latest MSVC toolset
for /d %%t in ("%VS_PATH%\VC\Tools\MSVC\*") do set "MSVC_DIR=%%t"
set "CL_EXE=%MSVC_DIR%\bin\Hostx64\x64\cl.exe"
set "LINK_EXE=%MSVC_DIR%\bin\Hostx64\x64\link.exe"
set "MSVC_LIB=%MSVC_DIR%\lib\x64"
set "MSVC_INC=%MSVC_DIR%\include"
echo [+] CL: %CL_EXE%

:: Find WDK
set "WDK_ROOT=C:\Program Files (x86)\Windows Kits\10"
if not exist "%WDK_ROOT%\Include" (
    set "WDK_ROOT=C:\Program Files\Windows Kits\10"
)
set "SDK_VER="
for /d %%v in ("%WDK_ROOT%\Include\10.*") do set "SDK_VER=%%~nxv"
set "WI=%WDK_ROOT%\Include\%SDK_VER%"
set "WL=%WDK_ROOT%\Lib\%SDK_VER%"
echo [+] WDK: %SDK_VER%
echo.

echo [*] Compiling driver.c ...
"%CL_EXE%" /c /nologo /Zp8 /Gy /W3 /WX- /Gz /GR- /GF /Ox /Ob2 /D _AMD64_ /D _WIN64 /D NTDDI_VERSION=0x0A000000 "/I%WI%\km" "/I%WI%\km\crt" "/I%WI%\shared" "/I%WI%\um" "/I%MSVC_INC%" /Fo"driver.obj" /kernel driver.c
if %ERRORLEVEL% neq 0 (
    echo [!] Compilation failed!
    pause
    exit /b 1
)

echo [+] Compilation OK
echo [*] Linking driver.sys ...
"%LINK_EXE%" /nologo /DRIVER /SUBSYSTEM:NATIVE /ENTRY:DriverEntry /OUT:"driver.sys" "/LIBPATH:%WL%\km\x64" "/LIBPATH:%MSVC_LIB%" ntoskrnl.lib hal.lib wmilib.lib BufferOverflowFastFailK.lib driver.obj
if %ERRORLEVEL% neq 0 (
    echo [!] Linking failed!
    pause
    exit /b 1
)

del /q driver.obj 2>nul

echo.
echo ============================================
echo [+] driver.sys built successfully!
echo ============================================
echo.
pause
