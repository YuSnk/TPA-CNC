#include <satplc.h>
#include <satscr.h>
#include <satmio.h>
#include <satkbd.h>
#include "vd2.h"
#include "press.h"

#define DI_START        DI1
#define DI_STOP         DI2
#define DI_ZERO         DI3
#define DO_RUN          DO1
#define DO_ALARM        DO7

static int prev_start;
static int prev_zero;
static int prev_stop;

static int rise(int now, int *prev)
{
    int r = (now && !*prev);
    *prev = now;
    return r;
}

static void draw(void)
{
    unsigned char hour, min, sec, date, month;
    unsigned short year;

    GetLocalDateTime(&date, &month, &year, &hour, &min, &sec);

    ClrScr();
    GotoXY(0, 0);
    puts(HWHT "TPA-Press C23\n");
    printf(HBLU "%02u:%02u:%02u\n", hour, min, sec);

    if (!PressOnline() && PressState() != PS_INIT) {
        puts(HRED "VD2 offline\n");
    } else {
        printf(HGRN "ST %s\n", PressStateName());
    }
    printf(HYEL "pos=%ld\n", (long)PressPos());
    printf(HYEL "n=%d rpm\n", PressSpeed());
    printf(HMAG "Er=%u  0=%d\n", PressFault(), PressZeroOk());
    if (PressState() == PS_FAULT) {
        printf(HRED "%s\n", PressFaultText());
    }
    puts(HCYN "UP close  DN open\n");
    puts(HCYN "LT zero   RT stop\n");
    ShowScr();
}

int main(void)
{
    ClrScr();
    printf("TPA-Press C23\n");
    ShowScr();
    SetDO(DO_ALARM, 1);
    Beep(200);
    msleep(300);
    SetDO(DO_ALARM, 0);

    PressInit();
    if (!Vd2Open()) {
        puts(HRED "RS-485 open fail");
        ShowScr();
        while (1) {
            Beep(100);
            msleep(500);
        }
    }

    SetEnableKeyboard(1);
    PressStart();

    while (1) {
        int k = GetKey();
        int di_start = GetDI(DI_START);
        int di_stop  = GetDI(DI_STOP);
        int di_zero  = GetDI(DI_ZERO);

        if (rise(di_stop, &prev_stop) || (k & KEY_RIGHT)) {
            PressCmdStop();
        } else if (rise(di_zero, &prev_zero) || (k & KEY_LEFT)) {
            PressCmdZero();
        } else if (rise(di_start, &prev_start) || (k & KEY_UP)) {
            PressCmdClose();
        } else if (k & KEY_DOWN) {
            PressCmdOpen();
        }

        PressTick();
        SetDO(DO_RUN, (PressState() != PS_IDLE && PressState() != PS_FAULT && PressState() != PS_INIT) ? 1 : 0);
        SetDO(DO_ALARM, (PressState() == PS_FAULT || !PressOnline()) ? 1 : 0);
        draw();
        msleep(50);
    }
    return 1;
}
