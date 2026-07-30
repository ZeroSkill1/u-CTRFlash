
#ifndef TOOL78_HW_H_
#define TOOL78_HW_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "tool78_defs.h"

enum tool78_hw_flags {
	tool78_hw_flag_done_reset = 1<<0,
	tool78_hw_flag_do_ocd     = 1<<1,
};

struct tool78_hw {
	const enum tool78_target target;
	enum tool78_target_detail target_detail;
	enum tool78_hw_flags flags;

	// performs entry sequence + initial handshake (RESET or similar stuff, eg.
	// UART needs extra null bytes in the beginning)
	bool (*init)(void);
	void (*deinit)(void);

	void (*set_baudrate)(uint32_t rate);
	// returns -1 if an rx overrun occurred
	int (*has_available)(void);

	// timeout:
	// * infinitely blocking: < 0
	// * not blocking at all: = 0
	// retval: negative if an rx overrun occurred
	int (*recv)(int len, uint8_t* data, int32_t timeout_ms);
	int (*send)(int len, const uint8_t* data, int32_t timeout_ms);

	void (*rx_set_stop_bit)(bool enable);
};

extern struct tool78_hw
	tool78_hw_test_uart2, // for testing DMA buffer logic & PIO stuff

	tool78_hw_78k0_uart2,
	tool78_hw_78k0_uart2_extclk,
	tool78_hw_78k0_spi,

	tool78_hw_78k0r_uart1,

	tool78_hw_rl78_uart1,
	tool78_hw_rl78_uart2,

	tool78_hw_rl78g10_uart1;

#endif

