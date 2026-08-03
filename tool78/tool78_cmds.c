
#include <stdio.h>
#include <string.h>

#include <pico/time.h>
#include <pico/timeout_helper.h>
#include <hardware/gpio.h>

#include "tool78_defs.h"
#include "tool78_hw.h"
#include "tool78_cmds.h"

static uint8_t databuf[255];

// variable name    |  /fRH=/8MHz                  us                    us
// 78k    RL78      |  78k0                      78k0r                 rl78
// -----------------+----------------------------------------------------------
// tCOM   tSDNx/tMB |   106                        13.2                  62 (good enough)
// tSF              |   215                         X                     X
// tWT0   tCS1      |   172                       200                   255
// tWT1  (tSD8)     |857883+128*44160      (1420100+128*281100)         212
// tWT2   tCS3      |214714*512+128*44160  (3300+271600+128*275000) (248862+299307)
// tWT3   tCS5      |  1506                         0                  1432
// tWT4   tDS5      |893355/2                  149900               (309870+488315)
// tWT5   tSS5      |100407                   1187500               (1732+58+(17403+29293)*128+(184+44))
// tWT6   tCS2      |   686                         0                   351
// tWT7   tDS2      | 12827                         0                 11981
// tWT8   tCS4      | 55044                     18000               (3805+168+(5035+1110)*128+(5827+318))
// tWT9  (tCS8)     |  1238                         X                   154
// tWT10  tCS6      |     X                       379.2                4735
// tWT11  tCS11     |  1233                         0                   111
// tWT12 (tCS9)     |   252                         0               (146110+534723+(5035+1110)*128+(203+57))
// tWT13  tCS7      |   975                         0                   168
// tWT14  tDS7      |66005812/2                    70.2             (277095+1075967)
// tWT15            |66018156/2                  1152.3                   X
// tWT16  tCS10     |   583                         0                   219
// tFD1   tSD10     | 54368                         0               (72+128*30720)
// tFD2   tSD11     |   321                        11.4                 512
// tFD3   tSD2      |   163                       416.4                  41
// tFD3   tSD5      |   163                       416.4                  41
// tFD4   tSD7      |   163                       985.8                  32
//       ~tCS8~     |
//       ~tSD8~     |
//       ~tCS9~     |

struct delayvalues { int none; int k0; int k0r; int rl78; };
#define D(a,b,c) ((struct delayvalues){.none=1000, .k0=(a), .k0r=(b), .rl78=(c)})
static const struct delayvalues delays[23] = {
	D(106, 14*50, 62), // tCOM
	D(215, -1, -1), // tSF (78k0 SPI only)
	D(500*1000, 4000, 255), // tWT0
	D(857883+128*44160,     (1420100+128*281100),        212), // tWT1
	D(214714*512+128*44160, (3300+271600+128*275000),(248862+299307)), // tWT2
	D(1506,4000,1432), // tWT3
	D(893355/2,                 149900,              (309870+488315)), // tWT4
	D(100407  ,                1187500,              (1732+58+(17403+29293)*128+(184+44))), // tWT5
	D(686*1000,4000,351), // tWT6
	D(12827*1000, 4000, 11981), // tWT7
	D(55044, 1800, (3805+168+(5035+1110)*128+(5827+318))), // tWT8
	D(1238, -1, 154), // tWT9,
	D(1233, 380, 4735), // tWT10,
	D(40*1000, 4000, 111), // tWT11,
	D(252, 4000, (146110+534723+(5035+1110)*128+(203+57))), // tWT12
	D(975, 4000, 168), // tWT13
	D(66005812/2                   ,71,             (277095+1075967)), // tWT14
	D(66018156/2                 ,1153,                 -1), // tWT15
	D(583, 4000, 219), // tWT16
	D(54368, 4000, (72+128*30720)), // tFD1
	D(321, 12*1000, 512), // tFD2
	D(163, 417*1000, 41), // tFD3
	D(163, 986*1000, 32), // tFD4
};
// SD5 SS5 SD2 CS4 SD7 CS8 SD8 CS9
#define tCOM  (((&delays[ 0].none)[(hw->target & tool78_mcu_mask) >> 4]))
#define tSF   (((&delays[ 1].none)[(hw->target & tool78_mcu_mask) >> 4]))
#define tWT0  (((&delays[ 2].none)[(hw->target & tool78_mcu_mask) >> 4]))
#define tWT1  (((&delays[ 3].none)[(hw->target & tool78_mcu_mask) >> 4]))
#define tWT2  (((&delays[ 4].none)[(hw->target & tool78_mcu_mask) >> 4]))
#define tWT3  (((&delays[ 5].none)[(hw->target & tool78_mcu_mask) >> 4]))
#define tWT4  (((&delays[ 6].none)[(hw->target & tool78_mcu_mask) >> 4]))
#define tWT5  (((&delays[ 7].none)[(hw->target & tool78_mcu_mask) >> 4]))
#define tWT6  (((&delays[ 8].none)[(hw->target & tool78_mcu_mask) >> 4]))
#define tWT7  (((&delays[ 9].none)[(hw->target & tool78_mcu_mask) >> 4]))
#define tWT8  (((&delays[10].none)[(hw->target & tool78_mcu_mask) >> 4]))
#define tWT9  (((&delays[11].none)[(hw->target & tool78_mcu_mask) >> 4]))
#define tWT10 (((&delays[12].none)[(hw->target & tool78_mcu_mask) >> 4]))
#define tWT11 (((&delays[13].none)[(hw->target & tool78_mcu_mask) >> 4]))
#define tWT12 (((&delays[14].none)[(hw->target & tool78_mcu_mask) >> 4]))
#define tWT13 (((&delays[15].none)[(hw->target & tool78_mcu_mask) >> 4]))
#define tWT14 (((&delays[16].none)[(hw->target & tool78_mcu_mask) >> 4]))
#define tWT15 (((&delays[17].none)[(hw->target & tool78_mcu_mask) >> 4]))
#define tWT16 (((&delays[18].none)[(hw->target & tool78_mcu_mask) >> 4]))
#define tFD1  (((&delays[19].none)[(hw->target & tool78_mcu_mask) >> 4]))
#define tFD2  (((&delays[20].none)[(hw->target & tool78_mcu_mask) >> 4]))
#define tFD3  (((&delays[21].none)[(hw->target & tool78_mcu_mask) >> 4]))
#define tFD4  (((&delays[22].none)[(hw->target & tool78_mcu_mask) >> 4]))
#define tSD5 tFD3
#define tSS5 tWT5
#define tSD2 tFD3
#define tCS4 tWT8
#define tSD7 tFD4
#define tCS8 tWT9
#define tSD8 tWT1
#define tCS9 tWT12
#define tG23 (1000*1000) /* RL78/G23 datasheet just says "everything 1ms lol" */

// G10 proto
#define tDTR (3*1000)
#define tDRT    20
#define tCRC  1000
#define tPRO  1000
#define tVER 20000
#define tERA (350*1000)
#define tDT      1
/*#define tDR  1*/


static enum tool78_stat tool78_data_send(struct tool78_hw* hw,
		int len, const uint8_t* data, bool final_block) {
	//printf("datasend: len=%d data=%02x %02x %02x ...\n", len, data[0], data[1], data[2]);
	if (len == 0) return tool78_stat_ack;
	if (len > 0x100) return tool78_stat_internal_error;
	uint8_t lenfield = len & 0xff;

	uint8_t pre[2], // stx, len
	        post[2]; // cksum, etx/etb
	pre[0] = tool78_frame_stx;
	pre[1] = lenfield;
	post[0] = 0; // checksum
	post[1] = final_block ? tool78_frame_etx : tool78_frame_etb;

	uint8_t r = 0;
	r = tool78_digest_checksum8(r, 1, &pre[1]); // len
	if (len) r = tool78_digest_checksum8(r, len, data);
	post[0] = r;

	//printf("data cksum=%02x\n", r);

	int rr = hw->send(2, pre, -1);
	if (rr != 2) return tool78_stat_internal_error;

	if (len) {
		rr = hw->send(len, data, -1);
		if (rr != len) {
			//printf("data not sent properly: %d\n", r);
			return tool78_stat_internal_error;
		}
	}

	rr = hw->send(2, post, -1);
	if (rr != 2) return tool78_stat_internal_error;

	return tool78_stat_ack;
}

