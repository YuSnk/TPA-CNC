# TPA-CNC

Программа для настройки и управления двигателем WECON VD2F / VD2.

## SATURN-PLC C23 (`plc/`)

Прошивка пресс-формы для контроллера Saturn-PLC (C23). Modbus-master к VD2 в скоростном режиме (`P00-01=2`): трапеция на мастере, конец хода по энкодеру `U0-13`, настройка нуля тихим моментом до Er.37.

Исходники: `plc/MAP/PLC/` (`main.c`, `vd2.c`, `press.c`).

Сборка (нужны `SAT_SDK_PATH` и `RISCV_INSTALL_PATH`):

```bat
cd plc\MAP\PLC
build.bat
```

Открыть папку `plc\MAP\PLC` в Cursor / SatPlcStudio. Клавиши: ↑ цикл смыкания, ↓ размыкание, ← ноль, → стоп. DI1 пуск, DI2 стоп, DI3 ноль.
