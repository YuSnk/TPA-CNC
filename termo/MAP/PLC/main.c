#include <satplc.h>
#include <satscr.h>
#include <satmio.h>
#include "thermo.h"

int main(void)
{
    uint32_t t0;

    ClrScr();
    printf("TPA-TERMO\n");
    ShowScr();
    Beep(200);
    msleep(300);

    if (!ThermoInit()) {
        puts(HRED "Init error");
        ShowScr();
        while (1) {
            Beep(100);
            msleep(500);
        }
    }

    t0 = GetTickCount();
    while (1) {
        ThermoCheckTcpWrites();
        if ((GetTickCount() - t0) >= 1000u) {
            t0 = GetTickCount();
            ThermoScan();
        }
        ThermoDraw();
        msleep(50);
    }
    return 1;
}
