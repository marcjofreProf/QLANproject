// PRU-ICSS program to operate and allow TimeTagging and Direct Memory Access management

.origin 0				// start of program in PRU memory
.entrypoinnt START			// program entry point (for debbuger)

#define INS_PER_US		200	// 5ns per instruction
#define INS_PER_DELAY_LOOP	2	// two instructions per delay loop


