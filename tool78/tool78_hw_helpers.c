
#include <string.h>

#include <hardware/gpio.h>
#include <hardware/pio.h>
#include <pico/time.h>
#include <pico/timeout_helper.h>

#include "pinout.h"

#include "tool78_defs.h"
#include "tool78_hw.h"

#include "tool78_hw_helpers.h"

static uint8_t bitswap(uint8_t in) {
	static const uint8_t lut[16] = {
		0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
		0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf
	};

	return (lut[in&0xf] << 4) | lut[in>>4];
}

bool tool78_hw_init_help(const pio_program_t* prgm_tx,
		const pio_program_t* prgm_rx, struct tool78_pio_vars* vars) {
	uint txoff = ~(uint)0, rxoff = ~(uint)0;
	int smtx = -1, smrx = -1;

	if (!prgm_tx && !prgm_rx) return false;

	if (prgm_rx) {
		if (!pio_can_add_program(PINOUT_TOOL78_PIO, prgm_rx))
			goto error;
		rxoff = pio_add_program(PINOUT_TOOL78_PIO, prgm_rx);
	}
	if (prgm_tx) {
		if (!pio_can_add_program(PINOUT_TOOL78_PIO, prgm_tx))
			goto error;
		txoff = pio_add_program(PINOUT_TOOL78_PIO, prgm_tx);
	}

	if (prgm_tx) {
		smtx = pio_claim_unused_sm(PINOUT_TOOL78_PIO, false);
		if (smtx == -1) goto error;
	}
	if (prgm_rx) {
		smrx = pio_claim_unused_sm(PINOUT_TOOL78_PIO, false);
		if (smrx == -1) goto error;
	}

	vars->txoff = txoff;
	vars->rxoff = rxoff;
	vars->smtx = smtx;
	vars->smrx = smrx;

	return true; // all is well!

error:
	if (smtx >= 0) pio_sm_unclaim(PINOUT_TOOL78_PIO, smtx);
	if (smrx >= 0) pio_sm_unclaim(PINOUT_TOOL78_PIO, smrx);
	if (!~txoff) pio_remove_program(PINOUT_TOOL78_PIO, prgm_tx, txoff);
	if (!~rxoff) pio_remove_program(PINOUT_TOOL78_PIO, prgm_rx, rxoff);
	return false;
}
void tool78_hw_deinit_help(const pio_program_t* prgm_tx,
		const pio_program_t* prgm_rx, const struct tool78_pio_vars* vars) {
	if (vars->smrx >= 0) {
		pio_sm_set_enabled(PINOUT_TOOL78_PIO, vars->smrx, false);
		pio_sm_unclaim(PINOUT_TOOL78_PIO, vars->smrx);
	}
	if (vars->smtx >= 0) {
		pio_sm_set_enabled(PINOUT_TOOL78_PIO, vars->smtx, false);
		pio_sm_unclaim(PINOUT_TOOL78_PIO, vars->smtx);
	}
	if (~vars->rxoff) pio_remove_program(PINOUT_TOOL78_PIO, prgm_rx, vars->rxoff);
	if (~vars->txoff) pio_remove_program(PINOUT_TOOL78_PIO, prgm_tx, vars->txoff);
}

bool tool78_hw_help_check_overrun(const struct tool78_pio_vars* vars) {
	if (vars->smrx < 0 || !~vars->rxoff) return false;

	// check if an RX overrun happened. if so, reset some stuff
	uint rxstall_flg = ((1u << vars->smrx) << PIO_FDEBUG_RXSTALL_LSB);
	if (PINOUT_TOOL78_PIO->fdebug & rxstall_flg) {
		pio_sm_set_enabled(PINOUT_TOOL78_PIO, vars->smrx, false);
		pio_sm_clear_fifos(PINOUT_TOOL78_PIO, vars->smrx);
		PINOUT_TOOL78_PIO->fdebug = rxstall_flg;
		pio_sm_restart(PINOUT_TOOL78_PIO, vars->smrx);
		pio_sm_exec(PINOUT_TOOL78_PIO, vars->smrx, pio_encode_jmp(vars->rxoff));
		pio_sm_set_enabled(PINOUT_TOOL78_PIO, vars->smrx, true);

		//printf("rxover!\n");
		return true;
	}

	return false;
}