static enum tool78_stat tool78_cmd_send(struct tool78_hw* hw, uint8_t cmd,
		int len, const uint8_t* data) {
	if (len >= 0x100) return tool78_stat_internal_error;
	uint8_t lenfield = len &= 0xff;

	busy_wait_us_32(tCOM); // aka tSN6(/tDN6?) (reset?) or tMB (others?)

	uint8_t pre[3], // soh, len, cmd
	        post[2]; // cksum, etx
	pre[0] = tool78_frame_soh;
	pre[1] = lenfield + 1;
	pre[2] = cmd;
	post[0] = 0; // checksum
	post[1] = tool78_frame_etx;

	uint8_t r = 0;
	r = tool78_digest_checksum8(r, 2, &pre[1]); // len and cmd
	if (len) r = tool78_digest_checksum8(r, len, data);
	post[0] = r;

	int rr = hw->send(3, pre, -1);
	if (rr != 3) return tool78_stat_internal_error;

	if (len) {
		rr = hw->send(len, data, -1);
		if (rr != len) return tool78_stat_internal_error;
	}

	rr = hw->send(2, post, -1);
	if (rr != 2) return tool78_stat_internal_error;

	return tool78_stat_ack;
}

static enum tool78_stat tool78_data_recv_ex(struct tool78_hw* hw, uint8_t* buf,
		int expected_len, int timeout_us_first, int* len_recv) {
	uint8_t pre[10]; // stx, len
	uint8_t post[2]; // cksum, etx/etb
	//printf("recv to=%d\n", timeout_us_first);

	int rr = hw->recv(1, &pre[0], timeout_us_first);
	//printf("data recv rr=%d\n", rr);
	if (rr == 0) return tool78_stat_busy;
	if (rr != 1) return tool78_stat_internal_error;
	//printf("pre[0]=0x%02x\n", pre[0]);
	if (hw->target == tool78k0_spi && pre[0] == tool78_stat_busy)
		return tool78_stat_busy;
	if (pre[0] != tool78_frame_stx) {
		//printf("didn't get stx, but %02hhx\n", pre[0]);
		return (hw->flags & tool78_hw_flag_done_reset)
			? tool78_stat_busy : tool78_stat_protocol_error;
	}

	rr = hw->recv(1, &pre[1], 100*100);
	//printf("rr=%d (0x%08lx) pre[1]=0x%02x\n", rr, (uint32_t)rr, pre[1]);
	if (rr != 1) {
		/*for (int i = 0; i < rr && rr < 256; ++i)
			printf("pre[%d]=0x%02x\n", i+1, pre[i+1]);
		printf("interr\n");*/
		return tool78_stat_internal_error;
	}

	int len = pre[1];
	if (!len) len = 256;
	if (len_recv) *len_recv = len;

	if (hw->target == tool78k0_spi && pre[1] == tool78_stat_busy)
		return tool78_stat_busy;
	if (expected_len >= 0 && len != expected_len) {
		//printf("bad length, wanted %d but got %d\n", expected_len, len);
		return tool78_stat_protocol_error;
	}

	rr = hw->recv(len, buf, 1000*len*100);
	if (rr != len) return tool78_stat_internal_error;

	rr = hw->recv(2, post, 200*100);
	if (rr != 2) return tool78_stat_internal_error;
	if (post[1] != tool78_frame_etx && post[1] != tool78_frame_etb) {
		//printf("didn't get etx/etb, but got %02hhx\n", post[1]);
		return tool78_stat_protocol_error;
	}

	uint8_t ck = tool78_calc_checksum8(len, buf);
	ck = tool78_digest_checksum8(ck, 1, &pre[1]);
	if (ck != post[0]) {
		//printf("bad checksum: %02hhx <-> %02hxx\n", ck, post[0]);
		return tool78_stat_protocol_error;
	}

	return tool78_stat_ack;
}
inline static enum tool78_stat tool78_data_recv(struct tool78_hw* hw,
		uint8_t* buf, int expected_len, int timeout_us_first) {
	return tool78_data_recv_ex(hw, buf, expected_len, timeout_us_first, NULL);
}
/*#define tool78_wait_status(hw, l, timeout_us, ...) \
	tool78_wait_status__impl(hw, l, timeout_us, __VA_OPT__(1 ? __VA_ARGS__ : ) -1) \
*/
static enum tool78_stat tool78_wait_status__impl(struct tool78_hw* hw, int l, int timeout_us/*, int minl*/, const char* fnname, int line) {
	/*if (hw->target == tool78k0_spi && timeout_us >= 0) {
		busy_wait_us_32(timeout_us);
		timeout_us *= 2;
	}*/

	enum tool78_stat st;
	bool blockinf = timeout_us < 0;
	absolute_time_t at = make_timeout_time_us(timeout_us);
	timeout_state_t ts = {0};
	check_timeout_fn ct = NULL;
	if (!blockinf) ct = init_single_timeout_until(&ts, at);

	for (int i = 0; (blockinf && i < 64) || !ct(&ts, false); ++i) {
		if (hw->target == tool78k0_spi) {
			// with SPI, a status request must be sent manually
			enum tool78_stat st = tool78_cmd_send(hw, tool78_cmd_status, 0, NULL);
			if (st != tool78_stat_ack) return st;

			busy_wait_us_32(tSF);
		}

		st = tool78_data_recv(hw, databuf, l, timeout_us);
		//printf("recv: 0x%02x\n", st);
		if (st == tool78_stat_busy) goto cont;
		if (st != tool78_stat_ack) return st;

		st = databuf[0];
		//printf("recvdat: 0x%02x\n", st);
		if (st == tool78_stat_busy) goto cont;

		return st;

	cont:
		busy_wait_us_32(timeout_us/64);
	}

	printf("wait t/o: %s:%d\n", fnname, line);
	return tool78_stat_timeout_error;
}
#define tool78_wait_status(hw, l, to) tool78_wait_status__impl(hw, l, to, __func__, __LINE__);

// ----

enum tool78_stat tool78_do_reset(struct tool78_hw* hw) {
	//busy_wait_ms(100);

	enum tool78_stat st = tool78_cmd_send(hw, tool78_cmd_reset, 0, NULL);
	//printf("send st=%02x\n", st);
	if (st != tool78_stat_ack) return st;

	st = tool78_wait_status(hw, 1, tWT0*10); // aka tCS1
	//printf("wait st=%02x\n", st);
	if (st != tool78_stat_ack) return st;
	//printf("reset cmd sent\n");

	return st;
}

enum tool78_stat tool78_do_baud_rate_set(struct tool78_hw* hw) {
	uint8_t d0x[5];
	enum tool78_stat st;

	switch (hw->target & tool78_mcu_mask) {
	case tool78_mcu_78k0r:
		//st = tool78_stat_ack;
		//break;
		// on the 78K0R/Kx3-L, baudrate set also switches to a fixed 115200 baud
		// line, much like the 78K0/Kx2 interface
		d0x[0] = 0x00;
		d0x[1] = 0x00;
		d0x[2] = 0x0a;
		d0x[3] = 0x00;
		d0x[4] = 0x00; // Kx3-L only

		// MODIFIED: fixed this fixme
		// FIXME: 78K0R/Kx3 wants length=4, Kx3-L wants length=5 !

		int cmdlen = hw->target_detail == tool78_target_78k0r_kx3l ? 5 : 4;

		st = tool78_cmd_send(hw, tool78_cmd_baud_rate_set, cmdlen, d0x);
		if (st != tool78_stat_ack) return st;

		//printf("done brs\n");
		busy_wait_us_32(tWT10);

		// now at 115.2kbaud, so let the physical layer switch to it
		hw->set_baudrate(115200);

		//printf("set baudrate, reresetting\n");
		st = tool78_do_reset(hw);
		//printf("done reset again %02x\n", st);
		break;
	case tool78_mcu_rl78:
		// TODO: don't hardcode baudrate? idk
		d0x[0] = 0x00; // 0=115.2k 1=250k 2=500k 3=1M
		d0x[1] = 33; // 3.3V, will switch to full-speed mode

		st = tool78_cmd_send(hw, tool78_cmd_baud_rate_set, 2, d0x);
		//printf("brs send=0x%02x\n", st);
		if (st != tool78_stat_ack) return st;

		st = tool78_wait_status(hw, -1, tWT10); // aka tCS6 // -1 because can be 1 (OCD mode, denied) or 3
		//printf("brs stat=0x%02x\n", st);
		if (st != tool78_stat_ack) return st;
		// let's ignore the two data bytes for now


		// now at 115.2kbaud, so let the physical layer switch to it
		hw->set_baudrate(115200);

		hw->flags |= tool78_hw_flag_done_reset;

		if (hw->flags & tool78_hw_flag_do_ocd) {
			//printf("brs ok, wait for ocd...\n");
			// wait for ack 0x00 byte
			uint8_t byte = 0xff;
			int rr = hw->recv(1, &byte, 10000);
			//printf("ocd rr=%d, v=%02x\n", rr, byte);
			if (rr != 1) st = tool78_stat_timeout_error;
			else if (byte != 0x00) st = tool78_stat_protocol_error;
			else st = tool78_stat_ack;
		} else {
			st = tool78_do_reset(hw);
		}
		break;
	case tool78_mcu_78k0: default:
		return tool78_stat_bad_mcu_for_cmd;
	}

	return st;
}

