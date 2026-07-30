
#include <hardware/gpio.h>
#include <hardware/pio.h>
#include <pico/time.h>

#include "pinout.h"

#include "tool78_defs.h"
#include "tool78_hw_helpers.h"

#include "tool78.pio.h"

#include "tool78_hw.h"

static struct tool78_pio_vars vars;

static int trl78_uart1_send(int len, const uint8_t* data, int32_t timeout_us);
static int trl78g10_uart1_send(int len, const uint8_t* data, int32_t timeout_us);

static bool trl78_uart1_init(void) {
	if (!tool78_hw_init_help(&tool78_uart_tx_program, &tool78_uart_rx_program,
				&vars)) return false;

	tool78_hw_rl78_uart1.target_detail = 0;
	vars.exclusive = true;
	vars.bitswap = true;

	enum tool78_entry typ = (tool78_hw_rl78_uart1.flags & tool78_hw_flag_do_ocd)
			? tool78_entry_rl78_ocd : tool78_entry_rl78_uart1;

	tool78_entryseq_rl78(typ);

	tool78_uart_rx_program_init(PINOUT_TOOL78_PIO, vars.smrx, vars.rxoff,
			PINOUT_TOOL78_RL78_TOOL0, 115200, true, true);
	tool78_uart_tx_program_init(PINOUT_TOOL78_PIO, vars.smtx, vars.txoff,
			PINOUT_TOOL78_RL78_TOOL0, 115200, true);

	uint8_t byte = (uint8_t)typ;
	trl78_uart1_send(1, &byte, -1);

	//busy_wait_us_32(70); // tMB
	busy_wait_ms(4);

	// now a baudrate set command needs to be sent, but we leave that to the
	// upper (command processing) layer as it has to know about those timings
	// anyway

	return true; // all is well!
}
static bool trl78g10_uart1_init(void) {
	if (!tool78_hw_init_help(&tool78_uart_tx_program, &tool78_uart_rx_program,
				&vars)) return false;

	tool78_hw_rl78_uart1.target_detail = 0;
	vars.exclusive = true;
	vars.bitswap = true;

	//enum tool78_entry typ = tool78_entry_rl78_uart1;
	enum tool78_entry typ = (tool78_hw_rl78g10_uart1.flags & tool78_hw_flag_do_ocd)
			? tool78_entry_rl78_ocd : tool78_entry_rl78_uart1;

	tool78_entryseq_rl78(typ);

	tool78_uart_rx_program_init(PINOUT_TOOL78_PIO, vars.smrx, vars.rxoff,
			PINOUT_TOOL78_RL78_TOOL0, 115200, true, false);
	tool78_uart_tx_program_init(PINOUT_TOOL78_PIO, vars.smtx, vars.txoff,
			PINOUT_TOOL78_RL78_TOOL0, 115200, true);

	uint8_t byte = (uint8_t)typ;
	trl78g10_uart1_send(1, &byte, -1);

	//busy_wait_ms(4);

	// the device will reply with 0x06

	return true; // all is well!
}
static void trl78_uart1_deinit(void) {
	tool78_hw_deinit_help(&tool78_uart_tx_program, &tool78_uart_rx_program,
			&vars);

	tool78_deinit_rl78();
}

static int trl78_uart1_has_available(void) {
	return tool78_hw_has_available_help(&vars);
}

static int trl78_uart1_recv(int len, uint8_t* data, int32_t timeout_us) {
	return tool78_hw_help_recv(&vars, len, data, timeout_us);
}
static int trl78_uart1_send(int len, const uint8_t* data, int32_t timeout_us) {
	return tool78_hw_help_send(&vars,
			(tool78_hw_rl78_uart1.flags & tool78_hw_flag_done_reset) ? 0 : 190
			/*inter-byte delay (us) tDR = 90/8M s */,
			len, data, timeout_us);
}
static int trl78g10_uart1_send(int len, const uint8_t* data, int32_t timeout_us) {
	return tool78_hw_help_send(&vars,
			(tool78_hw_rl78g10_uart1.flags & tool78_hw_flag_done_reset) ? 0 : 190
			/*inter-byte delay (us) tDR = 90/8M s */,
			len, data, timeout_us);
}

static void trl78_uart1_set_baudrate(uint32_t baudrate) {
	float div = (float)clock_get_hz(clk_sys) / (8*baudrate);
	pio_sm_set_clkdiv(PINOUT_TOOL78_PIO, vars.smrx, div);
	pio_sm_set_clkdiv(PINOUT_TOOL78_PIO, vars.smtx, div);
}

/*static void trl78_uart1_rx_set_stop_bit(bool enable) {
	tool78_rx_set_stop_bit(&vars, enable);
}

void trl78_uart1_set_exclusive(bool ex) {
	(void)ex;
	//vars.exclusive = ex;
}*/

struct tool78_hw tool78_hw_rl78_uart1 = {
	.target = tool78rl_uart1,

	.init = trl78_uart1_init,
	.deinit = trl78_uart1_deinit,

	.set_baudrate = trl78_uart1_set_baudrate,
	.has_available = trl78_uart1_has_available,

	.recv = trl78_uart1_recv,
	.send = trl78_uart1_send,

	//.rx_set_stop_bit = trl78_uart1_rx_set_stop_bit
};
struct tool78_hw tool78_hw_rl78g10_uart1 = {
	.target = tool78rlg10_uart1,

	.init = trl78g10_uart1_init,
	.deinit = trl78_uart1_deinit,

	.set_baudrate = trl78_uart1_set_baudrate,
	.has_available = trl78_uart1_has_available,

	.recv = trl78_uart1_recv,
	.send = trl78g10_uart1_send,

	//.rx_set_stop_bit = trl78_uart1_rx_set_stop_bit
};

