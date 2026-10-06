# TPA-CNC

Программа для настройки и управления двигателем WECON VD2F / VD2.

## SATURN-PLC C23 пресс (`plc/`)

Прошивка пресс-формы. Исходники: `plc/MAP/PLC/`.

## SATURN-PLC C23 термо (`termo/`)

Проект SatPlcStudio **TPA-TERMO**: Saturn-PLC, C23, IP `192.168.1.10`.

Четыре зоны нагрева шнека/сопла: МВ110 (RTU #16) + МР-DOR13 (RTU #1), уставки в EEPROM, карта Modbus TCP с адреса 8192 (526 регистров).

Исходники: `termo/MAP/PLC/` (`main.c`, `thermo.c`). Сборка: `cd termo\MAP\PLC` и `build.bat`.