enum tool78_stat tool78_do_osc_freq_set(struct tool78_hw* hw) {
	if ((hw->target & tool78_mcu_mask) != tool78_mcu_78k0)
		return tool78_stat_bad_mcu_for_cmd;

	// OFS specifies the frequency of the EXTCLK signal (see 78k0_uart2_extclk)
	// otherwise kinda irrelevant, EXCEPT THAT IS ALSO SWITCHES THE BAUDRATE TO
	// A FIXED VALUE OF 115200 BAUD!
	// so for now we kinda hardcode it to the value for the EXTCLK frequency to
	// the one also used in 78k0_uart2_extclk.c, aka 8 MHz
	uint8_t d0x[4];
	// one-digit BCD, last element is an exponent
	// i.e. value is (d00*100 + d01*10 + d02) * 10^d03 in Hz
	d0x[0] = 8;
	d0x[1] = 0;
	d0x[2] = 0;
	d0x[3] = 4;

	enum tool78_stat st = tool78_cmd_send(hw, tool78_cmd_osc_freq_set, 4, d0x);
	if (st != tool78_stat_ack) return st;

	// now at 115.2kbaud, so let the physical layer switch to it
	if (hw->target != tool78k0_spi) hw->set_baudrate(115200);

	st = tool78_wait_status(hw, 1, tWT9);
	if (st != tool78_stat_ack) return st;

	return st;
}

// -

enum tool78_stat tool78_do_chip_erase(struct tool78_hw* hw) {
	if ((hw->target & tool78_mcu_mask) == tool78_mcu_rl78)
		return tool78_stat_bad_mcu_for_cmd;

	enum tool78_stat st = tool78_cmd_send(hw, tool78_cmd_chip_erase, 0, NULL);
	if (st != tool78_stat_ack) return st;

	return tool78_wait_status(hw, 1, tWT1);
}

enum tool78_stat tool78_do_block_erase(struct tool78_hw* hw, uint32_t start, uint32_t end) {
	bool isrl78 = (hw->target & tool78_mcu_mask) == tool78_mcu_rl78;
	if (isrl78) end = 0xffffff;
	if (start >= end) return 0;

	uint8_t data[6];
	// on the rl78, the end address is ignored/unused and it's little-endian
	if (isrl78) {
		data[0] = (start >>  0) & 0xff;
		data[1] = (start >>  8) & 0xff;
		data[2] = (start >> 16) & 0xff;
	} else {
		data[0] = (start >> 16) & 0xff;
		data[1] = (start >>  8) & 0xff;
		data[2] = (start >>  0) & 0xff;
		data[3] = (end   >> 16) & 0xff;
		data[4] = (end   >>  8) & 0xff;
		data[5] = (end   >>  0) & 0xff;
	}

	enum tool78_stat st = tool78_cmd_send(hw, tool78_cmd_block_erase,
			isrl78 ? 3 : 6, data);
	if (st != tool78_stat_ack) return st;

	st = tool78_wait_status(hw, 1, tWT2); // aka tCS3
	if (st != tool78_stat_ack) return st;

	//st = tool78_wait_status(hw, 1, tWTx); // TODO: ?
	return st;
}

enum tool78_stat tool78_do_programming(struct tool78_hw* hw, uint32_t start,
		uint32_t end, const uint8_t* src) {
	if (start >= end) return 0;
	bool isrl78 = (hw->target & tool78_mcu_mask) == tool78_mcu_rl78;

	uint8_t data[6];
	if (isrl78) {
		data[0] = (start >>  0) & 0xff;
		data[1] = (start >>  8) & 0xff;
		data[2] = (start >> 16) & 0xff;
		data[3] = (end   >>  0) & 0xff;
		data[4] = (end   >>  8) & 0xff;
		data[5] = (end   >> 16) & 0xff;
	} else {
		data[0] = (start >> 16) & 0xff;
		data[1] = (start >>  8) & 0xff;
		data[2] = (start >>  0) & 0xff;
		data[3] = (end   >> 16) & 0xff;
		data[4] = (end   >>  8) & 0xff;
		data[5] = (end   >>  0) & 0xff;
	}

	enum tool78_stat st = tool78_cmd_send(hw, tool78_cmd_programming, 6, data);
	if (st != tool78_stat_ack) return st;

	// MODIFIED: 3DS MCU needs a little more time apparently
	//           doubled the timeout
	st = tool78_wait_status(hw, 1, tWT3 * 2); // aka tCS5
	if (st != tool78_stat_ack) return st;

	size_t nbytes = end - start + 1;
	int nblocks = ((nbytes + 255) / 256);
	for (size_t off = 0; off < nbytes; off += 256) {
		busy_wait_us_32(tSD5); // aka tFD3

		size_t todo = nbytes - off;
		if (todo > 256) todo = 256;
		bool finalblk = off + 256 >= nbytes;
		//printf("prgmdata@%zu = %02x %02x %02x ...\n", off, src[off], src[off+1], src[off+2]);
		st = tool78_data_send(hw, todo, &src[off], finalblk);
		if (st != tool78_stat_ack) return st;

		st = tool78_wait_status(hw, 2, tWT4*10); // aka tDS5
		printf("\r  prgm 0x%lx st=%02x %02x fin=%c            ", off, st, databuf[1], finalblk?'t':'f');
		if (finalblk) {
			printf("\r\n");
		}
		if (st != tool78_stat_ack) return st;
		st = databuf[1];
		if (st != tool78_stat_ack) return st;
	}

	st = tool78_wait_status(hw, 1, isrl78 ? (tSS5*10) : (tWT5 * nblocks));
	if (st != tool78_stat_ack) return st;

	return st;
}

enum tool78_stat tool78_do_verify(struct tool78_hw* hw, uint32_t start,
		uint32_t end, const uint8_t* src) {
	if (start >= end) return 0;
	bool isrl78 = (hw->target & tool78_mcu_mask) == tool78_mcu_rl78;

	uint8_t data[6];
	if (isrl78) {
		data[0] = (start >>  0) & 0xff;
		data[1] = (start >>  8) & 0xff;
		data[2] = (start >> 16) & 0xff;
		data[3] = (end   >>  0) & 0xff;
		data[4] = (end   >>  8) & 0xff;
		data[5] = (end   >> 16) & 0xff;
	} else {
		data[0] = (start >> 16) & 0xff;
		data[1] = (start >>  8) & 0xff;
		data[2] = (start >>  0) & 0xff;
		data[3] = (end   >> 16) & 0xff;
		data[4] = (end   >>  8) & 0xff;
		data[5] = (end   >>  0) & 0xff;
	}

	//printf("  verify: %02x %02x %02x %02x %02x %02x\n", data[0], data[1], data[2], data[3], data[4], data[5]);
	enum tool78_stat st = tool78_cmd_send(hw, tool78_cmd_verify, 6, data);
	if (st != tool78_stat_ack) return st;

	// MODIFIED: 3DS MCU needs a little more time apparently
	//           doubled the timeout
	st = tool78_wait_status(hw, 1, tWT6 * 2); // aka tCS2
	//printf("  start verify st: %d\n", st);
	if (st != tool78_stat_ack) return st;

	size_t nbytes = end - start + 1;
	//int nblocks = ((nbytes + 255) / 256);
	for (size_t off = 0; off < end - start; off += 256) {
		busy_wait_us_32(tSD2); // aka tFD3

		size_t todo = nbytes - off;
		if (todo > 256) todo = 256;
		bool finalblk = off + 256 >= nbytes;
		printf("\r  verify blk 0x%lx... (final: %c) ", start+off, finalblk?'Y':'n');
		st = tool78_data_send(hw, todo, &src[off], finalblk);
		if (st != tool78_stat_ack) {
			printf("\r\n");
			return st;
		}

		// MODIFIED: 3DS MCU needs a little more time apparently
		//           doubled the timeout
		st = tool78_wait_status(hw, 2, tWT7 * 2); // aka tDS2
		printf("res: 0x%lx %d %d               ", start+off, st, databuf[1]);
		if (st != tool78_stat_ack || (st = databuf[1]) != tool78_stat_ack) {
			printf("\r\n");
			return st;
		}

		if (finalblk) {
			printf("\r\n");
		}
	}

	return st;
}

