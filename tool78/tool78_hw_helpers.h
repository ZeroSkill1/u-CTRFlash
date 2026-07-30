
#ifndef TOOL78_HW_HELPERS_H_
#define TOOL78_HW_HELPERS_H_

#include <stdint.h>
#include <stdbool.h>

#include <hardware/pio.h>

#include "tool78_defs.h"

struct tool78_pio_vars {
	uint txoff, rxoff;
	int smtx, smrx;
	// temporarily disable rx when doing tx (typically desired when doing
	// single-wire UART, as otherwise the data would be echoing back)
	bool exclusive, bitswap/*, nostopbit*/;
};

// claims and inits program space & state machines, saves the offsets & SMs
// in a struct. returns true if successful.
// still needs to be done: initing & enabling the PIO SMs
bool tool78_hw_init_help(const pio_program_t* prgm_tx,
		const pio_program_t* prgm_rx, struct tool78_pio_vars* vars);

// disables the SM, deallocates the program & SM
// still nees to be done: changing the SM pin functions to something/NULL
void tool78_hw_deinit_help(const pio_program_t* prgm_tx,
		const pio_program_t* prgm_rx, const struct tool78_pio_vars* vars);

bool tool78_hw_help_check_overrun(const struct tool78_pio_vars* vars);

int tool78_hw_has_available_help(const struct tool78_pio_vars* vars);
int tool78_hw_help_recv(const struct tool78_pio_vars* vars,
		int len, uint8_t* data, int32_t timeout_us);
int tool78_hw_help_send(const struct tool78_pio_vars* vars, uint32_t sleep_us_between_bytes,
		int len, const uint8_t* data, int32_t timeout_us);

// performs an entry sequence using SIO
void tool78_entryseq_78k0(enum tool78_entry typ);
// pulls nRESET low & puts other stuff in hi-Z
void tool78_deinit_78k0(void);

void tool78_entryseq_78k0r(enum tool78_entry typ);
void tool78_deinit_78k0r(void);

void tool78_entryseq_rl78(enum tool78_entry typ);
void tool78_deinit_rl78(void);

/*inline static void tool78_rx_set_stop_bit(struct tool78_pio_vars* vars,
		bool enable) {
	vars->nostopbit = !enable;
	if (enable) {
		tool78_uart_rx_enable_stop(PINOUT_TOOL78_PIO, vars->rxoff);
	} else {
		tool78_uart_rx_disable_stop(PINOUT_TOOL78_PIO, vars->rxoff);
	}
}*/

#endif

