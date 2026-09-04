#include <satplc.h>
#include "vd2.h"
#include "press.h"

/* Каноника tpa-pilot: docs/modbus-press-saturn.md
 * Скоростной режим P00-01=2, трапецию считает мастер, конец — по энкодеру.
 * Не ждать COIN. */

#define CLOSE_DIR               1               /* направление смыкания */
#define IMP_PER_MM              1000            /* 5000 имп/об / 5 мм/об */
#define OPEN_MM                 200             /* отвод на старт, мм */
#define APPROACH_MM             8
#define APPROACH_RPM            12
#define CLOSE_RPM               40
#define OPEN_RPM                100
#define ACCEL_MS                500
#define DECEL_MS                800
#define DONE_IMP                8
#define DONE_RPM                5
#define HOLD_MS                 300
#define TORQUE_WORK             800             /* 80.0% */
#define TORQUE_HOME             80              /* 8.0% */
#define SAT_TIMEOUT_MS          200
#define ZERO_HOME_RPM           12

static press_state_t state;
static int32_t pos;
static int16_t speed;
static uint16_t fault;
static int online;
static int zero_ok;
static int32_t pos_closed;
static int32_t pos_open;
static int32_t move_dest;
static unsigned int move_cruise;
static uint32_t hold_t0;
static uint32_t clear_t0;
static const char *fault_text;

static int32_t iabs32(int32_t v)
{
    return (v < 0) ? -v : v;
}

static int16_t iabs16(int16_t v)
{
    return (v < 0) ? (int16_t)-v : v;
}

static int poll_drive(void)
{
    int16_t rpm;
    int32_t p;
    uint16_t f;
    if (!Vd2ReadSpeed(&rpm) || !Vd2ReadPos(&p) || !Vd2ReadFault(&f)) {
        online = 0;
        return 0;
    }
    speed = rpm;
    pos = p;
    fault = f;
    online = 1;
    return 1;
}

static int setup_speed_mode(void)
{
    if (Vd2Write(VD2_P00_01, VD2_MODE_SPEED) != MDBS_OK) {
        return 0;
    }
    if (Vd2Write(VD2_P01_01, 0) != MDBS_OK) {
        return 0;
    }
    if (Vd2Write(VD2_P01_14, 0) != MDBS_OK) {
        return 0;
    }
    if (Vd2Write(VD2_P01_19, SAT_TIMEOUT_MS) != MDBS_OK) {
        return 0;
    }
    return 1;
}

static int16_t trapezoid_cmd(int32_t dest, unsigned int cruise_rpm)
{
    int32_t rem = dest - pos;
    int sign = (rem >= 0) ? 1 : -1;
    int32_t rem_abs = iabs32(rem);
    int32_t approach_imp = (int32_t)APPROACH_MM * IMP_PER_MM;
    unsigned int rpm;

    if (rem_abs <= approach_imp) {
        rpm = APPROACH_RPM;
    } else {
        rpm = cruise_rpm;
    }
    return (int16_t)(sign * (int)rpm);
}

static int move_done(int32_t dest)
{
    return (iabs32(dest - pos) < DONE_IMP) && (iabs16(speed) < DONE_RPM);
}

static void finish_move(void)
{
    Vd2ForceSpeed(0);
    (void)Vd2Write(VD2_P01_03, 200);
    (void)Vd2Write(VD2_P01_04, 200);
}

static void enter_fault(const char *msg)
{
    Vd2ForceSpeed(0);
    (void)Vd2ServoOn(0);
    fault_text = msg;
    state = PS_FAULT;
}

static int begin_move(int32_t dest, unsigned int cruise_rpm)
{
    move_dest = dest;
    move_cruise = cruise_rpm;
    if (!Vd2SetTorquePct(TORQUE_WORK)) {
        return 0;
    }
    if (!Vd2SetAccel(ACCEL_MS, DECEL_MS, cruise_rpm)) {
        return 0;
    }
    Vd2ForceSpeed(0);
    return 1;
}

void PressInit(void)
{
    state = PS_INIT;
    pos = 0;
    speed = 0;
    fault = 0;
    online = 0;
    zero_ok = 0;
    pos_closed = 0;
    pos_open = 0;
    fault_text = "";
}

press_state_t PressState(void) { return state; }
int32_t       PressPos(void) { return pos; }
int16_t       PressSpeed(void) { return speed; }
uint16_t      PressFault(void) { return fault; }
const char   *PressFaultText(void) { return fault_text; }
int           PressOnline(void) { return online; }
int           PressZeroOk(void) { return zero_ok; }

