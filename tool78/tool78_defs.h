
#ifndef TOOL78_DEFS_H_
#define TOOL78_DEFS_H_

#include <stddef.h>
#include <stdint.h>

// NOTE:
// * RL78/x1y flash block size: 0x400
// * RL78/x2y flash block size: 0x800

enum tool78_target {
	tool78k0_uart2        = 0x12,
	tool78k0_spi          = 0x13,
	tool78k0_uart2_extclk = 0x14,

	tool78k0r_uart1 = 0x21,

	tool78rl_uart1 = 0x31,
	tool78rl_uart2 = 0x32,
	tool78rl_ocd   = 0x38, // FIXME: do this in a better way
	tool78rlg10_uart1 = 0x41,

	tool78_mcu_mask  = 0xf0,
	tool78_mcu_78k0  = 0x10,
	tool78_mcu_78k0r = 0x20,
	tool78_mcu_rl78  = 0x30,
	tool78_mcu_rl78g10 = 0x40,

	tool78_phy_mask  = 0x0f,
	tool78_phy_uart1 = 0x01,
	tool78_phy_uart2 = 0x02,
	tool78_phy_spi   = 0x03,
	tool78_phy_uart2_extclk = 0x04,
	// TODO: Kx2-L/Ix2 phy
};
enum tool78_target_detail {
	tool78_target_default = 0x00,

	tool78_target_78k0_kx2  = 0x10,
	tool78_target_78k0_kx2l = 0x11, // or Ix2 // TODO: needs to be implemented
	// Kx2-L/Ix2: uses BRS instead of OFS, no SPI mode, TOOLC/TOOLD pins & no
	//            FLMD0, different entry sequence, BRS & blank check Kx3-L
	//            format, silicon sig also Kx3-L-style w/ 6 res bytes
	//            security set is 6 bytes but no FSW so last 4 bytes are 0xFF
	//            => may need another main target enum value (or at least
	//               another hw struct) due to different pinout + entry seq
	// TODO: 78K0S? most likely need another main target enum value

	tool78_target_78k0r_kx3  = 0x20,
	tool78_target_78k0r_kx3l = 0x21, // or Ix3
	// TODO: any others?

	tool78_target_rl78_g1x = 0x30,
	tool78_target_rl78_g2x = 0x31,
	tool78_target_rl78_f2x = 0x32,
};

enum tool78_entry {
	// FLMD0 pulse numbers
	tool78_entry_78k0_uart2 = 0,
	tool78_entry_78k0_uart2_extclk = 3,
	tool78_entry_78k0_spi = 8,

	// FLMD0 pulse numbers
	// TODO: or is it the byte sent over TOOL0? probably not
	tool78_entry_78k0r_uart1 = 0,

	// TOOL0 initial byte values
	tool78_entry_rl78_uart1 = 0x3a,
	tool78_entry_rl78_uart2 = 0x00,
	tool78_entry_rl78_ocd   = 0xc5,
	tool78_entry_rl78_ocd2  = 0xca, // 2-wire OCD (RL78/G23 only)
};

// TODO: initial baudrates? 9600 for 78k, 115.2k for RL78

enum tool78_cmd {
	// only used in UART comms (i.e. not in 78k0/Kx2 SPI)
	tool78_cmd_reset  = 0x00,
	// TODO: RL78/G13 v3.03: 3, 4, 6, 8, 0x16, 0x21, 0x10, 0x11 // 0x12, 0x14, 0x15
	// => 0x10 and on: FSL only
	tool78_cmd_verify = 0x13,
	// undocumented, RL78 only?
	tool78_cmd_verify_internal = 0x19,
	tool78_cmd_chip_erase = 0x20,
	// not available on RL78
	tool78_cmd_block_erase = 0x22,
	tool78_cmd_block_blank_check = 0x32,
	tool78_cmd_programming = 0x40,
	// RL78/G10 only
	tool78_cmd_crc_check = 0x53,
	// RL78/G10 only
	tool78_cmd_write_after_erase = 0x60,
	// 78k0/Kx2 SPI only
	tool78_cmd_status = 0x70,
	// 78k0/Kx2 only
	tool78_cmd_osc_freq_set = 0x90,
	// not on 78k0/Kx2. arguments are different for 78K0R/Kx3-L and RL78!
	tool78_cmd_baud_rate_set = 0x9a,
	tool78_cmd_security_idauth = 0x9c,
	// RL78/G23 only
	// 78K0R/Kx3-L and RL78/x1y: also sets flash shield window
	tool78_cmd_security_set = 0xa0,
	// RL78 only
	tool78_cmd_security_get = 0xa1,
	// RL78 only
	tool78_cmd_security_release = 0xa2,
	tool78_cmd_exopts_set = 0xa5,
	// RL78/G23 only
	tool78_cmd_flash_rdp_set = 0xab,
	// RL78/G23 only
	tool78_cmd_fsw_set = 0xac,
	// RL78/G23 only
	tool78_cmd_fsw_get = 0xad,
	// RL78/G23 only
	tool78_cmd_checksum = 0xb0,
	// response format & length differ between all MCU versions!
	// length & first few bytes can be used to discern between MCUs
	// EXCEPT: osc freq set/baudrate set cmd required as first after reset??
	tool78_cmd_silicon_signature = 0xc0,
	// not on RL78. same response for 78k0/Kx2 and 78K0R/Kx3-L, so kinda useless
	tool78_cmd_version_get = 0xc5,
};

