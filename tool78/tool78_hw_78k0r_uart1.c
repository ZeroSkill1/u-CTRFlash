
#include <hardware/gpio.h>
#include <hardware/pio.h>
#include <pico/time.h>
#include <stdio.h>

#include "pinout.h"

#include "tool78_defs.h"
#include "tool78_hw_helpers.h"

#include "tool78.pio.h"

#include "tool78_hw.h"

static struct tool78_pio_vars vars;

static void t78k0r_uart1_deinit(void);
static int t78k0r_uart1_recv(int len, uint8_t* data, int32_t timeout_us);
static int t78k0r_uart1_send(int len, const uint8_t* data, int32_t timeout_us);

static bool t78k0r_uart1_init(void) {
	if (!tool78_hw_init_help(&tool78_uart_tx_program, &tool78_uart_rx_program,
				&vars)) return false;

	//if (tool78_hw_78k0r_uart1.flags & tool78_hw_flag_do_ocd)
	//	return false; // OCD not supported with this phy

	tool78_hw_78k0r_uart1.target_detail = 0;
	vars.exclusive = true;
	vars.bitswap = true;

	if (tool78_hw_78k0r_uart1.flags & tool78_hw_flag_do_ocd) {
		gpio_pull_up(PINOUT_TOOL78_78K0R_TOOL0);
		gpio_put(PINOUT_TOOL78_nRESET, 1u);
		gpio_set_dir(PINOUT_TOOL78_nRESET, GPIO_OUT);
		gpio_set_dir(PINOUT_TOOL78_78K0R_FLMD0, GPIO_IN);
		gpio_set_function(PINOUT_TOOL78_nRESET, GPIO_FUNC_SIO);
		gpio_set_function(PINOUT_TOOL78_78K0R_FLMD0, GPIO_FUNC_SIO);

		do {
			busy_wait_ms(4);
		} while (gpio_get(PINOUT_TOOL78_78K0R_FLMD0) == 0);
	} else {
		tool78_entryseq_78k0r(tool78_entry_78k0r_uart1);
	}

	//gpio_set_function(PINOUT_TOOL78_78K0R_TOOL0, GPIO_FUNC_SIO); // also sets input/output enables properly
	tool78_uart_rx_program_init(PINOUT_TOOL78_PIO, vars.smrx, vars.rxoff,
			PINOUT_TOOL78_78K0R_TOOL0, 9600, true, true);
	tool78_uart_tx_program_init(PINOUT_TOOL78_PIO, vars.smtx, vars.txoff,
			PINOUT_TOOL78_78K0R_TOOL0, 9600, true);

	gpio_set_slew_rate(PINOUT_TOOL78_78K0R_TOOL0, GPIO_SLEW_RATE_FAST);
	gpio_set_drive_strength(PINOUT_TOOL78_78K0R_TOOL0, GPIO_DRIVE_STRENGTH_12MA);

	if (!(tool78_hw_78k0r_uart1.flags & tool78_hw_flag_do_ocd)) {
		// wait for 0x00 "READY" byte
		uint8_t byte = 0xff;
		size_t s = t78k0r_uart1_recv(1, &byte, 50*1000);
		if (s == 0 || byte != 0x00) {
			t78k0r_uart1_deinit();
			return false;
		}
		busy_wait_us_32(120 + 20); // t01 or tCOM

		// send two 0x00 "LOW" bytes
		byte = 0x00;
		t78k0r_uart1_send(1, &byte, -1);
		busy_wait_us_32(120); // t12
		t78k0r_uart1_send(1, &byte, -1);
		busy_wait_us_32(610); // t2C
	} else {
		//tool78_hw_78k0r_uart1.set_baudrate(115200);
	}

	tool78_hw_78k0r_uart1.flags |= tool78_hw_flag_done_reset;

	// now a reset command needs to be sent, but we leave that to the upper
	// (command processing) layer as it has to know about those timings anyway

	return true; // all is well!
}
static void t78k0r_uart1_deinit(void) {
	tool78_hw_deinit_help(&tool78_uart_tx_program, &tool78_uart_rx_program,
			&vars);

	tool78_deinit_78k0r();
}

static int t78k0r_uart1_has_available(void) {
	return tool78_hw_has_available_help(&vars);
}

static int t78k0r_uart1_recv(int len, uint8_t* data, int32_t timeout_us) {
	return tool78_hw_help_recv(&vars, len, data, timeout_us);
}
static int t78k0r_uart1_send(int len, const uint8_t* data, int32_t timeout_us) {
	return tool78_hw_help_send(&vars, 10/*inter-byte delay (us) */,
			len, data, timeout_us);
}

static void t78k0r_uart1_set_baudrate(uint32_t baudrate) {
	float div = (float)clock_get_hz(clk_sys) / (8*baudrate);

	pio_sm_set_enabled(PINOUT_TOOL78_PIO, vars.smrx, false);
	pio_sm_set_clkdiv(PINOUT_TOOL78_PIO, vars.smrx, div);
	pio_sm_restart(PINOUT_TOOL78_PIO, vars.smrx);
	pio_sm_exec(PINOUT_TOOL78_PIO, vars.smrx, pio_encode_jmp(vars.rxoff));
	pio_sm_set_enabled(PINOUT_TOOL78_PIO, vars.smrx, true);

	pio_sm_set_enabled(PINOUT_TOOL78_PIO, vars.smtx, false);
	pio_sm_set_clkdiv(PINOUT_TOOL78_PIO, vars.smtx, div);
	pio_sm_restart(PINOUT_TOOL78_PIO, vars.smtx);
	pio_sm_exec(PINOUT_TOOL78_PIO, vars.smtx, pio_encode_jmp(vars.txoff));
	pio_sm_set_enabled(PINOUT_TOOL78_PIO, vars.smtx, true);

	/*tool78_uart_rx_program_init(PINOUT_TOOL78_PIO, vars.smrx, vars.rxoff,
			PINOUT_TOOL78_78K0R_TOOL0, baudrate, true, true);
	tool78_uart_tx_program_init(PINOUT_TOOL78_PIO, vars.smtx, vars.txoff,
			PINOUT_TOOL78_78K0R_TOOL0, baudrate, true);*/
}

/*static void t78k0r_uart1_rx_set_stop_bit(bool enable) {
	tool78_rx_set_stop_bit(&vars, enable);
}*/

struct tool78_hw tool78_hw_78k0r_uart1 = {
	.target = tool78k0r_uart1,

	.init = t78k0r_uart1_init,
	.deinit = t78k0r_uart1_deinit,

	.set_baudrate = t78k0r_uart1_set_baudrate,
	.has_available = t78k0r_uart1_has_available,

	.recv = t78k0r_uart1_recv,
	.send = t78k0r_uart1_send,

	//.rx_set_stop_bit = t78k0r_uart1_rx_set_stop_bit
};

