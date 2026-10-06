#include "thermo.h"
#include <satser.h>
#include <satmio.h>
#include <satscr.h>
#include <satmem.h>
#include <satmenu.h>
#include <string.h>

int        g_heat_enable;
float      g_sp[ZONE_COUNT];
float      g_temp[ZONE_COUNT];
int        g_err[ZONE_COUNT];
int        g_relay[ZONE_COUNT];
uint16_t   TcpRegs[TCP_REG_COUNT];

static int Handle;
static unsigned int tcp_wr_count;
static int last_menu_en;
static float last_menu_sp[ZONE_COUNT];

/* МВ110: температура ×10 и ошибка канала, INT16 */
static const uint16_t mv_t_addr[ZONE_COUNT] = { 1u, 7u, 13u, 19u };
/* МР-DOR13: DO6..DO9, функция 05 */
static const uint16_t dor_coil[ZONE_COUNT] = { 261u, 262u, 263u, 264u };
static const char *const zone_title[ZONE_COUNT] = {
    "1 сопло", "2 зона", "3 зона", "4 зона"
};

static float clampf(float v, float lo, float hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static int clamp_en(int v)
{
    return (v != 0) ? 1 : 0;
}

static void publish_sp(void)
{
    TcpRegs[OFF_HEAT_EN] = (uint16_t)g_heat_enable;
    for (int i = 0; i < ZONE_COUNT; i++) {
        ModbusSetFloatCDAB(g_sp[i], &TcpRegs[OFF_SP1 + (unsigned)i * 2u]);
    }
}

static void publish_process(void)
{
    for (int i = 0; i < ZONE_COUNT; i++) {
        ModbusSetFloatCDAB(g_temp[i], &TcpRegs[OFF_T1 + (unsigned)i * 2u]);
        TcpRegs[OFF_ERR1 + (unsigned)i] = (uint16_t)g_err[i];
        TcpRegs[OFF_RLY1 + (unsigned)i] = (uint16_t)g_relay[i];
    }
}

static void save_eprom(void)
{
    WriteEprom(EPROM_MAGIC_ADR, EPROM_MAGIC);
    WriteEprom(EPROM_EN_ADR, (uint16_t)g_heat_enable);
    for (int i = 0; i < ZONE_COUNT; i++) {
        int x10 = (int)(g_sp[i] * 10.0f + (g_sp[i] >= 0.0f ? 0.5f : -0.5f));
        if (x10 < 0) {
            x10 = 0;
        }
        if (x10 > 5000) {
            x10 = 5000;
        }
        WriteEprom((uint16_t)(EPROM_SP_ADR + i), (uint16_t)x10);
    }
}

static void load_eprom(void)
{
    if (ReadEprom(EPROM_MAGIC_ADR) != EPROM_MAGIC) {
        g_heat_enable = 0;
        for (int i = 0; i < ZONE_COUNT; i++) {
            g_sp[i] = 0.0f;
        }
        save_eprom();
        return;
    }
    g_heat_enable = clamp_en((int)ReadEprom(EPROM_EN_ADR));
    for (int i = 0; i < ZONE_COUNT; i++) {
        g_sp[i] = clampf((float)ReadEprom((uint16_t)(EPROM_SP_ADR + i)) / 10.0f,
                         SP_MIN_C, SP_MAX_C);
    }
}

static void remember_menu(void)
{
    last_menu_en = g_heat_enable;
    for (int i = 0; i < ZONE_COUNT; i++) {
        last_menu_sp[i] = g_sp[i];
    }
}

static int menu_changed(void)
{
    if (last_menu_en != g_heat_enable) {
        return 1;
    }
    for (int i = 0; i < ZONE_COUNT; i++) {
        if (last_menu_sp[i] != g_sp[i]) {
            return 1;
        }
    }
    return 0;
}

static mdbs_result_t mb_read(uint8_t adr, uint16_t start, uint16_t cnt, uint16_t *reg)
{
    return ModbusReadHoldingRegisters(Handle, adr, start, cnt, reg);
}

static mdbs_result_t mb_coil(uint16_t coil, int on)
{
    return ModbusWriteSingleCoil(Handle, MB_ADR_DOR13, coil, on ? 1u : 0u);
}

static void read_zone(int z)
{
    uint16_t r[2];
    mdbs_result_t res;

    msleep(100);
    res = mb_read(MB_ADR_MV110, mv_t_addr[z], 2u, r);
    if (res != MDBS_OK) {
        g_temp[z] = 0.0f;
        g_err[z] = ERR_COMM;
        return;
    }
    /* INT16, после библиотеки — host endian; ×10 °C */
    g_temp[z] = (float)((int16_t)r[0]) / 10.0f;
    if (r[1] != 0) {
        g_err[z] = ERR_SENSOR;
    } else {
        g_err[z] = ERR_NONE;
    }
}

static void apply_heat(void)
{
    for (int i = 0; i < ZONE_COUNT; i++) {
        int on = 0;
        if (g_heat_enable && g_err[i] == ERR_NONE) {
            if (g_temp[i] < (g_sp[i] - HYSTERESIS_C)) {
                on = 1;
            } else if (g_temp[i] >= g_sp[i]) {
                on = 0;
            } else {
                on = g_relay[i];
            }
        }
        if (on != g_relay[i]) {
            msleep(100);
            if (mb_coil(dor_coil[i], on) != MDBS_OK) {
                on = 0;
            }
        }
        g_relay[i] = on;
    }
}

int ThermoInit(void)
{
    memset(TcpRegs, 0, sizeof(TcpRegs));
    memset(g_temp, 0, sizeof(g_temp));
    memset(g_err, 0, sizeof(g_err));
    memset(g_relay, 0, sizeof(g_relay));

    load_eprom();
    publish_sp();
    publish_process();

    Handle = SerialOpen(1, B9600, PARNONE, 1);
    if (Handle <= 0) {
        return 0;
    }
    ModbusRTUTimeout = 100;

    SetModbusServerStartRegAddress(TCP_BASE, TCP_BASE);
    if (!StartModbusTCPServer(TcpRegs, TCP_REG_COUNT, TcpRegs, TCP_REG_COUNT)) {
        return 0;
    }
    tcp_wr_count = ModbusTCPRegWriteCount();

    AddSystemMenuBoolItem("Нагрев шнека", &g_heat_enable);
    AddSystemMenuFloatItem("Зона 1 сопло", &g_sp[0], SP_MIN_C, SP_MAX_C, 1);
    AddSystemMenuFloatItem("Зона 2", &g_sp[1], SP_MIN_C, SP_MAX_C, 1);
    AddSystemMenuFloatItem("Зона 3", &g_sp[2], SP_MIN_C, SP_MAX_C, 1);
    AddSystemMenuFloatItem("Зона 4", &g_sp[3], SP_MIN_C, SP_MAX_C, 1);
    remember_menu();
    return 1;
}

void ThermoCheckTcpWrites(void)
{
    unsigned int n = ModbusTCPRegWriteCount();
    int changed = 0;

    if (n != tcp_wr_count) {
        float sp;
        tcp_wr_count = n;
        g_heat_enable = clamp_en((int)TcpRegs[OFF_HEAT_EN]);
        TcpRegs[OFF_HEAT_EN] = (uint16_t)g_heat_enable;
        for (int i = 0; i < ZONE_COUNT; i++) {
            sp = clampf(ModbusGetFloatCDAB(&TcpRegs[OFF_SP1 + (unsigned)i * 2u]),
                        SP_MIN_C, SP_MAX_C);
            g_sp[i] = sp;
            ModbusSetFloatCDAB(sp, &TcpRegs[OFF_SP1 + (unsigned)i * 2u]);
        }
        changed = 1;
    }

    g_heat_enable = clamp_en(g_heat_enable);
    for (int i = 0; i < ZONE_COUNT; i++) {
        g_sp[i] = clampf(g_sp[i], SP_MIN_C, SP_MAX_C);
    }
    if (menu_changed()) {
        changed = 1;
        publish_sp();
    }
    if (changed) {
        save_eprom();
        remember_menu();
    }
}

void ThermoScan(void)
{
    for (int i = 0; i < ZONE_COUNT; i++) {
        read_zone(i);
    }
    apply_heat();
    publish_process();
}

void ThermoDraw(void)
{
    const char *en;

    if (SystemMenuVisible()) {
        return;
    }
    en = g_heat_enable ? "ВКЛ" : "ВЫКЛ";
    ClrScr();
    GotoXY(0, 0);
    printf(HWHT "TPA-TERMO  %s\n", en);
    puts(HBLU "# SP    T     R  E");
    for (int i = 0; i < ZONE_COUNT; i++) {
        const char *col = g_err[i] ? HRED : (g_relay[i] ? HYEL : HGRN);
        printf("%s%s %5.1f %5.1f %s %d\n",
               col,
               zone_title[i],
               (double)g_sp[i],
               (double)g_temp[i],
               g_relay[i] ? "ON " : "off",
               g_err[i]);
    }
    ShowScr();
}
