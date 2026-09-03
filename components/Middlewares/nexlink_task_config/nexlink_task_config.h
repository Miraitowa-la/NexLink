#pragma once

/* CPU0 runs USB and wireless control. CPU1 exclusively owns target I/O. */
#define NEXLINK_CPU_WIRELESS 0
#define NEXLINK_CPU_TARGET   1