enum tool78_stat tool78_do_block_blank_check(struct tool78_hw* hw,
		uint32_t start, uint32_t end, bool check_flash_opt /* RL78 only */) {
	if (start >= end) return 0;
	bool isrl78 = (hw->target & tool78_mcu_mask) == tool78_mcu_rl78;

	uint8_t data[7];
	if (isrl78) {
		data[0] = (start >>  0) & 0xff;
		data[1] = (start >>  8) & 0xff;
		data[2] = (start >> 16) & 0xff;
		data[3] = (end   >>  0) & 0xff;
		data[4] = (end   >>  8) & 0xff;
		data[5] = (end   >> 16) & 0xff;
		data[6] = check_flash_opt ? 1 : 0; // RL78 only
	} else {
		data[0] = (start >> 16) & 0xff;
		data[1] = (start >>  8) & 0xff;
		data[2] = (start >>  0) & 0xff;
		data[3] = (end   >> 16) & 0xff;
		data[4] = (end   >>  8) & 0xff;
		data[5] = (end   >>  0) & 0xff;
	}

	size_t nbytes = end - start + 1;
	int nblocks = ((nbytes + 255) / 256);
	enum tool78_stat st = tool78_cmd_send(hw, tool78_cmd_block_blank_check,
			isrl78 ? 7 : 6, data);
	if (st != tool78_stat_ack) return st;

	st = tool78_wait_status(hw, 1, isrl78 ? tCS4 : (tWT8 * nblocks*100));
	if (st != tool78_stat_ack) return st;

	return st;
}

static bool check_parity(uint8_t v) {
	uint8_t parity = (v >> 7) & 1;
	uint8_t calc = 1; // odd parity
	for (size_t i = 0; i < 7; ++i) calc ^= (v >> i) & 1;

	return calc == parity;
}

enum tool78_stat tool78_do_silicon_signature(struct tool78_hw* hw,
		tool78_silicon_sig_t* sig_dest) {
	busy_wait_us_32(tWT10*10);

	int nbyte = 0;
	switch (hw->target & tool78_mcu_mask) {
	case tool78_mcu_78k0: nbyte = 19; break;
	case tool78_mcu_78k0r: nbyte = -1; break;
	case tool78_mcu_rl78: nbyte = 22; break;
	default: // oops
		return tool78_stat_bad_mcu_for_cmd;
	}

	/*gpio_set_function(18, GPIO_FUNC_SIO);
	gpio_set_dir(18, GPIO_OUT);
	gpio_put(18, true);*/

	memset(sig_dest, 0, 27);
	enum tool78_stat st = tool78_cmd_send(hw, tool78_cmd_silicon_signature, 0, NULL);
	//printf("sent silisig: %02x\n", st);
	if (st != tool78_stat_ack) return st;

	//gpio_put(18, false);
	st = tool78_wait_status(hw, 1, tWT11*2); // aka tCS11
	//gpio_put(18, true);
	//printf("silisig: status 0x%02x\n", st);
	if (st != tool78_stat_ack) return st;

	//gpio_put(18, false);
	int len_recv = -1;
	st = tool78_data_recv_ex(hw, (uint8_t*)sig_dest, nbyte, tFD2, &len_recv); // aka tSD11
	if (nbyte == -1 && hw->target_detail == 0 && len_recv > 0) {
		// assuming 78k0r here
		switch (len_recv) {
		case 27:
			hw->target_detail = tool78_target_78k0r_kx3l;
			break;
		case 24:
			hw->target_detail = tool78_target_78k0r_kx3;
			break;
		default:
			printf("huh? silisig len for 78k0r is %d, unknown target!\n", len_recv);
			break;
		}
	}
	//gpio_put(18, true);
	//printf("silisig: data 0x%02x\n", st);
	if (st != tool78_stat_ack) return st;

	switch (hw->target & tool78_mcu_mask) {
	case tool78_mcu_78k0: nbyte = 18; break;
	case tool78_mcu_78k0r: nbyte = (hw->target_detail == tool78_target_78k0r_kx3l) ? 6 : 5; break;
	case tool78_mcu_rl78: nbyte = 0; break;
	}

	for (int i = 0; i < nbyte; ++i) {
		if (!check_parity(((const uint8_t*)sig_dest)[i]))
			return tool78_stat_parity_error;
	}

	return st;
}

enum tool78_stat tool78_do_version_get(struct tool78_hw* hw, tool78_version_t* vout) {
	if ((hw->target & tool78_mcu_mask) == tool78_mcu_rl78)
		return tool78_stat_bad_mcu_for_cmd;

	enum tool78_stat st = tool78_cmd_send(hw, tool78_cmd_version_get, 0, NULL);
	if (st != tool78_stat_ack) return st;

	st = tool78_wait_status(hw, 1, tWT12);
	if (st != tool78_stat_ack) return st;

	memset(vout, 0, 6);
	st = tool78_data_recv(hw, (uint8_t*)vout, 6, tFD2);
	if (st != tool78_stat_ack) return st;

	return st;
}

enum tool78_stat tool78_do_checksum(struct tool78_hw* hw, uint32_t start,
		uint32_t end, uint16_t* ckout) {
	if (end <= start) return 0;
	bool isrl78 = (hw->target & tool78_mcu_mask) == tool78_mcu_rl78;

	uint8_t data[6];
	if (isrl78) {
		data[0] = (start >>  0) & 0xff;
		data[1] = (start >>  8) & 0xff;
		data[2] = (start >> 16) & 0xff;
		data[3] = (end   >>  0) & 0xff;
		data[4] = (end   >>  8) & 0xff;
		data[5] = (end   >> 16) & 0xff;
	} else {
		data[0] = (start >> 16) & 0xff;
		data[1] = (start >>  8) & 0xff;
		data[2] = (start >>  0) & 0xff;
		data[3] = (end   >> 16) & 0xff;
		data[4] = (end   >>  8) & 0xff;
		data[5] = (end   >>  0) & 0xff;
	}

	enum tool78_stat st = tool78_cmd_send(hw, tool78_cmd_checksum, 6, data);
	if (st != tool78_stat_ack) return st;

	st = tool78_wait_status(hw, 1, tWT16); // aka tCS10
	if (st != tool78_stat_ack) return st;

	*ckout = 0;
	st = tool78_data_recv(hw, (uint8_t*)ckout, 2, tFD1); // aka tSD10
	if (st != tool78_stat_ack) return st;

	return st;
}

enum tool78_stat tool78_do_security_set(struct tool78_hw* hw,
		const struct tool78_security* sec) {
	uint8_t data[8];
	int len;

	// 78k0/78k0r
	data[1] = data[0] = 0;
	bool isrl78 = (hw->target & tool78_mcu_mask) == tool78_mcu_rl78;
	enum tool78_stat st = tool78_cmd_send(hw, tool78_cmd_security_set,
			isrl78 ? 0 : 2, data);
	if (st != tool78_stat_ack) return st;

	st = tool78_wait_status(hw, 1, tWT13); // aka tCS7
	if (st != tool78_stat_ack) return st;

	busy_wait_us_32(tSD7); // aka tFD3
	switch (hw->target & tool78_mcu_mask) {
	case tool78_mcu_78k0:
		data[0] = sec->flg;
		data[1] = 0x03; // 78k0: fixed value
		len = 2;
		st = tool78_data_send(hw, len, data, true);
		break;
	case tool78_mcu_78k0r:
	case tool78_mcu_rl78:
		data[0] = sec->flg;
		data[1] = isrl78 ? sec->bot : 0x03; // 78k0r: fixed value
		// yes they switched around the endianness. sigh.
		if (isrl78) {
			data[2] = (sec->fsws >> 0) & 0xff;
			data[3] = (sec->fsws >> 8) & 0xff;
			data[4] = (sec->fswe >> 0) & 0xff;
			data[5] = (sec->fswe >> 8) & 0xff;
		} else {
			data[2] = (sec->fsws >> 8) & 0xff;
			data[3] = (sec->fsws >> 0) & 0xff;
			data[4] = (sec->fswe >> 8) & 0xff;
			data[5] = (sec->fswe >> 0) & 0xff;
		}
		data[7] = data[6] = 0xff; // fixed/reserved

		if (isrl78 && hw->target_detail == tool78_target_rl78_g2x) {
			data[0] |= 0xe9;
			data[1] = sec->flg2 | 0xfa;
			len = 2;
		} else if (isrl78) {
			len = 8; // also F2x
		} else if (hw->target_detail == tool78_target_78k0r_kx3) {
			len = 6;
		} else if (hw->target_detail == tool78_target_78k0r_kx3l) {
			len = 6; // FIXME: or 8?? (doc erratum? says 6 but depicts 8)
		} else {
			st = tool78_stat_bad_mcu_for_cmd;
			len = 0;
		}
		if (len != 0) st = tool78_data_send(hw, len, data, true);
		break;
	default: st = tool78_stat_bad_mcu_for_cmd;
	}
	if (st != tool78_stat_ack) return st;

	st = tool78_wait_status(hw, 1, tWT14); // aka tDS7
	if (st != tool78_stat_ack) return st;

	if (!isrl78) { // yeah...
		st = tool78_wait_status(hw, 1, tWT15);
		if (st != tool78_stat_ack) return st;
	}

	return st;
}