enum tool78_stat {
	tool78_stat_internal_error    = 0x00, // error internal to this implementation code

	tool78_stat_cmd_not_supported = 0x04,
	tool78_stat_parameter_error   = 0x05,
	tool78_stat_ack               = 0x06,
	tool78_stat_checksum_error    = 0x07,
	tool78_stat_verify_error      = 0x0f,
	// can't do command due to protection settings
	tool78_stat_protect_error     = 0x10,
	tool78_stat_nak               = 0x15,
	tool78_stat_mrg10_erase_verif_error = 0x1a,
	tool78_stat_mrg11_internal_verif_error = 0x1b,
	tool78_stat_write_error       = 0x1c,
	// 78k0/Kx2 only. security related?
	tool78_stat_read_error        = 0x20,
	// 78k0/Kx2 SPI only
	tool78_stat_busy              = 0xff,

	tool78_stat_protocol_error    = 0xc1, // low-level wire protocol violation
	tool78_stat_bad_mcu_for_cmd   = 0xc2,
	tool78_stat_timeout_error     = 0xc3,
	tool78_stat_fatal_hw_error    = 0xc4,
	tool78_stat_parity_error      = 0xc5,
	tool78_stat_bad_flash_size    = 0xc6,
	tool78_stat_no_ocd_status     = 0xc7,

	// TODO: OCD status stuff (for 0x91)
};

// these commands use a checksum that is the bitwise complement from the usual
// checksum (used in the flash programming protocol)
enum tool78ocd_cmd {
	//tool78ocd_cmd_reset = 0x00,
	tool78ocd_cmd_version = 0x90, // also called "ping"
	// also called "unlock", as it requires a 10-byte passphrase
	tool78ocd_cmd_connect = 0x91,
	// read from memory?
	tool78ocd_cmd_read = 0x92,
	// write to memory (2 bytes addr, 1 byte len, data)
	tool78ocd_cmd_write = 0x93,
	// execute code at 0xf07e0 (TODO: verify addr across versions)
	tool78ocd_cmd_exec = 0x94,
	tool78ocd_cmd_exit_reti = 0x95, // TODO: what is this?
	// TODO: 96 too?
	tool78ocd_cmd_exit_ram = 0x97, // TODO: what is this?

	tool78ocd_cmdg10_connect   = 0x55,
	tool78ocd_cmdg10_setptr    = 0x56,
	tool78ocd_cmdg10_read_raw  = 0x57,
	tool78ocd_cmdg10_write_raw = 0x58,
	tool78ocd_cmdg10_read_word = 0x59,
	tool78ocd_cmdg10_write_word= 0x5a,
	tool78ocd_cmdg10_exec      = 0x5b,
	tool78ocd_cmdg10_exit_reti = 0x5c,
};

enum tool78_frame {
	// first byte of command
	tool78_frame_soh = 0x01,
	tool78_frame_cmd = 0x01,
	// first byte of data or response status
	tool78_frame_stx = 0x02,
	tool78_frame_dat = 0x02,
	// end of command or final data/resp commandframe
	tool78_frame_etx = 0x03,
	tool78_frame_end = 0x03,
	// end of data/resp for non-final commandframe
	tool78_frame_etb = 0x17,
	tool78_frame_eot = 0x17,
};

static inline uint8_t tool78_digest_checksum8(uint8_t r, size_t len, const uint8_t* data) {
	for (size_t i = 0; i < len; ++i) r = r - data[i];
	return r;
}
static inline uint8_t tool78_calc_checksum8(size_t len, const uint8_t* data) {
	return tool78_digest_checksum8(0, len, data);
}
static inline uint8_t tool78_calc_ocd_checksum8(size_t len, const uint8_t* data) {
	//return /*~*/(tool78_calc_checksum8(len, data)/*+1*/)&0xff;
	uint8_t r = 0;
	for (size_t i = 0; i < len; ++i) r = r + data[i];
	r = (r - 1) & 0xff;
	// if corrupt checksum:
	//	r = (r + 1) & 0xff;
	return r;
}

static inline uint16_t tool78_digest_checksum16(uint16_t r, size_t len, const uint8_t* data) {
	for (size_t i = 0; i < len; ++i) r = r - data[i]; // yes it's done byte by byte
	return r;
}
static inline uint8_t tool78_calc_checksum16(size_t len, const uint8_t* data) {
	return tool78_digest_checksum16(0, len, data);
}

