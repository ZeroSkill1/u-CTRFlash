
#ifndef TOOL78_CMDS_H_
#define TOOL78_CMDS_H_

#include "tool78_defs.h"
#include "tool78_hw.h"

enum tool78_stat tool78_do_reset(struct tool78_hw* hw);

enum tool78_stat tool78_do_baud_rate_set(struct tool78_hw* hw);
enum tool78_stat tool78_do_osc_freq_set(struct tool78_hw* hw);

static inline enum tool78_stat tool78_do_generic_baudrate(struct tool78_hw* hw) {
	if ((hw->target & tool78_mcu_mask) == tool78_mcu_78k0)
		return tool78_do_osc_freq_set(hw);
	else
		return tool78_do_baud_rate_set(hw);
}

enum tool78_stat tool78_do_chip_erase(struct tool78_hw* hw);
enum tool78_stat tool78_do_block_erase(struct tool78_hw* hw, uint32_t start, uint32_t end);

enum tool78_stat tool78_do_programming(struct tool78_hw* hw, uint32_t start,
		uint32_t end, const uint8_t* src);
enum tool78_stat tool78_do_verify(struct tool78_hw* hw, uint32_t start,
		uint32_t end, const uint8_t* src);
enum tool78_stat tool78_do_block_blank_check(struct tool78_hw* hw,
		uint32_t start, uint32_t end, bool check_flash_opt /* RL78 only */);
enum tool78_stat tool78_do_checksum(struct tool78_hw* hw, uint32_t start,
		uint32_t end, uint16_t* ckout);

enum tool78_stat tool78_do_silicon_signature(struct tool78_hw* hw,
		tool78_silicon_sig_t* sig_dest);
enum tool78_stat tool78_do_version_get(struct tool78_hw* hw, tool78_version_t* vout);
enum tool78_stat tool78_do_security_set(struct tool78_hw* hw,
		const struct tool78_security* sec);
enum tool78_stat tool78_do_security_get(struct tool78_hw* hw, // RL78 only
		struct tool78_security* sec);
enum tool78_stat tool78_do_security_release(struct tool78_hw* hw);

// RL78/G10,G1M,G1N commands

// this one is very hacky
enum tool78_stat tool78_do_g10_get_flash_size(struct tool78_hw*,
		enum tool78_g10_flash_size* fsz);
enum tool78_stat tool78_do_g10_check_crc(struct tool78_hw*,
		enum tool78_g10_flash_size fsz, uint16_t* crc);
enum tool78_stat tool78_do_g10_erase_then_write(struct tool78_hw*,
		enum tool78_g10_flash_size fsz, const uint8_t* data, size_t datalen);

// new commands from the RL78/G23 (Proto C)

// 0x9C: payload = passwd; ack/nak
enum tool78_stat tool78_do_security_idauth(struct tool78_hw* hw,
		const uint8_t passwd[static 10]);
// 0xA5: payload = exopt; ack/nak
enum tool78_stat tool78_do_exopt_set(struct tool78_hw* hw,
		const uint8_t exopt[static 14]);
// 0xAB: payload = block_start || block_end (little-endian); ack/nak
enum tool78_stat tool78_do_flash_rdp_set(struct tool78_hw* hw,
		uint16_t block_start, uint16_t block_end, bool rdp_rw_allow);
// 0xAC
enum tool78_stat tool78_do_fsw_set(struct tool78_hw* hw,
		const struct tool78_fsw_settings* fswopt);
// 0xAD
enum tool78_stat tool78_do_fsw_get(struct tool78_hw* hw,
		struct tool78_fsw_settings* fswopt);

// ----

//int tool78_ocd_reset(struct tool78_hw* hw);
int tool78_ocd_version(struct tool78_hw* hw, uint16_t* ver);
int tool78_ocd_connect(struct tool78_hw* hw, const uint8_t passwd[10]);
int tool78_ocd_read(struct tool78_hw* hw, uint16_t off, uint8_t len,
		uint8_t* data); // only is able to read from 0xf0000
int tool78_ocd_write(struct tool78_hw* hw, uint16_t addr, uint8_t len,
		const uint8_t* data);
int tool78_ocd_exec(struct tool78_hw* hw); // call 0xf07e0
int tool78_ocd_leave(struct tool78_hw* hw, bool exit_to_ram);

// ----

enum tool78_stat tool78_init_sfp(struct tool78_hw* hw, tool78_silicon_sig_t* sig);
enum tool78_stat tool78_init_ocd(struct tool78_hw* hw, uint16_t* ver,
		const uint8_t passwd[10]);



#endif