enum tool78_stat tool78_do_security_get(struct tool78_hw* hw,
		struct tool78_security* sec) {
	if ((hw->target & tool78_mcu_mask) != tool78_mcu_rl78)
		return tool78_stat_bad_mcu_for_cmd;

	memset(sec, 0, sizeof *sec);
	uint8_t data[8];
	memset(data, 0, sizeof data);

	enum tool78_stat st = tool78_cmd_send(hw, tool78_cmd_security_get, 0, NULL);
	if (st != tool78_stat_ack) return st;

	st = tool78_wait_status(hw, 1, tCS8);
	if (st != tool78_stat_ack) return st;

	int len_real;
	st = tool78_data_recv_ex(hw, data, -1, tSD8, &len_real);
	if (st != tool78_stat_ack) return st;

	// TODO: check returned length with target_detail?
	if (len_real == 8) {
		// everything up to RL78/G23, also RL78/F2x
		sec->flg = data[0];
		sec->bot = data[1];
		sec->fsws = data[2] | ((uint16_t)data[3]<<8);
		sec->fswe = data[4] | ((uint16_t)data[5]<<8);
	} else {
		// RL78/G23
		sec->flg = data[0];
		sec->flg2 = data[1];
		sec->bot = data[2];
	}

	return st;
}

enum tool78_stat tool78_do_security_release(struct tool78_hw* hw) {
	// basically does the same as chip erase... oh well
	if ((hw->target & tool78_mcu_mask) != tool78_mcu_rl78)
		return tool78_stat_bad_mcu_for_cmd;

	enum tool78_stat st = tool78_cmd_send(hw, tool78_cmd_security_release, 0, NULL);
	if (st != tool78_stat_ack) return st;

	st = tool78_wait_status(hw, 1, tCS9);
	if (st != tool78_stat_ack) return st;

	// TODO: needs a modeset/re-entry sequence now?

	return st;
}

enum tool78_stat tool78_do_security_idauth(struct tool78_hw* hw,
		const uint8_t passwd[static 10]) {
	if ((hw->target & tool78_mcu_mask) != tool78_mcu_rl78)
		return tool78_stat_bad_mcu_for_cmd;
	if (hw->target_detail != tool78_target_rl78_g2x || hw->target_detail != tool78_target_rl78_f2x)
		return tool78_stat_bad_mcu_for_cmd;

	enum tool78_stat st = tool78_cmd_send(hw, tool78_cmd_security_idauth, 10, passwd);
	if (st != tool78_stat_ack) return st;

	st = tool78_wait_status(hw, 1, tG23);
	if (st != tool78_stat_ack) return st;

	return st;
}

enum tool78_stat tool78_do_exopt_set(struct tool78_hw* hw,
		const uint8_t exopt[static 14]) {
	if ((hw->target & tool78_mcu_mask) != tool78_mcu_rl78)
		return tool78_stat_bad_mcu_for_cmd;
	if (hw->target_detail != tool78_target_rl78_g2x)
		return tool78_stat_bad_mcu_for_cmd;

	enum tool78_stat st = tool78_cmd_send(hw, tool78_cmd_exopts_set, 14, exopt);
	if (st != tool78_stat_ack) return st;

	st = tool78_wait_status(hw, 1, tG23);
	if (st != tool78_stat_ack) return st;

	return st;
}

enum tool78_stat tool78_do_flash_rdp_set(struct tool78_hw* hw,
		uint16_t block_start, uint16_t block_end, bool rdp_rw_allow) {
	if ((hw->target & tool78_mcu_mask) != tool78_mcu_rl78)
		return tool78_stat_bad_mcu_for_cmd;
	if (hw->target_detail != tool78_target_rl78_g2x)
		return tool78_stat_bad_mcu_for_cmd;

	uint8_t data[4];
	data[0] = block_start & 0xff;
	data[1] = (block_start >> 8) | 0xfe;
	data[2] = block_end & 0xff;
	data[3] = ((block_end >> 8) & 1) | 0x7e | (rdp_rw_allow ? 0x80 : 0);

	enum tool78_stat st = tool78_cmd_send(hw, tool78_cmd_flash_rdp_set, 4, data);
	if (st != tool78_stat_ack) return st;

	st = tool78_wait_status(hw, 1, tG23);
	if (st != tool78_stat_ack) return st;

	return st;
}

enum tool78_stat tool78_do_fsw_set(struct tool78_hw* hw,
		const struct tool78_fsw_settings* fswopt) {
	if ((hw->target & tool78_mcu_mask) != tool78_mcu_rl78)
		return tool78_stat_bad_mcu_for_cmd;
	if (hw->target_detail != tool78_target_rl78_g2x)
		return tool78_stat_bad_mcu_for_cmd;

	uint8_t data[4];
	data[0] = fswopt->block_start & 0xff;
	data[1] = ((fswopt->block_start >> 8) & 1) | 0x7e
		| (fswopt->fswopt_rw_allow ? 0x80 : 0);
	data[2] = fswopt->block_end & 0xff;
	data[3] = ((fswopt->block_end >> 8) & 1) | 0x7e
		| (fswopt->fsw_area_invert ? 0x80 : 0);

	enum tool78_stat st = tool78_cmd_send(hw, tool78_cmd_fsw_set, 4, data);
	if (st != tool78_stat_ack) return st;

	st = tool78_wait_status(hw, 1, tG23);
	if (st != tool78_stat_ack) return st;

	return st;
}

enum tool78_stat tool78_do_fsw_get(struct tool78_hw* hw,
		struct tool78_fsw_settings* fswopt) {
	bool isrl78 = (hw->target & tool78_mcu_mask) == tool78_mcu_rl78;
	if (!isrl78)
		return tool78_stat_bad_mcu_for_cmd;
	if (hw->target_detail != tool78_target_rl78_g2x)
		return tool78_stat_bad_mcu_for_cmd;

	uint8_t data[4];

	enum tool78_stat st = tool78_cmd_send(hw, tool78_cmd_fsw_get, 0, NULL);
	if (st != tool78_stat_ack) return st;

	st = tool78_wait_status(hw, 1, tG23);
	if (st != tool78_stat_ack) return st;

	st = tool78_data_recv(hw, data, 4, tG23);
	if (st != tool78_stat_ack) return st;

	fswopt->block_start = data[0] | ((data[1] & 1) << 8);
	fswopt->block_end   = data[2] | ((data[3] & 1) << 8);
	fswopt->fswopt_rw_allow = data[1] & 0x80;
	fswopt->fsw_area_invert = data[3] & 0x80;

	return st;
}

// ----