const char *PressStateName(void)
{
    switch (state) {
    case PS_INIT:        return "INIT";
    case PS_IDLE:        return "IDLE";
    case PS_ZERO_CLOSE:  return "ZERO CLOSE";
    case PS_ZERO_CLEAR:  return "ZERO CLR";
    case PS_ZERO_OPEN:   return "ZERO OPEN";
    case PS_MOVE_CLOSE:  return "CLOSE";
    case PS_HOLD:        return "HOLD";
    case PS_MOVE_OPEN:   return "OPEN";
    case PS_FAULT:       return "FAULT";
    default:             return "?";
    }
}

void PressCmdStop(void)
{
    Vd2ForceSpeed(0);
    if (state != PS_INIT) {
        state = PS_IDLE;
    }
}

void PressCmdZero(void)
{
    if (state != PS_IDLE && state != PS_FAULT) {
        return;
    }
    if (!setup_speed_mode() || !Vd2SetTorquePct(TORQUE_HOME)) {
        enter_fault("setup home");
        return;
    }
    if (!Vd2SetAccel(ACCEL_MS, DECEL_MS, ZERO_HOME_RPM)) {
        enter_fault("accel home");
        return;
    }
    if (!Vd2ServoOn(1)) {
        enter_fault("SON");
        return;
    }
    zero_ok = 0;
    Vd2ForceSpeed((int16_t)(CLOSE_DIR * ZERO_HOME_RPM));
    state = PS_ZERO_CLOSE;
}

void PressCmdClose(void)
{
    if (state != PS_IDLE || !zero_ok) {
        return;
    }
    if (!begin_move(pos_closed, CLOSE_RPM)) {
        enter_fault("setup close");
        return;
    }
    state = PS_MOVE_CLOSE;
}

void PressCmdOpen(void)
{
    if (state != PS_IDLE || !zero_ok) {
        return;
    }
    if (!begin_move(pos_open, OPEN_RPM)) {
        enter_fault("setup open");
        return;
    }
    state = PS_MOVE_OPEN;
}

void PressTick(void)
{
    if (state == PS_INIT) {
        return;
    }
    if (!poll_drive()) {
        if (state != PS_FAULT) {
            enter_fault("modbus");
        }
        return;
    }

    switch (state) {
    case PS_IDLE:
        break;

    case PS_ZERO_CLOSE:
        if (fault == VD2_ER37_TORQUE_SAT) {
            Vd2ForceSpeed(0);
            (void)Vd2ClearFault();
            clear_t0 = GetTickCount();
            state = PS_ZERO_CLEAR;
        } else if (fault != 0) {
            enter_fault("fault home");
        }
        break;

    case PS_ZERO_CLEAR:
        if ((GetTickCount() - clear_t0) < 200u) {
            break;
        }
        if (fault != 0) {
            (void)Vd2ClearFault();
            clear_t0 = GetTickCount();
            break;
        }
        pos_closed = pos;
        pos_open = pos_closed - (int32_t)CLOSE_DIR * OPEN_MM * IMP_PER_MM;
        if (!begin_move(pos_open, OPEN_RPM)) {
            enter_fault("setup retract");
            break;
        }
        state = PS_ZERO_OPEN;
        break;

    case PS_ZERO_OPEN:
        if (fault != 0 && fault != VD2_ER37_TORQUE_SAT) {
            enter_fault("fault retract");
            break;
        }
        (void)Vd2SetSpeed(trapezoid_cmd(move_dest, move_cruise));
        if (move_done(move_dest)) {
            finish_move();
            zero_ok = 1;
            state = PS_IDLE;
        }
        break;

    case PS_MOVE_CLOSE:
    case PS_MOVE_OPEN:
        if (fault != 0) {
            enter_fault("fault move");
            break;
        }
        (void)Vd2SetSpeed(trapezoid_cmd(move_dest, move_cruise));
        if (move_done(move_dest)) {
            finish_move();
            if (state == PS_MOVE_CLOSE) {
                hold_t0 = GetTickCount();
                state = PS_HOLD;
            } else {
                state = PS_IDLE;
            }
        }
        break;

    case PS_HOLD:
        if ((GetTickCount() - hold_t0) >= HOLD_MS) {
            if (!begin_move(pos_open, OPEN_RPM)) {
                enter_fault("setup open");
                break;
            }
            state = PS_MOVE_OPEN;
        }
        break;

    case PS_FAULT:
        break;

    default:
        break;
    }
}

void PressStart(void)
{
    if (!setup_speed_mode()) {
        enter_fault("P00-01");
        return;
    }
    (void)Vd2ServoOn(1);
    state = PS_IDLE;
}
