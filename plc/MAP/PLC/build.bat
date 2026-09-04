@echo off

setlocal EnableDelayedExpansion

set PREFIX=%RISCV_INSTALL_PATH%\bin\riscv32-unknown-elf
set CFLAGS=-g -ggdb -nostdlib -nostdinc -ffreestanding -march=rv32imaf -mabi=ilp32f -Wall -Werror -std=c2x -I. -Iinclude -I%SAT_SDK_PATH%/include
set OBJ=

del /s *.o  >nul 2>&1

for /F "tokens=*" %%F in  (.vscode\project.ini) do (
    echo Compiling %%F....
    @%PREFIX%-gcc %CFLAGS% -c %%F || goto :error

    set OBJ=!OBJ! %%~nF.o
)

REM Порядок параметров важен!
%PREFIX%-ld -L %SAT_SDK_PATH%\lib -T satplc.ld -nostdlib -m elf32lriscv -Map main.map -o main.out %OBJ% -lsatstd || goto :error

%PREFIX%-objcopy -O binary main.out main.bin
%PREFIX%-objdump -drlS main.out > main.lst

echo Build completed successfully!

:error
@exit /b %errorlevel%