enum tool78_stat tool78_do_g10_get_flash_size(struct tool78_hw* hw,
		enum tool78_g10_flash_size* fsz) {
	busy_wait_us_32(tDTR);

	// start crc calc cmd
	uint8_t data[3];
	data[0] = tool78_cmd_crc_check;
	int rr = hw->send(1, data, -1);
	if (rr != 1) return tool78_stat_internal_error;

	data[1] = data[2] = 0;
	rr = hw->recv(2, data, (tDT+tDRT)*100); // cmd is echoed back
	//printf("gfsz rr=%d data=%02x %02x %02x\n", rr, data[0], data[1], data[2]);
	if (rr != 2) return tool78_stat_timeout_error;
	if (data[0] != tool78_stat_ack) return data[0];

	*fsz = data[1];

	busy_wait_us_32(tDTR);
	data[0] = tool78_stat_nak; // should send ack here, but send nak to stop crc calc
	rr = hw->send(1, data, -1);
	//printf("send nak: rr=%d data=%02x\n", rr, data[0]);
	if (rr != 1) return tool78_stat_internal_error;

	rr = hw->recv(2, data, tCRC);
	//printf("recv rr=%d data=%02x %02x %02x\n", rr, data[0], data[1], data[2]);
	// should answer with 0x04 but let's not care here

	return tool78_stat_ack;
}
enum tool78_stat tool78_do_g10_check_crc(struct tool78_hw* hw,
		enum tool78_g10_flash_size fsz, uint16_t* crc) {
	busy_wait_us_32(tDTR);

	uint8_t data[3];
	data[0] = tool78_cmd_crc_check;
	int rr = hw->send(1, data, -1);
	if (rr != 1) return tool78_stat_internal_error;

	rr = hw->recv(2, data, (tDT+tDRT)*100);
	if (rr != 2) return tool78_stat_timeout_error;
	if (data[0] != tool78_stat_ack) return data[0];

	busy_wait_us_32(tDTR);
	if (data[1] != fsz) {
		data[0] = tool78_stat_nak; // should send ack here, but send nak to stop crc calc
		rr = hw->send(1, data, -1);
		if (rr != 1) return tool78_stat_internal_error;

		rr = hw->recv(2, data, tCRC);
		// should answer with 0x04 but let's not care here

		return tool78_stat_bad_flash_size;
	} else {
		data[0] = tool78_stat_ack;
		rr = hw->send(1, data, -1);
		if (rr != 1) return tool78_stat_internal_error;

		rr = hw->recv(1, data, tCRC*2);
		if (rr != 1) return tool78_stat_timeout_error;
		if (data[0] == 0xff) { // framing error bullshit
			rr = hw->recv(1, &data[0], tCRC);
			if (rr != 1) return tool78_stat_timeout_error;
		}
		//printf("recv1 rr=%d data=%02x %02x\n", rr, data[0], data[1]);
		enum tool78_stat st = data[0];
		if (st != tool78_stat_ack) return st;

		rr = hw->recv(3, data, tDT*1000);
		//printf("recv2 rr=%d data=%02x %02x %02x\n", rr, data[0], data[1], data[2]);
		if (rr != 3) return tool78_stat_timeout_error;
		if (data[0] == 0xff) {
			data[0] = data[1];
			rr = hw->recv(1, &data[1], tDT*500);
			if (rr != 1) return tool78_stat_timeout_error;
		}
		//printf("recv2 rr=%d data=%02x %02x %02x\n", rr, data[0], data[1], data[2]);

		*crc = data[1] | ((uint16_t)data[2] << 8);

		return tool78_stat_ack;
	}
}
enum tool78_stat tool78_do_g10_erase_then_write(struct tool78_hw* hw,
		enum tool78_g10_flash_size fsz, const uint8_t* data_to_wr, size_t datalen) {
	busy_wait_us_32(tDTR);

	uint8_t data[5];
	data[0] = tool78_cmd_write_after_erase;
	int rr = hw->send(1, data, -1);
	if (rr != 1) return tool78_stat_internal_error;

	rr = hw->recv(3, data, (tDT+tDRT)*100);
	printf("recv1 rr=%d data=%02x %02x %02x\n", rr, data[0], data[1], data[2]);
	if (rr != 3) return tool78_stat_timeout_error;
	if (data[1] != tool78_stat_ack) return data[1];

	busy_wait_us_32(tDTR);
	if (data[2] != fsz) {
		data[0] = tool78_stat_nak; // should send ack here, but send nak to stop crc calc
		rr = hw->send(1, data, -1);
		if (rr != 1) return tool78_stat_internal_error;

		rr = hw->recv(3, data, tERA);
		// should answer with 0x04 but let's not care here

		return tool78_stat_bad_flash_size;
	} else {
		data[0] = tool78_stat_ack;
		rr = hw->send(1, data, -1);
		if (rr != 1) return tool78_stat_internal_error;

		rr = hw->recv(2, data, tERA);
		if (rr != 2) return tool78_stat_timeout_error;
		if (data[0] == 0xff) { // framing error bullshit
			data[0] = data[1];
			rr = hw->recv(1, &data[1], tERA);
			if (rr != 1) return tool78_stat_timeout_error;
		}
		printf("recv2 rr=%d data=%02x %02x\n", rr, data[0], data[1]);
		enum tool78_stat st = data[1];
		if (st != tool78_stat_ack) return st;

		size_t flash_sz = (fsz + 1) * 256;
		for (size_t i = 0; i < flash_sz; i += 4) {
			data[0] = ((i+0) < datalen) ? data_to_wr[i+0] : 0xff;
			data[1] = ((i+1) < datalen) ? data_to_wr[i+1] : 0xff;
			data[2] = ((i+2) < datalen) ? data_to_wr[i+2] : 0xff;
			data[3] = ((i+3) < datalen) ? data_to_wr[i+3] : 0xff;

			busy_wait_us_32(tDTR);
			rr = hw->send(1, &data[0], -1);
			if (rr != 1) return tool78_stat_internal_error;
			busy_wait_us_32(tDRT*20);
			rr = hw->send(1, &data[1], -1);
			if (rr != 1) return tool78_stat_internal_error;
			busy_wait_us_32(tDRT*20);
			rr = hw->send(1, &data[2], -1);
			if (rr != 1) return tool78_stat_internal_error;
			busy_wait_us_32(tDRT*20);
			rr = hw->send(1, &data[3], -1);
			if (rr != 1) return tool78_stat_internal_error;

			rr = hw->recv(5, data, tPRO);
			printf("recv3 i=%04zx rr=%d data=%02x %02x %02x %02x %02x\n", i, rr, data[0],
					data[1], data[2], data[3], data[4]);
			if (rr != 5) return tool78_stat_timeout_error;
			st = data[4];
			if (st != tool78_stat_ack) return st;
		}

		rr = hw->recv(2, data, tVER*100);
		printf("recv4 rr=%d data=%02x %02x\n", rr, data[0], data[1]);
		if (rr != 2) return tool78_stat_timeout_error;
		st = data[1];

		return st;
	}
}
// ----

