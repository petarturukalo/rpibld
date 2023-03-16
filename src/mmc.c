#include "mmc.h"
#include "mmio.h"

enum mmc_registers {
	CMDTM
};

static struct periph_access mmc_access = {
	.periph_base_off = 0x300000,
	.register_offsets = {
		[CMDTM] = 0x0c
	}
};