struct __attribute__((__packed__)) tool78_silicon_sig_78k0 {
	uint8_t ven; // vendor code. 0x10 == NEC
	uint8_t met; // macro extension (0x7f)
	uint8_t msc; // macro function code (0x04)
	uint8_t dec; // device extension code (0x07)
	uint8_t end[3]; // internal flash memory last address
	uint8_t dev[10]; // device name
	uint8_t scf; // security flag info
	uint8_t bot; // boot block number (0x03)
	// "for above fields except BOT, the lower 7 bits are used as data entity,
	//  and the highest bit is used as an odd parity"
};
struct __attribute__((__packed__)) tool78_silicon_sig_78k0r_kx3 {
	uint8_t ven; // vendor code. 0x10 == NEC
	uint8_t met; // macro extension (0x6f or 0x7f)
	uint8_t msc; // macro function code (0x04)
	uint8_t dec[2]; // device extension code
	uint8_t uae[3]; // internal flash memory last address
	uint8_t dev[10]; // device name
	uint8_t scf; // security flag info
	uint8_t bot; // boot block number (0x03)
	uint8_t fswsh; // flash shield window start hi
	uint8_t fswsl; // flash shield window start lo
	uint8_t fsweh; // flash shield window end hi
	uint8_t fswel; // flash shield window end lo
};
struct __attribute__((__packed__)) tool78_silicon_sig_78k0r_kx3l {
	uint8_t ven; // vendor code. 0x10 == NEC
	uint8_t met; // macro extension (0x6f or 0x7f)
	uint8_t msc; // macro function code (0x04)
	uint8_t dec[3]; // device extension code
	uint8_t uae[3]; // internal flash memory last address
	uint8_t dev[10]; // device name
	uint8_t scf; // security flag info
	uint8_t bot; // boot block number (0x03)
	uint8_t fswsh; // flash shield window start hi
	uint8_t fswsl; // flash shield window start lo
	uint8_t fsweh; // flash shield window end hi
	uint8_t fswel; // flash shield window end lo
	uint8_t res[2];
	// "for VEN, MET, MSC and DEC, the lower 7 bits are used as data entity,
	//  and the highest bit is used as an odd parity."
};
struct __attribute__((__packed__)) tool78_silicon_sig_rl78 {
	uint8_t dec[3]; // device code
	// G1x: 10 00 06
	// F2x: 10 00 0b
	// G2x: 10 00 0a
	uint8_t dev[10]; // device name
	uint8_t cen[3]; // last address of code flash rom, little-endian
	uint8_t den[3]; // last address of data flash rom, little-endian
	uint8_t ver[3]; // firmware version number
	// no parity bits
};
typedef union __attribute__((__packed__)) tool78_silicon_sig {
	struct tool78_silicon_sig_78k0  k780 ;
	struct tool78_silicon_sig_78k0r_kx3  k780r_kx3;
	struct tool78_silicon_sig_78k0r_kx3l k780r_kx3l;
	struct tool78_silicon_sig_rl78  rl78 ;
} tool78_silicon_sig_t;

// same structure for 78k0 and 78k0r, not used in rl78
typedef struct __attribute__((__packed__)) tool78_version {
	uint8_t dv[3]; // all zero
	uint8_t fw[3];
} tool78_version_t;

enum tool78_sec_flag { // 78k0/78k0r
	tool78_sec_flag_boot_rw_allow = 1<<4,
	tool78_sec_flag_prog_allow = 1<<2,
	tool78_sec_flag_blk_erase_allow = 1<<1,
	// always 1 on RL78 as chip erase doesn't exist on it
	tool78_sec_flag_chip_erase_allow = 1<<0,

	tool78_sec_flag_default_78k = 0xe8, // 0b11101000: all set to 1 by default
	tool78_sec_flag_default_rl78= 0xe9, // 0b11101000: all set to 1 by default

	tool78_sec_flag_boot_xchg = 1<<0 // rl78 only
};
enum tool78_sec_flag12 { // RL78/G23
	tool78_sec_flag1_boot_rw_allow = 1<<1,
	tool78_sec_flag1_blk_erase_allow = 1<<2,
	tool78_sec_flag1_prog_allow = 1<<4,

	tool78_sec_flag2_idauth_disable = 1<<0,
	tool78_sec_flag2_debug_enable = 1<<2,
	tool78_sec_flag2_rdp_write_allow = 1<<3, // NOTE: must be 1 when writing
	tool78_sec_flag2_option_write_allow = 1<<4 // NOTE: must be 1 when writing
};

struct tool78_security {
	uint8_t flg;
	uint8_t bot;
	// ^ only these 2 used for 78k0
	uint16_t fsws;
	uint16_t fswe;
	// ^ these two used for RL78/x1y
	uint8_t flg2;
	// ^ used in RL78/G23
};

struct tool78_fsw_settings {
	uint16_t block_start;
	uint16_t block_end;
	bool fswopt_rw_allow;
	bool fsw_area_invert;
};

enum tool78_g10_flash_size {
	tool78_g10_flash_16k = 0x3f,
	tool78_g10_flash_8k  = 0x1f,
	tool78_g10_flash_4k  = 0x0f,
	tool78_g10_flash_2k  = 0x07,
	tool78_g10_flash_1k  = 0x03,
	tool78_g10_flash_512 = 0x01,
};

#endif