int tool78_ocd_version(struct tool78_hw* hw, uint16_t* ver) {
	bool is_g10 = (hw->target & tool78_mcu_mask) == tool78_mcu_rl78g10;
	if (is_g10) return -5;

	busy_wait_ms(1*25);

	uint8_t bytes[3];
	bytes[0] = tool78ocd_cmd_version;
	int rr = hw->send(1, &bytes[0], -1);

	rr = hw->recv(2, bytes, 1000*10);
	if (rr != 2) {
		printf("OCDver rr: %d\n", rr);
		/*for (int iii = 0; iii < rr; ++iii) {
			printf("0x%02x%c", bytes[iii], (iii==rr-1)?'\n':' ');
		}*/
		return -1;
	}

	// BCD, so big-endian
	*ver = bytes[1] | ((uint16_t)bytes[0] << 8);

	return 0;
}
int tool78_ocd_connect(struct tool78_hw* hw, const uint8_t passwd[10]) {
	busy_wait_ms(1);
	uint8_t stuff[11];
	uint8_t connst;
	bool is_g10 = (hw->target & tool78_mcu_mask) == tool78_mcu_rl78g10;
	stuff[0] = is_g10 ? tool78ocd_cmdg10_connect : tool78ocd_cmd_connect;
	int rr = hw->send(1, &stuff[0], -1);

	rr = hw->recv(1, &stuff[1], 1000);
	if (rr != 1) {
		printf("OCDconn rr: %d\n", rr);
		/*for (int iii = 0; iii < rr; ++iii) {
			printf("0x%02x%c", stuff[1+iii], (iii==rr-1)?'\n':' ');
		}*/
		return -1;
	}
	connst=stuff[1];
	//printf("connst 1=%02x\n", connst);
	if (connst == 0xff) {
		rr = hw->recv(1, &connst, 1000);
		if (rr != 1) return -1;
	}
	//printf("connst 2=%02x\n", connst);
	if (connst == 0xf0 || connst == 0xf4) return connst;
	// connst==0xf1

	memcpy(stuff, passwd, 10);
	if (!is_g10) {
		stuff[10] = tool78_calc_ocd_checksum8(10, passwd);
	}

	busy_wait_us_32(100);
	rr = hw->send(is_g10 ? 10 : 11, stuff, -1);

	// stuff[9] is the checksum byte echoed back
	rr = hw->recv(1, &stuff[9], 100000);
	//printf("ocdpw 2 = %02x %02x\n", stuff[9], stuff[10]);
	if (rr != 1) {
		printf("OCDconn2 rr: %d\n", rr);
		/*for (int iii = 0; iii < rr; ++iii) {
			printf("0x%02x%c", stuff[9+iii], (iii==rr-1)?'\n':' ');
		}*/
		return -2;
	}
	if (stuff[9] == 0xff && is_g10) { // framing error bullshit
		rr = hw->recv(1, &stuff[9], 100000);
		if (rr != 1) return -2;
	}
	//printf("ocdpw 3 = %02x %02x\n", stuff[9], stuff[10]);
	while (stuff[9] == 0) {
		rr = hw->recv(1, &stuff[9], 100000);
		if (rr != 1) return -2;
	}
	//printf("ocdpw 4 = %02x %02x\n", stuff[9], stuff[10]);

	/*printf("cksum=0x%02x conn=%02x pw=%02x\n",
			cksum, connst, stuff[9]);*/

	//printf("connst=%u stuff=%u\n", connst, stuff[9]);
	return stuff[9];
}
int tool78_ocd_read(struct tool78_hw* hw, uint16_t off, uint8_t len,
		uint8_t* data) {
	busy_wait_ms(1);

	bool is_g10 = (hw->target & tool78_mcu_mask) == tool78_mcu_rl78g10;

	uint8_t hdr[4];
	int rr;

	if (is_g10) {
		if (len == 2) {
			hdr[0] = tool78ocd_cmdg10_setptr;
			hdr[1] = off & 0xff; // l
			hdr[2] = off >> 8; // h
			hdr[3] = 1; // b
			rr = hw->send(sizeof hdr, hdr, -1);
			if (rr != sizeof hdr) return -1;

			/*// command echoed back
			rr = hw->recv(4, hdr, 1000);
			if (rr != 4) return -2;
			if (hdr[0] == 0xff) { // frame error bullshit
				hdr[0] = hdr[1];
				hdr[1] = hdr[2];
				hdr[2] = hdr[3];
				rr = hw->recv(1, &hdr[3], 1000);
				if (rr != 1) return -2;
			}*/

			// status
			rr = hw->recv(1, hdr, 1000);
			if (rr != 1) return -1;
			if (hdr[0] != 0x00) return -3;

			busy_wait_us_32(100);
			hdr[0] = tool78ocd_cmdg10_read_word;
			rr = hw->send(1, hdr, -1);
			if (rr != 1) return -1;

			/*rr = hw->recv(1, hdr, 1000); // command echoed back
			if (rr != 1) return -1;*/

			rr = hw->recv(2, data, 1000*len);
			if (rr != 2) return -2;
			return 0;
		} else {
			hdr[0] = tool78ocd_cmdg10_setptr;
			hdr[1] = off & 0xff; // l
			hdr[2] = off >> 8; // h
			hdr[3] = len; // b
			rr = hw->send(sizeof hdr, hdr, -1);
			if (rr != sizeof hdr) return -1;
			//printf("sent hdr\n");

			/*rr = hw->recv(4, hdr, 1000);
			//printf("stat rr=%d hdr=%02x %02x %02x %02x\n", rr, hdr[0], hdr[1], hdr[2], hdr[3]);
			if (rr != 4) return -2;
			if (hdr[0] == 0xff) { // frame error bullshit
				hdr[0] = hdr[1];
				hdr[1] = hdr[2];
				hdr[2] = hdr[3];
				rr = hw->recv(1, &hdr[3], 1000);
				if (rr != 1) return -2;
			}*/
			rr = hw->recv(1, hdr, 1000);
			//printf("stat rr=%d hdr=%02x\n", rr, hdr[0]);
			if (rr != 1) return -1;
			if (hdr[0] != 0x00) {
				if (hdr[0] == len) {
					rr = hw->recv(1, hdr, 1000);
					//printf("stat 2 rr=%d hdr=%02x\n", rr, hdr[0]);
					if (rr != 1 || hdr[0] != 0x00) return -3;
				} else return -3;
			}

			busy_wait_us_32(100);
			hdr[0] = tool78ocd_cmdg10_read_raw;
			rr = hw->send(1, hdr, -1);
			if (rr != 1) return -1;
			//printf("sent rdcmd\n");

			/*rr = hw->recv(1, hdr, 1000); // command echoed back
			//printf("recv dd=%d hdr=%02x\n", rr, hdr[0]);
			if (rr != 1) return -1;*/

			rr = hw->recv(len, data, 1000*len);
			//printf("got data rr=%d data=%02x\n", rr, data[0]);
			if (rr != len) return -2;
			return 0;
		}
	} else {
		hdr[0] = tool78ocd_cmd_read;
		hdr[2] = off >> 8;
		hdr[1] = off & 0xff;
		hdr[3] = len;
		//printf("rd len=%u\n", len);

		int rr = hw->send(sizeof hdr, hdr, -1);
		if (rr != sizeof hdr) return -1;
		/*rr = hw->recv(1, &hdr[2], 1000);
		// last byte of header sent echoed back
		if (rr != 1 || hdr[2] != hdr[3]) {
			//printf("rd recv=%02x\n", hdr[2]);
			return -2;
		}*/

		rr = hw->recv(len, data, 1000*len);
		//printf("ocd read %d\n", rr);
		return (rr == len) ? 0 : -1;
	}
}
int tool78_ocd_write(struct tool78_hw* hw, uint16_t addr, uint8_t len,
		const uint8_t* data) {
	if (len == 0) return -1;
	busy_wait_ms(1);

	bool is_g10 = (hw->target & tool78_mcu_mask) == tool78_mcu_rl78g10;

	uint8_t hdr[4];
	int rr;

	if (is_g10) {
		if (len == 2) {
			hdr[0] = tool78ocd_cmdg10_setptr;
			hdr[1] = addr & 0xff; // l
			hdr[2] = addr >> 8; // h
			hdr[3] = data[1]; // b

			rr = hw->send(sizeof hdr, hdr, -1);
			if (rr != sizeof hdr) return -1;

			/*// command echoed back
			rr = hw->recv(4, hdr, 1000);
			if (rr != 4) return -2;
			if (hdr[0] == 0xff) { // frame error bullshit
				hdr[0] = hdr[1];
				hdr[1] = hdr[2];
				hdr[2] = hdr[3];
				rr = hw->recv(1, &hdr[3], 1000);
				if (rr != 1) return -2;
			}*/

			// status
			rr = hw->recv(1, hdr, 1000);
			if (rr != 1) return -1;
			if (hdr[0] != 0x00) return -3;

			hdr[0] = tool78ocd_cmdg10_write_word;
			hdr[1] = data[0]; // a

			busy_wait_us_32(100);
			rr = hw->send(2, hdr, -1);
			if (rr != 2) return -1;

			rr = hw->recv(1, hdr, 1000);
			if (rr != 1) return -1;

			return (hdr[0] == 0) ? 0 : -2;
		} else {
			hdr[0] = tool78ocd_cmdg10_setptr;
			hdr[1] = addr & 0xff; // l
			hdr[2] = addr >> 8; // h
			hdr[3] = len; // b

			rr = hw->send(sizeof hdr, hdr, -1);
			if (rr != sizeof hdr) return -1;

			// command echoed back
			/*rr = hw->recv(4, hdr, 1000);
			//printf("wr p1: rr=%d %02x %02x %02x %02x\n", rr, hdr[0],
			//		hdr[1], hdr[2], hdr[3]);
			if (rr != 4) return -2;
			if (hdr[0] == 0xff) { // frame error bullshit
				hdr[0] = hdr[1];
				hdr[1] = hdr[2];
				hdr[2] = hdr[3];
				rr = hw->recv(1, &hdr[3], 1000);
				if (rr != 1) return -2;
			}*/
			//printf("wr p1: rr=%d %02x %02x %02x %02x\n", rr, hdr[0],
			//		hdr[1], hdr[2], hdr[3]);

			// status
			rr = hw->recv(1, hdr, 1000);
			//printf("wr p2: rr=%d %02x %02x %02x %02x\n", rr, hdr[0],
			//		hdr[1], hdr[2], hdr[3]);
			if (rr != 1) return -1;
			if (hdr[0] == len) {
				rr = hw->recv(1, hdr, 1000);
				//printf("wr p2: rr=%d %02x %02x %02x %02x\n", rr, hdr[0],
				//		hdr[1], hdr[2], hdr[3]);
				if (rr != 1) return -1;
			}
			if (hdr[0] != 0x00) return -3;

			hdr[0] = tool78ocd_cmdg10_write_raw;

			busy_wait_us_32(100);
			rr = hw->send(1, hdr, -1);
			if (rr != 1) return -1;
			/*rr = hw->recv(1, hdr, 1000); // sigh
			//printf("wr p3: rr=%d %02x %02x %02x %02x\n", rr, hdr[0],
			//		hdr[1], hdr[2], hdr[3]);
			if (rr != 1) return -1;*/

			for (int i = 0; i < len; i += 8) {
				int todo = len - i;
				if (todo > 8) todo = 8;

				busy_wait_us_32(100);
				rr = hw->send(todo, &data[i], -1);
				if (rr != todo) return -1;
				/*rr = hw->recv(todo, NULL, 1000*todo); // sigh
				//printf("wr p4: rr=%d %02x %02x %02x %02x\n", rr, hdr[0],
				//		hdr[1], hdr[2], hdr[3]);
				if (rr != todo) return -1;*/
			}

			rr = hw->recv(1, hdr, 1000);
			//printf("wr p5: rr=%d %02x %02x %02x %02x\n", rr, hdr[0],
			//		hdr[1], hdr[2], hdr[3]);
			//printf("recv %d: %02x %02x\n", rr, hdr[0], hdr[1]);
			if (rr != 1) return -1;

			return (hdr[0] == 0) ? 0 : -2;
		}
	} else {
		hdr[0] = tool78ocd_cmd_write;
		hdr[1] = addr & 0xff;
		hdr[2] = addr >> 8;
		hdr[3] = len;

		int rr = hw->send(sizeof hdr, hdr, -1);
		if (rr != sizeof hdr) return -1;
		rr = hw->send(len, data, -1);
		if (rr != len) return -1;

		// last data byte echoed back
		rr = hw->recv(1, hdr, 1000*len);
		if (rr != 1) return -1;
		///printf("ocd write 0x%02x\n", hdr[0]);
		return (hdr[0] == tool78ocd_cmd_write) ? 0 : -2;
	}
}
int tool78_ocd_exec(struct tool78_hw* hw) {
	busy_wait_ms(1);

	bool is_g10 = (hw->target & tool78_mcu_mask) == tool78_mcu_rl78g10;

	uint8_t hdr[2], cmd;
	cmd = hdr[0] = is_g10 ? tool78ocd_cmdg10_exec : tool78ocd_cmd_exec;

	int rr = hw->send(1, hdr, -1);

	// G13 etc send status before exec, G10 sends it after
	if (!is_g10) {
		rr = hw->recv(1, hdr, 1000);
		if (rr != 1) return -1;
		//printf("exec r=%d: h[0]=0x%02x, h[1]=0x%02x\n", rr, hdr[0], hdr[1]);

	}
	return 0;//(hdr[0] == cmd) ? 0 : -2;
}
int tool78_ocd_leave(struct tool78_hw* hw, bool exit_to_ram) {
	busy_wait_ms(1);

	bool is_g10 = (hw->target & tool78_mcu_mask) == tool78_mcu_rl78g10;

	uint8_t hdr[2];

	if (is_g10) {
		hdr[0] = tool78ocd_cmdg10_exit_reti;
	} else {
		hdr[0] = exit_to_ram ? tool78ocd_cmd_exit_ram : tool78ocd_cmd_exit_reti;
	}

	int rr = hw->send(1, hdr, -1);

	if (is_g10) {
		rr = hw->recv(1, &hdr[0], 1000);
		if (rr != 1) return -1;
	}

	return 0;
}

