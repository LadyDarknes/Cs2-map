@echo off
setlocal
set "VS_PATH=D:\Microsoft VS\VC\Auxiliary\Build\vcvarsall.bat"

if not exist "%VS_PATH%" (
    echo [!] HATA: vcvarsall.bat'ı kendime göre ayarladım.
    echo Lutfen VS_PATH degiskenini cl.exe'nin oldugu gercek yola gore guncelle, muhtemelen benim yukarida verdigim path'in. C konumundadir.
    pause
    exit /b
)

call "%VS_PATH%" x64

echo.
echo [+] x64 Native Tools
echo.
if exist build rd /s /q build
cmake -G "Ninja" -B build -DCMAKE_BUILD_TYPE=Release

if %errorlevel% neq 0 (
    echo [!] Yapilandirma sirasinda hata olustu!
    pause
    exit /b
)

echo [+] Derleme basliyor...
cmake --build build --config Release > build_log.txt 2>&1
type build_log.txt

if %errorlevel% neq 0 (
    echo.
    echo [!] DERLEME BASARISIZ! bkz. "build_log.txt"
) else (
    echo.
    echo [OK] Derleme basarili!
)

pause