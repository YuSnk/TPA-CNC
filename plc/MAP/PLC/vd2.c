#include <satplc.h>
#include <satser.h>
#include <modbus.h>
#include <satscr.h>
#include <satmio.h>
#include "vd2.h"

int Handle;
static int16_t last_speed_cmd = 0x7FFF;

int Vd2Open(void)
{
    Handle = SerialOpen(1, B9600, PARNONE, 1);
    if (Handle <= 0) {
        return 0;
    }
    ModbusRTUTimeout = 200;
    last_speed_cmd = 0x7FFF;
    return 1;
}

mdbs_result_t Vd2Write(uint16_t addr, uint16_t value)
{
    mdbs_result_t res = MDBS_DEVICE_NOT_RESPOND;
    for (int i = 0; i < 3; i++) {
        res = ModbusWriteSingleRegister(Handle, MB_ADR_VD2, addr, value);
        if (res == MDBS_OK) {
            return res;
        }
        msleep(20);
    }
    return res;
}

mdbs_result_t Vd2Read(uint16_t addr, uint16_t cnt, uint16_t *reg)
{
    mdbs_result_t res = MDBS_DEVICE_NOT_RESPOND;
    for (int i = 0; i < 3; i++) {
        res = ModbusReadHoldingRegisters(Handle, MB_ADR_VD2, addr, cnt, reg);
        if (res == MDBS_OK) {
            return res;
        }
        msleep(20);
    }
    return res;
}

int Vd2ReadSpeed(int16_t *rpm)
{
    uint16_t r;
    if (Vd2Read(VD2_U0_02, 1, &r) != MDBS_OK) {
        return 0;
    }
    *rpm = (int16_t)r;
    return 1;
}

int Vd2ReadPos(int32_t *pos)
{
    uint16_t r[2];
    /* U0-13: 32 бит, старшее слово в 0x1E10 (P12-06=0) */
    if (Vd2Read(VD2_U0_13, 2, r) != MDBS_OK) {
        return 0;
    }
    *pos = ((int32_t)r[0] << 16) | (uint16_t)r[1];
    return 1;
}

int Vd2ReadFault(uint16_t *code)
{
    uint16_t r;
    if (Vd2Read(VD2_U1_01, 1, &r) != MDBS_OK) {
        return 0;
    }
    *code = r;
    return 1;
}

int Vd2SetSpeed(int16_t rpm)
{
    if (rpm == last_speed_cmd) {
        return 1;
    }
    if (Vd2Write(VD2_P01_02, (uint16_t)rpm) != MDBS_OK) {
        return 0;
    }
    last_speed_cmd = rpm;
    return 1;
}

void Vd2ForceSpeed(int16_t rpm)
{
    last_speed_cmd = 0x7FFF;
    (void)Vd2SetSpeed(rpm);
}

int Vd2SetAccel(unsigned int accel_ms, unsigned int decel_ms, unsigned int cruise_rpm)
{
    unsigned int p03;
    unsigned int p04;
    if (cruise_rpm == 0u) {
        cruise_rpm = 1u;
    }
    /* P01-03/04 — время 0↔1000 об/мин */
    p03 = (accel_ms * 1000u) / cruise_rpm;
    p04 = (decel_ms * 1000u) / cruise_rpm;
    if (p03 > 65535u) {
        p03 = 65535u;
    }
    if (p04 > 65535u) {
        p04 = 65535u;
    }
    if (Vd2Write(VD2_P01_03, (uint16_t)p03) != MDBS_OK) {
        return 0;
    }
    if (Vd2Write(VD2_P01_04, (uint16_t)p04) != MDBS_OK) {
        return 0;
    }
    return 1;
}

int Vd2SetTorquePct(unsigned int pct_x10)
{
    if (Vd2Write(VD2_P01_15, (uint16_t)pct_x10) != MDBS_OK) {
        return 0;
    }
    if (Vd2Write(VD2_P01_16, (uint16_t)pct_x10) != MDBS_OK) {
        return 0;
    }
    return 1;
}

int Vd2ServoOn(int on)
{
    return Vd2Write(VD2_P13_02, on ? 1u : 0u) == MDBS_OK;
}

int Vd2ClearFault(void)
{
    if (Vd2Write(VD2_P13_03, 1u) != MDBS_OK) {
        return 0;
    }
    msleep(50);
    if (Vd2Write(VD2_P13_03, 0u) != MDBS_OK) {
        return 0;
    }
    return 1;
}