// ----

static enum tool78_stat tool78_init_common(struct tool78_hw* hw) {
	if (!hw) return tool78_stat_internal_error;
	if ((hw->target & tool78_mcu_mask) != tool78_mcu_rl78g10) {
		hw->flags &= ~tool78_hw_flag_done_reset;
	} else {
		hw->flags |=  tool78_hw_flag_done_reset;
	}

	enum tool78_stat st = tool78_stat_ack;
	for (size_t i = 0; i < 16; ++i) {
		if (!hw->init()) return tool78_stat_fatal_hw_error;
		//printf("init %zu ok\n", i);

		if ((hw->target & tool78_mcu_mask) != tool78_mcu_rl78
				&& !(hw->flags & tool78_hw_flag_do_ocd)
				&& (hw->target & tool78_mcu_mask) != tool78_mcu_rl78g10) {
			//printf("sending reset cmd\n");
			st = tool78_do_reset(hw);
			//printf("done reset st=0x%02x\n", st);
		}
		if (st == tool78_stat_timeout_error) {
			hw->deinit();
			continue;
		}
		break;
	}
	//printf("hw init stat: %d\n", st);
	if (st != tool78_stat_ack) return st;

	if ((hw->target & tool78_mcu_mask) != tool78_mcu_rl78g10) {
		if (!(hw->flags & tool78_hw_flag_do_ocd)) {
			//printf("silisig normal\n");
			tool78_silicon_sig_t sigtmp;
			st = tool78_do_silicon_signature(hw, &sigtmp);
			if (st != tool78_stat_ack) return st;

			switch (hw->target & tool78_mcu_mask) {
			case tool78_mcu_78k0:
				// TODO: Ix2 and Kx2-L support
				if (sigtmp.k780.met != 0x7f || sigtmp.k780.msc != 0x04) {
					printf("TODO: not a 78K0-Kx2! support needs to be implemented...\n");
					return 0;
				}
				hw->target_detail = tool78_target_78k0_kx2;
				break;
			case tool78_mcu_78k0r:
				// ok so here it's super fucking annoying. we could read the
				// device name BUT the offset of that field also varies by
				// subtype... so let's try something else instead
				/*if ((((uint32_t)sigtmp.k780r_kx3.uae[0] + 1) & sigtmp.k780r_kx3.uae[0]) != 0) {
					// if sigtmp.k780r_kx3.uae[0] + 1 isn't a power of two,
					// it's most likely not a real "end address", i.e. actually
					// the 3rd device code for the kx3l/ix3
					sigtmp.k780r_kx3l.dev[9] = 0;
					hw->target_detail = tool78_target_78k0r_kx3l;
					printf("Detected 78K0R/Kx3-L or Ix3: %s\n", sigtmp.k780r_kx3l.dev);
				} else {
					sigtmp.k780r_kx3.dev[9] = 0;
					hw->target_detail = tool78_target_78k0r_kx3;
					printf("Detected 78K0R/Kx3: %s\n", sigtmp.k780r_kx3.dev);
				}*/
				// actually, no, this can be deduced simply from the silicon
				// signature response size, so we set the field there
				if (hw->target_detail == tool78_target_78k0r_kx3) {
					sigtmp.k780r_kx3.dev[9] = 0;
					//printf("Detected 78K0R/Kx3: %s\n", sigtmp.k780r_kx3.dev);
				} else if (hw->target_detail == tool78_target_78k0r_kx3l) {
					sigtmp.k780r_kx3l.dev[9] = 0;
					//printf("Detected 78K0R/Kx3-L or Ix3: %s\n", sigtmp.k780r_kx3l.dev);
				} else {
					printf("WARNING: unknown part detected!\n");
				}
				break;
			case tool78_mcu_rl78:
				// R5F1xx: G1X and compatible (Protocol A)
				// R7F10x: G23 and compatible (Protocol C)
				// R7F12x: F24 and compatible (Protocol D)
				if (!strncmp((const char*)sigtmp.rl78.dev, "R5F1", 4)) {
					sigtmp.rl78.dev[9] = 0;
					hw->target_detail = tool78_target_rl78_g1x;
					printf("Detected RL78/G1x (Protocol A): %s\n", sigtmp.rl78.dev);
				} else if (!strncmp((const char*)sigtmp.rl78.dev, "R7F10", 5)) {
					sigtmp.rl78.dev[9] = 0;
					hw->target_detail = tool78_target_rl78_g2x;
					printf("Detected RL78/G2x (Protocol C): %s\n", sigtmp.rl78.dev);
				} else if (!strncmp((const char*)sigtmp.rl78.dev, "R7F12", 5)) {
					sigtmp.rl78.dev[9] = 0;
					hw->target_detail = tool78_target_rl78_f2x;
					printf("Detected RL78/F2x (Protocol D): %s\n", sigtmp.rl78.dev);
				} else {
					sigtmp.rl78.dev[9] = 0;
					printf("Detected RL78/??? (Protocol ?): %s\n", sigtmp.rl78.dev);
					printf("WARN: unknown part detected! Will assume Protocol A functionality, but things may fail!\n");
				}
				break;
			}
		}

		if ((!(hw->flags & tool78_hw_flag_do_ocd)
				|| (hw->target & tool78_mcu_mask) == tool78_mcu_rl78)
				&& (hw->target & tool78_mcu_mask) != tool78_mcu_78k0) {
			st = tool78_do_generic_baudrate(hw);
			//printf("baudrate result=0x%02x\n", st);
			if (st != tool78_stat_ack) return st;
		}
	} else {
		printf("silisig g10\n");
		// 0x3a/0xc5 is echoed back as well // TODO err why does this happen?
		uint8_t data[5];
		int rr = hw->recv(1, data, 1000);
		//printf("init rr:%d st=%02x\n", rr, st);
		if (rr != 1) return tool78_stat_timeout_error;
		st = data[0];

		if (st == 0x06 && (hw->flags & tool78_hw_flag_do_ocd)) {
			// should be 5 zero bytes, but let's not check
			rr = hw->recv(5, data, 5*1000);
			if (rr != 5) return tool78_stat_no_ocd_status;
		}
	}

	return st;
}

enum tool78_stat tool78_init_sfp(struct tool78_hw* hw, tool78_silicon_sig_t* sig) {
	hw->flags &= ~tool78_hw_flag_do_ocd;

	enum tool78_stat st = tool78_init_common(hw);
	printf("init common %02x\n", st);
	if (st != tool78_stat_ack) return st;

	if ((hw->target & tool78_mcu_mask) != tool78_mcu_rl78g10) {
		st = tool78_do_silicon_signature(hw, sig);
		//printf("sig result=0x%02x\n", st);
		if (st != tool78_stat_ack) return st;
	}

	return st;
}

enum tool78_stat tool78_init_ocd(struct tool78_hw* hw, uint16_t* ver,
		const uint8_t passwd[10]) {
	hw->flags |= tool78_hw_flag_do_ocd;
	int st = tool78_init_common(hw);
	//printf("common st=%d\n", st);
	if (st != tool78_stat_ack) return st;

	if ((hw->target & tool78_mcu_mask) != tool78_mcu_rl78g10) {
		st = tool78_ocd_version(hw, ver);
		//printf("ocdver st=%d\n", st);
		if (st) return st;
	}

	st = tool78_ocd_connect(hw, passwd);
	//printf("conn st=%d\n", st);
	/*uint8_t i = 0;
	do {
		st = tool78_ocd_connect(hw, passwd, i);
		++i;

		if (i == 0) break;
	} while ((st & 0xff00) == 0xf300 && (st & 0xff) == 0xf1);*/

	return st;
}

