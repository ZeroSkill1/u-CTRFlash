#include <hardware/pio.h>

#ifndef _PINOUT_H
#define _PINOUT_H

// minimum required to flash 3DS MCU

extern PIO PINOUT_TOOL78_PIO;

#define PINOUT_TOOL78_RL78_TOOL0 22

#define PINOUT_TOOL78_RL78_TX 22
#define PINOUT_TOOL78_RL78_RX 22

#define PINOUT_TOOL78_78K0R_TOOL0 22
#define PINOUT_TOOL78_nRESET      20
#define PINOUT_TOOL78_78K0R_FLMD0 21

#endif