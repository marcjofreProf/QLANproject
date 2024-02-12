// PRU-ICSS program to control realtime GPIO pins and allow TimeTagging and Direct Memory Access management
// But only if the Pinmux Mode has been set correctly with a device  
 // tree overlay!  
 //  
 // Assemble in BBB with:  
 // pasm -b PRUassemblerSignalsScript.p
 
.origin 0				// start of program in PRU memory
.entrypoinnt START			// program entry point (for debbuger)

#define INS_PER_US		200	// 5ns per instruction
#define INS_PER_DELAY_LOOP	2	// two instructions per delay loop

#define DELAY 2*1000 * (INS_PER_US / INS_PER_DELAY_LOOP)

SIGNALON:
	set r30, r30, 4
	mov r0, r0, DELAY

DELAYON:
	sub r0, r0, 1
	QBNE DELAYON, r0, 0
	
SIGNALOFF:
	clr r30, r30, 4
	mov r0, r0, DELAY

DELAYOFF:
	sub r0, r0, 1
	QBNE DELAYOFF, r0, 0