int tool78_hw_has_available_help(const struct tool78_pio_vars* vars) {
	if (tool78_hw_help_check_overrun(vars)) return -1;

	return pio_sm_get_rx_fifo_level(PINOUT_TOOL78_PIO, vars->smrx);
}

int tool78_hw_help_recv(const struct tool78_pio_vars* vars,
		int len, uint8_t* data, int32_t timeout_us) {
	pio_sm_set_enabled(PINOUT_TOOL78_PIO, vars->smrx, true);

	bool overrun = tool78_hw_help_check_overrun(vars);

	bool blockinf = timeout_us < 0;
	int i = 0;
	uint8_t irq;

	absolute_time_t at = make_timeout_time_us(timeout_us);
	timeout_state_t ts = {0};
	check_timeout_fn ct = NULL;
	if (!blockinf) ct = init_single_timeout_until(&ts, at);

	for (; i < len /*&& ((!pio_sm_is_rx_fifo_empty(PINOUT_TOOL78_PIO, vars->smrx) || !ct(&ts))
				|| blockinf)*/; ++i) {
		while (pio_sm_is_rx_fifo_empty(PINOUT_TOOL78_PIO, vars->smrx)) {
			if (!blockinf && ct(&ts, false)) goto end; // whoops, timeout

			// TODO: sleep a bit? idk
		}

		uint8_t v = *(volatile uint8_t*)&PINOUT_TOOL78_PIO->rxf[vars->smrx];
		if (vars->bitswap) v = bitswap(v);
		//printf("[recv] got %02x\n", v);
		if (data) {
			data[i] = v;
		} else {
			(void)v;
		}
	}

end:
	irq = PINOUT_TOOL78_PIO->irq;
	PINOUT_TOOL78_PIO->irq = irq;
	/*if (irq) {
		gpio_put(3, true);
		printf("irq: 0x%02x\n", irq);
	} else gpio_put(3, false);
	irq = PINOUT_TOOL78_PIO->intr;
	if (irq&0xf00) {
		printf("intr: 0x%02x\n", irq);
	}*/

	//printf("[recv] %soverrun\n", overrun?"":"no ");
	return overrun ? (!i ? (int)0x08000000 : -i) : i;
}

