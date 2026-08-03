#include <tool78_hw_helpers.h>
#include <tool78_cmds.h>

#include <pico/stdio.h>
#include <pico/multicore.h>
#include <hardware/gpio.h>

#include <stdio.h>
#include <stdio.h>

const uint8_t mcu_firmware[0x4000] = {
#embed "mcu_firmware.bin"
};

PIO PINOUT_TOOL78_PIO = pio0;

#ifdef PICO_DEFAULT_LED_PIN

void core1_main() {
	gpio_init(PICO_DEFAULT_LED_PIN);
	gpio_set_dir(PICO_DEFAULT_LED_PIN, true);
	gpio_put(PICO_DEFAULT_LED_PIN, 0);

	while (1) {
		sleep_ms(250);
		gpio_put(PICO_DEFAULT_LED_PIN, 1);
		sleep_ms(250);
		gpio_put(PICO_DEFAULT_LED_PIN, 0);
	}
}
#endif

int main() {
	stdio_init_all();

	bool ok = false;

	do {
		enum tool78_stat m = 0;
		tool78_silicon_sig_t sig = { 0 };

		if ((m = tool78_init_sfp(&tool78_hw_78k0r_uart1, &sig)) != tool78_stat_ack) {
			printf("protocol init failure, %02X\n", m);
			break;
		}

		if (tool78_hw_78k0r_uart1.target_detail != tool78_target_78k0r_kx3l) {
			printf("unexpected hardware type!\n");
			break;
		}

		const struct tool78_silicon_sig_78k0r_kx3l *s = &sig.k780r_kx3l;
		printf("vendor: %02X\n", s->ven);
		printf("macro extension: %02X\n", s->met);
		printf("macro function code: %02X\n", s->msc);
		printf("device extension code: %02X %02X %02X\n", s->dec[0], s->dec[1], s->dec[2]);
		printf("internal flash last addr: %02X%02X%02X\n", s->uae[0], s->uae[1], s->uae[2]);
		printf("device name: %.10s\n", s->dev);
		printf("security flag info: %02X\n", s->scf);
		printf("boot block number: %02X\n", s->bot);
		printf("flash shield window start: %04X\n", s->fswsh << 8 | s->fswsl);
		printf("flash shield window end: %04X\n", s->fsweh << 8 | s->fswel);

		tool78_version_t ver = { 0 };
		if ((m = tool78_do_version_get(&tool78_hw_78k0r_uart1, &ver)) != tool78_stat_ack) {
			printf("get version fail, %02X\n", m);
			break;
		}

		printf("version data:\n");
		printf("dv: %02X %02X %02X\n", ver.dv[0], ver.dv[1], ver.dv[2]);
		printf("fw: %02X %02X %02X\n", ver.fw[0], ver.fw[1], ver.fw[2]);

		struct { uint16_t payload_offs, start, end; } ranges[4] = {
			{ 0x0   , 0x0   , 0x0FFF },
			{ 0x1000, 0x2000, 0x2FFF },
			{ 0x2000, 0x3000, 0x3FFF },
			{ 0x3000, 0x4000, 0x4FFF },
		};

#ifdef PICO_DEFAULT_LED_PIN
		multicore_launch_core1(core1_main);
#endif

		for (int i = 0; i < count_of(ranges); i++) {
			if ((m = tool78_do_block_erase(&tool78_hw_78k0r_uart1, ranges[i].start, ranges[i].end)) != tool78_stat_ack) {
				printf("chip block erase %04X-%04X failed: %02X\n", ranges[i].start, ranges[i].end, m);
				break;
			}
			printf("erased block %04X-%04X successfully\n", ranges[i].start, ranges[i].end);

			if ((m = tool78_do_programming(&tool78_hw_78k0r_uart1, ranges[i].start, ranges[i].end, &mcu_firmware[ranges[i].payload_offs])) != tool78_stat_ack) {
				printf("chip block write %04X-%04X failed: %02X\n", ranges[i].start, ranges[i].end, m);
				break;
			}

			printf("flashed block %04X-%04X successfully\n", ranges[i].start, ranges[i].end);

			if ((m = tool78_do_verify(&tool78_hw_78k0r_uart1, ranges[i].start, ranges[i].end, &mcu_firmware[ranges[i].payload_offs])) != tool78_stat_ack) {
				printf("chip block verify %04X-%04X failed: %02X\n", ranges[i].start, ranges[i].end, m);
				break;
			}

			printf("verified flashed block %04X-%04X successfully\n", ranges[i].start, ranges[i].end);
		}

		ok = true;
	} while (0);

	tool78_deinit_78k0r();

	if (ok) {
		printf("the MCU firmware has been successfully flashed.\n");
	}


#ifdef PICO_DEFAULT_LED_PIN
	multicore_reset_core1();

	gpio_put(PICO_DEFAULT_LED_PIN, ok);
#endif

	while (1) {
		tight_loop_contents();
	}
}
