/*
 * SD/MMC registers and definitions.
 */
#ifndef REG_H
#define REG_H

#include "../mmio.h"

enum sd_register {
	BLKSIZECNT,
	ARG1,
	CMDTM,
	RESP0,
	DATA,
	STATUS,
	CONTROL0,
	CONTROL1,
	INTERRUPT,
	IRPT_MASK,
	IRPT_EN,
	FORCE_IRPT
};

static struct periph_access sd_access = {
	/* 
	 * This offset comes from the BCM2711 RPI 4 B device tree 
	 * EMMC2 node at /emmc2bus/mmc. 
	 */
	.periph_base_off = 0x2340000,
	.register_offsets = {
		[BLKSIZECNT] = 0x04,
		[ARG1]       = 0x08,
		[CMDTM]      = 0x0c,
		[RESP0]      = 0x10,
		[DATA]	     = 0x20,
		[STATUS]     = 0x24,
		[CONTROL0]   = 0x28,
		[CONTROL1]   = 0x2c,
		[INTERRUPT]  = 0x30,
		[IRPT_MASK]  = 0x34,
		[IRPT_EN]    = 0x38,
		[FORCE_IRPT] = 0x50
	}
};

struct blksizecnt {
	bits_t blksize : 10;
	bits_t reserved : 6;
	bits_t blkcnt : 16;
} __attribute__((packed));

struct cmdtm {
	bits_t reserved1 : 1;
	bits_t block_cnt_en : 1;
	enum  {
		CMDTM_TM_AUTO_CMD_EN_NONE  = 0b00,
		CMDTM_TM_AUTO_CMD_EN_CMD12 = 0b01,
		CMDTM_TM_AUTO_CMD_EN_CMD23 = 0b10,
		CMDTM_TM_AUTO_CMD_EN_RESVD = 0b11
	} auto_cmd_en : 2;
	enum {
		CMDTM_TM_DAT_DIR_READ  = 1,
		CMDTM_TM_DAT_DIR_WRITE = 0
	} data_transfer_direction : 1;
	bits_t multi_block : 1;
	bits_t reserved2 : 10;
	enum {
		CMDTM_RESPONSE_TYPE_NONE,
		CMDTM_RESPONSE_TYPE_136_BITS,
		CMDTM_RESPONSE_TYPE_48_BITS,
		CMDTM_RESPONSE_TYPE_48_BITS_BUSY
	} response_type : 2;
	bits_t reserved3 : 1;
	bits_t cmd_crc_chk_en : 1;
	bits_t cmd_index_chk_en : 1;
	bits_t data_present : 1;
	bits_t cmd_type : 2;
	bits_t cmd_index : 6;
	bits_t reserved4 : 2;
} __attribute__((packed));

/* Format of registers INTERRUPT, IRPT_MASK, IRPT_EN, FORCE_IRPT. */
struct interrupt {
	bits_t cmd_complete : 1;
	bits_t transfer_complete : 1;
	bits_t block_gap : 1;
	bits_t reserved1 : 1;
	bits_t write_ready : 1;
	bits_t read_ready : 1;
	bits_t reserved2 : 2;
	bits_t card : 1;
	bits_t reserved3 : 3;
	bits_t retune : 1;
	bits_t bootack : 1;
	bits_t endboot : 1;
/* Errors below. */
	/* The "error" field only applies to the INTERRUPT register. */
	bits_t error : 1;  
	bits_t cmd_timeout_error : 1;
	bits_t cmd_crc_error : 1;
	bits_t cmd_end_bit_error : 1;
	bits_t cmd_index_error : 1;
	bits_t data_timeout_error : 1;
	bits_t data_crc_error : 1;
	bits_t data_end_bit_error : 1;
	bits_t reserved4 : 1;
	bits_t auto_cmd_error : 1;
	bits_t reserved5 : 7;
} __attribute__((packed));

/* Extra masks for the interrupt registers (INTERRUPT, etc.). */
#define INTERRUPT_CMD_COMPLETE       0x01
#define INTERRUPT_TRANSFER_COMPLETE  0x02
#define INTERRUPT_READ_READY         0x20

/* Status register fields. */
#define STATUS_COMMAND_INHIBIT_CMD 0x1
#define STATUS_COMMAND_INHIBIT_DAT 0x2

/* Control 0 register fields. */
#define CONTROL0_DATA_TRANSFER_WIDTH 0x2
/*
 * The BCM2835 datasheet lists the below power control bits as reserved, 
 * but from the SD Host Controller spec they make up the power control 
 * register.
 */
#define CONTROL0_PWR_CTL_SHIFT 8
/* Power control register fields. */
/* Voltage select. */
#define PWR_CTL_SD_BUS_VOLT_SEL_3V3 0b1110
#define PWR_CTL_SD_BUS_POWER        0b0001

/* Control 1 register fields. */
/* Mask for clock enable, internal clock stable, internal clock enable. */
#define CONTROL1_INT_CLK_EN     0x1
#define CONTROL1_INT_CLK_STABLE 0x2
#define CONTROL1_CLK_EN         0x4
/* SD clock frequency select and shift. */
#define CONTROL1_CLK_FREQ_SEL 0xff00
#define CONTROL1_CLK_FREQ_SEL_SHIFT 8
/* Software reset host controller. */
#define CONTROL1_SW_RESET_HC 0x1000000

#endif