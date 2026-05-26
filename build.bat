@echo off
setlocal

cd /d "%~dp0"

if "%PLANKAC_ROOT%"=="" set "PLANKAC_ROOT=%~dp0..\PlankaMath"

if not exist "%PLANKAC_ROOT%\c\include\plankac.h" (
    echo PlankaC header was not found.
    echo Set PLANKAC_ROOT to the PlankaC checkout path.
    exit /b 1
)

where gcc >nul 2>nul
if errorlevel 1 (
    echo gcc was not found in PATH.
    exit /b 1
)

if not exist build mkdir build

if not exist "%PLANKAC_ROOT%\build\libplankac.a" (
    echo Building PlankaC dependency...
    call "%PLANKAC_ROOT%\build.bat"
    if errorlevel 1 exit /b 1
)

set PS_INC=-I"%PLANKAC_ROOT%\c\include"
set PS_LIB="%PLANKAC_ROOT%\build\libplankac.a"
set PS_DEFS=-DWINVER=0x0501 -D_WIN32_WINNT=0x0501
set PS_CONSOLE_LD=-Wl,--subsystem,console:5.01 -static -static-libgcc
set PS_WINDOWS_LD=-Wl,--subsystem,windows:5.01 -static -static-libgcc

echo Building PlankaStarten CLI...
gcc -Wall -Wextra -std=c99 %PS_DEFS% %PS_INC% src\plankastarten_cli.c src\plankastarten_compile.c %PS_LIB% -o build\plankastarten_cli.exe %PS_CONSOLE_LD% -lm
if errorlevel 1 exit /b 1

echo Building PlankaStarten GUI...
windres src\plankastarten.rc -O coff -o build\plankastarten_res.o
if errorlevel 1 exit /b 1

gcc -Wall -Wextra -std=c99 %PS_DEFS% %PS_INC% src\plankastarten_gui.c src\plankastarten_compile.c build\plankastarten_res.o %PS_LIB% -o build\PlankaStarten.exe %PS_WINDOWS_LD% -lcomctl32 -lcomdlg32 -lshell32 -lole32 -lgdi32 -lm
if errorlevel 1 exit /b 1

echo Running API smoke tests...
build\plankastarten_cli.exe check examples\max3.plk
if errorlevel 1 exit /b 1

build\plankastarten_cli.exe list examples\max3.plk
if errorlevel 1 exit /b 1

build\plankastarten_cli.exe run examples\max3.plk start
if errorlevel 1 exit /b 1

build\plankastarten_cli.exe compile examples\max3.plk
if errorlevel 1 exit /b 1

build\max3_console.exe start
if errorlevel 1 exit /b 1

build\plankastarten_cli.exe bytecode examples\max3.plk build\max3.pbc
if errorlevel 1 exit /b 1

build\plankastarten_cli.exe ir examples\max3.plk build\max3.ir
if errorlevel 1 exit /b 1

build\plankastarten_cli.exe evidence examples\max3.plk build\max3.evidence.json
if errorlevel 1 exit /b 1

build\plankastarten_cli.exe cgen examples\max3.plk build\max3_generated.c
if errorlevel 1 exit /b 1

build\plankastarten_cli.exe asmgen examples\max3.plk build\max3_runtime.S
if errorlevel 1 exit /b 1

build\plankastarten_cli.exe asm8086 examples\max3.plk build\max3_8086.asm
if errorlevel 1 exit /b 1

findstr /c:"plankac-evidence-v1" build\max3.evidence.json >nul
if errorlevel 1 exit /b 1

echo Done.