int tool78_hw_help_send(const struct tool78_pio_vars* vars, uint32_t sleep_us_between_bytes,
		int len, const uint8_t* data, int32_t timeout_us) {

	if (vars->exclusive) {
		//gpio_put(3, false);
		//pio_sm_set_enabled(PINOUT_TOOL78_PIO, vars->smrx, false);
	}

	bool blockinf = timeout_us < 0;
	absolute_time_t at = make_timeout_time_us(timeout_us);
	timeout_state_t ts = {0};
	check_timeout_fn ct = NULL;
	if (!blockinf) ct = init_single_timeout_until(&ts, at);

	/*size_t nyeeted=0;
	uint8_t yeets[100];
	memset(yeets, 0, sizeof yeets);*/


	int i = 0;
	for (; i < len; ++i) {
		while (pio_sm_is_tx_fifo_full(PINOUT_TOOL78_PIO, vars->smtx)) {
			if (!blockinf && ct(&ts, false)) goto end; // whoops, timeout
		}

		*(volatile uint8_t*)&PINOUT_TOOL78_PIO->txf[vars->smtx] =
			vars->bitswap ? bitswap(data[i]) : data[i];

		// FIXME: THIS IS NOT HOW TO WAIT BETWEEN TWO BYTES YOU DOOFUS
		// (ok maybe it might work because of bad reasons, so let's fix this
		// once it causes problems)
		// FIXME: also remove the dependency on tool78_hw_rl78_uart1!
		// NOTE 2 much later: actually no the 78K0 needs this...
		if (sleep_us_between_bytes && ((i != len-1
						/*&& !(tool78_hw_rl78_uart1.flags & tool78_hw_flag_do_ocd)*/)
					|| !(tool78_hw_rl78_uart1.flags & tool78_hw_flag_done_reset))) {
			busy_wait_us_32(sleep_us_between_bytes);
		}

		if (vars->exclusive) {
			//if (nyeeted < sizeof yeets) {
				while (pio_sm_is_rx_fifo_empty(PINOUT_TOOL78_PIO, vars->smrx))
					;
				char x = *(volatile uint8_t*)&PINOUT_TOOL78_PIO->rxf[vars->smrx];
				if (vars->bitswap) x = bitswap(x);
				//printf("[send] echoback %02x\n", x);
				(void)x;
				//yeets[nyeeted] = x;
				//if (bitswap) yeets[nyeeted] = bitswap(yeets[nyeeted]);
				//++nyeeted;
			//}
		}
	}

end:

	if (vars->exclusive) {
		/*if (yeets[2] != 0x9a && yeets[0] == 0x01) {
			for (size_t i = 0; i < nyeeted; ++i) {
				printf("yeet %02hhx\n", yeets[i]);
			}
		}*/

		// FIXME: remove the dependency on tool78_hw_rl78_uart1!
		//if ((tool78_hw_rl78_uart1.flags & tool78_hw_flag_done_reset)
		//		/*&& !(tool78_hw_rl78_uart1.flags & tool78_hw_flag_do_ocd)*/) {
		//	// wait until everything is sent before reenabling the RX SM again
		//	while (!pio_sm_is_tx_fifo_empty(PINOUT_TOOL78_PIO, vars->smtx))
		//		; // wait until FIFO is clear
		//	while (!(PINOUT_TOOL78_PIO->fdebug & ((1u << vars->smtx) << PIO_FDEBUG_TXSTALL_LSB)))
		//		; // now wait for the SM to hang on the next 'pull'
		//}

		//gpio_put(3, true);
		//pio_sm_set_enabled(PINOUT_TOOL78_PIO, vars->smrx, true);
	}

	return i;
}


// ------

void tool78_entryseq_78k0r(enum tool78_entry typ) {
	gpio_pull_up(PINOUT_TOOL78_78K0R_TOOL0);
	gpio_set_function(PINOUT_TOOL78_78K0R_TOOL0, GPIO_FUNC_NULL);

	gpio_put_masked((1u<<PINOUT_TOOL78_nRESET) | (1u<<PINOUT_TOOL78_78K0R_FLMD0), 0u);
	gpio_put(PINOUT_TOOL78_78K0R_TOOL0, true);
	gpio_set_dir_out_masked((1u<<PINOUT_TOOL78_nRESET) | (1u<<PINOUT_TOOL78_78K0R_FLMD0));
	gpio_set_dir_out_masked((1u<<PINOUT_TOOL78_nRESET) | (1u<<PINOUT_TOOL78_78K0R_FLMD0)
			/*| (1u<<PINOUT_TOOL78_78K0R_TOOL0)*/);
	gpio_set_function(PINOUT_TOOL78_nRESET     , GPIO_FUNC_SIO);
	gpio_set_function(PINOUT_TOOL78_78K0R_FLMD0, GPIO_FUNC_SIO);
	//gpio_set_function(PINOUT_TOOL78_78K0R_TOOL0, GPIO_FUNC_SIO);

	busy_wait_ms(4); // tDP

	gpio_put(PINOUT_TOOL78_78K0R_FLMD0, true);
	//gpio_put(PINOUT_TOOL78_78K0R_TOOL0, true);

	busy_wait_ms(4); // tPR

	gpio_put(PINOUT_TOOL78_nRESET, true);

	busy_wait_ms(2); // tR0/2

	/*for (int i = 0; i < (int)typ; ++i) {
		gpio_put(PINOUT_TOOL78_78K0R_FLMD0, false);
		busy_wait_us_32(12); // TODO
		gpio_put(PINOUT_TOOL78_78K0R_FLMD0, true);
		busy_wait_us_32(12); // TODO
	}*/

	busy_wait_ms(2); // tR0/2
}
void tool78_deinit_78k0r(void) {
	gpio_put(PINOUT_TOOL78_nRESET, false);

	// start deiniting stuff
	gpio_disable_pulls(PINOUT_TOOL78_78K0R_TOOL0);
	gpio_disable_pulls(PINOUT_TOOL78_78K0R_FLMD0);
	gpio_set_function(PINOUT_TOOL78_78K0R_TOOL0, GPIO_FUNC_NULL);
	gpio_set_function(PINOUT_TOOL78_78K0R_FLMD0, GPIO_FUNC_NULL);

	busy_wait_us(80*500); // TODO

	//gpio_pull_down(PINOUT_TOOL78_nRESET);
	gpio_set_function(PINOUT_TOOL78_nRESET, GPIO_FUNC_NULL);
	gpio_pull_up(PINOUT_TOOL78_nRESET);
}

