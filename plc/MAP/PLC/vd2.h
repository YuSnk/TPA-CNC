#ifndef VD2_H
#define VD2_H

#include "types.h"
#include <modbus.h>

/* WECON VD2 / VD2F, адрес как в пульте TPA-CNC (P12-01) */
#define MB_ADR_VD2              1u

/* Px-yy → (group << 8) | index */
#define VD2_P00_01              0x0001u     /* режим: 2 = скорость */
#define VD2_P01_01              0x0101u     /* источник скорости: 0 = внутренний */
#define VD2_P01_02              0x0102u     /* задание скорости, об/мин */
#define VD2_P01_03              0x0103u     /* разгон, время 0→1000 об/мин, мс */
#define VD2_P01_04              0x0104u     /* торможение, время 0→1000 об/мин, мс */
#define VD2_P01_14              0x010Eu     /* источник момента: 0 = внутренний */
#define VD2_P01_15              0x010Fu     /* лимит момента +, 0.1% */
#define VD2_P01_16              0x0110u     /* лимит момента −, 0.1% */
#define VD2_P01_19              0x0113u     /* таймаут насыщения момента, мс */

#define VD2_P13_02              0x0D02u     /* VDI_1: Servo ON */
#define VD2_P13_03              0x0D03u     /* VDI_2: A-CLR сброс аварии */

#define VD2_U0_02               0x1E02u     /* скорость, об/мин */
#define VD2_U0_13               0x1E10u     /* позиция энкодера, 32 бит */
#define VD2_U1_01               0x1F01u     /* код аварии */

#define VD2_MODE_SPEED          2u
#define VD2_ER37_TORQUE_SAT     37u

extern int Handle;

int  Vd2Open(void);
mdbs_result_t Vd2Write(uint16_t addr, uint16_t value);
mdbs_result_t Vd2Read(uint16_t addr, uint16_t cnt, uint16_t *reg);

int  Vd2ReadSpeed(int16_t *rpm);
int  Vd2ReadPos(int32_t *pos);
int  Vd2ReadFault(uint16_t *code);

int  Vd2SetSpeed(int16_t rpm);
void Vd2ForceSpeed(int16_t rpm);
int  Vd2SetAccel(unsigned int accel_ms, unsigned int decel_ms, unsigned int cruise_rpm);
int  Vd2SetTorquePct(unsigned int pct_x10);
int  Vd2ServoOn(int on);
int  Vd2ClearFault(void);

#endif
