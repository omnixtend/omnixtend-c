#ifndef __TILELINK_MSG_H__
#define __TILELINK_MSG_H__

#include <stdint.h>
#include "util/circular_queue.h"

#define NUM_CHANNEL     5
#define CHANNEL_0       0
#define CHANNEL_A       1
#define CHANNEL_B       2
#define CHANNEL_C       3
#define CHANNEL_D       4
#define CHANNEL_E       5

// Channel A
#define A_PUTFULLDATA_OPCODE    0
#define A_PUTPARTIALDATA_OPCODE 1
#define A_ARITHMETICDATA_OPCODE 2
#define A_LOGICALDATA_OPCODE    3
#define A_GET_OPCODE            4
#define A_INTENT_OPCODE         5
#define A_ACQUIREBLOCK_OPCODE   6
#define A_ACQUIREPERM_OPCODE    7

// Channel B
#define B_PUTFULLDATA_OPCODE    0
#define B_PUTPARTIALDATA_OPCODE 1
#define B_ARITHMETICDATA_OPCODE 2
#define B_LOGICALDATA_OPCODE    3
#define B_GET_OPCODE            4
#define B_INTENT_OPCODE         5
#define B_PROBEBLOCK_OPCODE     6
#define B_PROBEPERM_OPCODE      7

// Channel C
#define C_ACCESSACK_OPCODE      0
#define C_ACCESSACKDATA_OPCODE  1
#define C_HINTACK_OPCODE        2
#define C_PROBEACK_OPCODE       4
#define C_PROBEACKDATA_OPCODE   5
#define C_RELEASE_OPCODE        6
#define C_RELEASEDATA_OPCODE    7

// Channel D
#define D_ACCESSACK_OPCODE      0
#define D_ACCESSACKDATA_OPCODE  1
#define D_HINTACK_OPCODE        2
#define D_GRANT_OPCODE          4
#define D_GRANTDATA_OPCODE      5
#define D_RELEASEACK_OPCODE     6

// Channel E
#define E_GRANTACK              0  // Not required opcode

// TileLink messag with Data and without Mask
typedef struct tilelink_message_struct {
    char header1[8];
    char header2[8];
    char data[0];
	int opcode;
} TileLinkMsg;

// TileLink message with Data and Mask (n <= 6)

// TileLink message with Data and Mask (n >= 6)

// Operations for the TileLink message

#endif // __TILELINK_MSG_H__
