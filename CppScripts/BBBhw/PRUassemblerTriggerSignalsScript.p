// PRU-ICSS program to control realtime GPIO pins
// But only if the Pinmux Mode has been set correctly with a device  
 // tree overlay!  
 //  
 // Assemble in BBB with:  
 // pasm -b PRUassemblerSignalsScript.p
 // https://www.ofitselfso.com/BBBCSIO/Help/BBBCSIOHelp_PRUPinInOutExamplePASMCode.html
 
.origin 0				// start of program in PRU memory
.entrypoint INITIATIONS			// program entry point (for debbuger)

#define GPIO_BANK1 0x4804c000 // this is the address of the BBB GPIO Bank1 Register. We set bits in special locations in offsets here to put a GPIO high or low.

#define GPIO_SETDATAOUT 0x194 // at this offset various GPIOs are associated with a bit position. Writing a 32 bit value to this offset enables them (sets them high) if there is a 1 in a corresponding bit. A zero in a bit position here is ignored - it does NOT turn the associated GPIO off.

#define GPIO_CLEARDATAOUT 0x190 //We set a GPIO low by writing to this offset. In the 32 bit value we write, if a bit is 1 the 
// GPIO goes low. If a bit is 0 it is ignored.

#define INS_PER_US		200	// 5ns per instruction fo rBeaglebone black
#define INS_PER_DELAY_LOOP	2	// two instructions per delay loop

#define DELAY 1 * 1000 * (INS_PER_US / INS_PER_DELAY_LOOP) // in milliseconds
#define PRU0_R31_VEC_VALID	32
#define PRU_EVTOUT_0		3	// the event number that is sent back

// Beaglebone Black has 32 bit registers (for instance Beaglebone AI has 64 bits and more than 2 PRU)
#define AllOutputInterestPinsHigh 0x00000FFF// For the defined output pins to set them high in block (and not the ones that are allocated by other processes)

INITIATIONS:
	MOV R1, GPIO_BANK1+GPIO_SETDATAOUT  // load the address to we wish to set to r2. Note that the operation GPIO_BANK1+GPIO_SETDATAOUT is performed by the assembler at compile time and the resulting constant value is used. The addition is NOT done at runtime by the PRU!
	MOV R2, GPIO_BANK1+GPIO_CLEARDATAOUT // load the address we wish to write to r3. Note that every bit that is a 1 will turn off the associated GPIO we do NOT write a 0 to turn it off. 0's are simply ignored.
	MOV R3, AllOutputInterestPinsHigh // load R3 with the LEDs enable/disable bit

SIGNALON:	// for setting just one pin would be set r30, r30, #Bit number	
	SBBO R3, R1, 0, 4 // write the contents of R3 out to the memory address contained in R1. Use no offset from that address and copy all 4 bytes of R2
	mov r0, DELAY

DELAYON:
	sub r0, r0, 1
	QBNE DELAYON, r0, 0
	
SIGNALOFF:      // for clearing just one pin would be clr r31, r31, #Bit number	
	SBBO R3, R2, 0, 4 // write the contents of R3 out to the memory address contained in R2. Use no offset from that address and copy all 4 bytes of R2
	mov r0, DELAY

DELAYOFF:
	sub r0, r0, 1
	QBNE DELAYOFF, r0, 0
	jmp SIGNALON

EXIT:
	mov r31.b0, PRU0_R31_VEC_VALID | PRU_EVTOUT_0
	halt

ERR:
	LED_ON
	JMP ERR

