#ifndef THERMO_H
#define THERMO_H

#include <satplc.h>
#include <modbus.h>
#include <stdbool.h>

#define ZONE_COUNT              4
#define HYSTERESIS_C            5.0f
#define SP_MIN_C                0.0f
#define SP_MAX_C                500.0f

#define MB_ADR_MV110            16u
#define MB_ADR_DOR13            1u

#define TCP_BASE                8192u
#define TCP_REG_COUNT           526u

/* Смещение относительно TCP_BASE (= абсолютный адрес − 8192) */
#define OFF_HEAT_EN             0u      /* 8192 INT16 */
#define OFF_SP1                 1u      /* 8193 FLOAT32 CDAB, далее +2 на зону */
#define OFF_T1                  9u      /* 8201 FLOAT32 CDAB */
#define OFF_ERR1                17u     /* 8209 INT16 */
#define OFF_RLY1                21u     /* 8213 INT16 */

#define ERR_NONE                0
#define ERR_SENSOR              1
#define ERR_COMM                2

#define EPROM_MAGIC_ADR         0u
#define EPROM_MAGIC             0xA55A
#define EPROM_EN_ADR            1u
#define EPROM_SP_ADR            2u      /* 4 ячейки: уставка ×10 */

extern int        g_heat_enable;
extern float      g_sp[ZONE_COUNT];
extern float      g_temp[ZONE_COUNT];
extern int        g_err[ZONE_COUNT];
extern int        g_relay[ZONE_COUNT];
extern uint16_t   TcpRegs[TCP_REG_COUNT];

int  ThermoInit(void);
void ThermoScan(void);
void ThermoCheckTcpWrites(void);
void ThermoDraw(void);

#endif
