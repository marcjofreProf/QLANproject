// PRU-ICSS program to control realtime GPIO pins and allow TimeTagging and Direct Memory Access management
// But only if the Pinmux Mode has been set correctly with a device  
 // tree overlay!  
 //  
 // Assemble in BBB with:  
 // pasm -b PRUassemblerSignalsScript.p
 
.origin 0				// start of program in PRU memory
.entrypoint SIGNALON			// program entry point (for debbuger)

#define INS_PER_US		200	// 5ns per instruction
#define INS_PER_DELAY_LOOP	2	// two instructions per delay loop

#define DELAY 1// * (INS_PER_US / INS_PER_DELAY_LOOP)
#define PRU0_R31_VEC_VALID	32
#define PRU_EVTOUT_0		3	// the event number that is sent back

SIGNALON:
	set r30, r30, 4
	mov r0, DELAY

DELAYON:
	sub r0, r0, 1
	QBNE DELAYON, r0, 0
	
SIGNALOFF:
	clr r30, r30, 4
	mov r0, DELAY

DELAYOFF:
	sub r0, r0, 1
	QBNE DELAYOFF, r0, 0
	jmp SIGNALON

END:
	mov r31.b0, PRU0_R31_VEC_VALID | PRU_EVTOUT_0
	halt