void tool78_entryseq_rl78(enum tool78_entry typ) {
	(void)typ;

	gpio_pull_up(PINOUT_TOOL78_RL78_TX);
	gpio_pull_up(PINOUT_TOOL78_RL78_RX);
	gpio_set_function(PINOUT_TOOL78_RL78_TX, GPIO_FUNC_NULL);
	gpio_set_function(PINOUT_TOOL78_RL78_RX, GPIO_FUNC_NULL);

	/*gpio_disable_pulls(PINOUT_TOOL78_RL78_TOOL0);
	gpio_disable_pulls(PINOUT_TOOL78_nRESET);*/
	gpio_put_masked((1u<<PINOUT_TOOL78_nRESET) | (1u<<PINOUT_TOOL78_RL78_TOOL0), 0u);
	gpio_set_dir_out_masked((1u<<PINOUT_TOOL78_nRESET) | (1u<<PINOUT_TOOL78_RL78_TOOL0));
	gpio_set_function(PINOUT_TOOL78_nRESET    , GPIO_FUNC_SIO);
	gpio_set_function(PINOUT_TOOL78_RL78_TOOL0, GPIO_FUNC_SIO);

	//busy_wait_us_32(500); // tSU
	busy_wait_ms(4);

	gpio_put(PINOUT_TOOL78_nRESET, true);

	//busy_wait_us_32(750); // 750us + tHD
	busy_wait_ms(4);

	gpio_put(PINOUT_TOOL78_RL78_TOOL0, true);

	//busy_wait_us_32(20); // tTM
	busy_wait_ms(2);
}
void tool78_deinit_rl78(void) {
	gpio_put(PINOUT_TOOL78_nRESET, false);

	// start deiniting stuff
	gpio_disable_pulls(PINOUT_TOOL78_RL78_TX   );
	gpio_disable_pulls(PINOUT_TOOL78_RL78_RX   );
	gpio_disable_pulls(PINOUT_TOOL78_RL78_TOOL0);
	gpio_set_function(PINOUT_TOOL78_RL78_TX   , GPIO_FUNC_NULL);
	gpio_set_function(PINOUT_TOOL78_RL78_RX   , GPIO_FUNC_NULL);
	gpio_set_function(PINOUT_TOOL78_RL78_TOOL0, GPIO_FUNC_NULL);

	busy_wait_us(500); // unknown?

	/*gpio_pull_down(PINOUT_TOOL78_nRESET);
	gpio_set_function(PINOUT_TOOL78_nRESET, GPIO_FUNC_NULL);*/
}