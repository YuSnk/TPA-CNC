#ifndef PRESS_H
#define PRESS_H

#include "types.h"

typedef enum {
    PS_INIT = 0,
    PS_IDLE,
    PS_ZERO_CLOSE,
    PS_ZERO_CLEAR,
    PS_ZERO_OPEN,
    PS_MOVE_CLOSE,
    PS_HOLD,
    PS_MOVE_OPEN,
    PS_FAULT
} press_state_t;

void PressInit(void);
void PressStart(void);
void PressTick(void);

press_state_t PressState(void);
const char   *PressStateName(void);
int32_t       PressPos(void);
int16_t       PressSpeed(void);
uint16_t      PressFault(void);
const char   *PressFaultText(void);
int           PressOnline(void);
int           PressZeroOk(void);

void PressCmdStop(void);
void PressCmdZero(void);
void PressCmdClose(void);
void PressCmdOpen(void);

#endif
