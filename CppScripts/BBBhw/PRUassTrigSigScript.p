// PRU-ICSS program to control realtime GPIO pins
// But only if the Pinmux Mode has been set correctly with a device  
 // tree overlay!  
 //  
 // Assemble in BBB with:  
 // pasm -b PRUassTrigSigScript.p
 // https://www.ofitselfso.com/BBBCSIO/Help/BBBCSIOHelp_PRUPinInOutExamplePASMCode.html
 
.origin 0				// start of program in PRU memory
.entrypoint INITIATIONS			// program entry point (for debbuger)

#define GPIO0_BANK 0x44E07000
#define GPIO1_BANK 0x4804c000 // this is the address of the BB GPIO1 Bank Register for PR0
#define GPIO2_BANK 0x481ac000 // this is the address of the BBB GPIO2 Bank Register for PRU1. We set bits in special locations in offsets here to put a GPIO high or low.
#define GPIO3_BANK 0x481AE000

#define GPIO_SETDATAOUT 0x194 // at this offset various GPIOs are associated with a bit position. Writing a 32 bit value to this offset enables them (sets them high) if there is a 1 in a corresponding bit. A zero in a bit position here is ignored - it does NOT turn the associated GPIO off.

#define GPIO_CLEARDATAOUT 0x190 //We set a GPIO low by writing to this offset. In the 32 bit value we write, if a bit is 1 the 
// GPIO goes low. If a bit is 0 it is ignored.

#define INS_PER_US		200	// 5ns per instruction fo rBeaglebone black
#define INS_PER_DELAY_LOOP	2	// two instructions per delay loop
#define NUM_REPETITIONS		65535	// Maximum value possible storable to limit the number of cycles. This is wuite limited in number but very controllable (maybe more than one register can be used)
#define DELAY 1 * (INS_PER_US / INS_PER_DELAY_LOOP) // in microseconds
#define PRU0_R31_VEC_VALID	32
#define PRU_EVTOUT_0		3	// the event number that is sent back

// Beaglebone Black has 32 bit registers (for instance Beaglebone AI has 64 bits and more than 2 PRU)
#define AllOutputInterestPinsHigh 0x000000FF// For the defined output pins to set them high in block (and not the ones that are allocated by other processes)

// *** LED routines, so that LED USR0 can be used for some simple debugging
// *** Affects: r2, r3. Each PRU has its of 32 registers
.macro LED_OFF
    MOV r2, 1<<21
    MOV r3, GPIO1_BANK | GPIO_CLEARDATAOUT
    SBBO r2, r3, 0, 4
.endm

.macro LED_ON
    MOV r2, 1<<21
    MOV r3, GPIO1_BANK | GPIO_SETDATAOUT
    SBBO r2, r3, 0, 4
.endm

INITIATIONS:
	MOV r1, GPIO2_BANK | GPIO_SETDATAOUT  // load the address to we wish to set to r1. Note that the operation GPIO2_BANK+GPIO_SETDATAOUT is performed by the assembler at compile time and the resulting constant value is used. The addition is NOT done at runtime by the PRU!
	MOV r2, GPIO2_BANK | GPIO_CLEARDATAOUT // load the address we wish to cleare to r2. Note that every bit that is a 1 will turn off the associated GPIO we do NOT write a 0 to turn it off. 0's are simply ignored.
	MOV r3, AllOutputInterestPinsHigh // load R3 with the LEDs enable/disable bits
	mov r4, 0x00000000

// With delays to produce longer pulses
SIGNALON:	// for setting just one pin would be set r30, r30, #Bit number
	//set r30, r30, 6	
	mov r30.b0, r3.b0 // write the contents of r3 byte 0 to magic r30 output byte 0
	mov r0, DELAY

DELAYON:
	sub r0, r0, 1
	QBNE DELAYON, r0, 0
	
SIGNALOFF:      // for clearing just one pin would be clr r30, r30, #Bit number	
	//clr r30, r30, 6
	mov r30.b0, r4.b0 // write the contents of r4 byte 0 to magic r30 byte 0
	mov r0, DELAY

DELAYOFF:
	sub r0, r0, 1
	QBNE DELAYOFF, r0, 0
	jmp SIGNALON // Might consume more than one clock (maybe 3) but always the same amount


// Without delays (fastest possible)



EXIT:
	mov r31.b0, PRU1_R31_VEC_VALID | PRU_EVTOUT_1
	MOV       r31.b0, PRU1_ARM_INTERRUPT+16
	halt

ERR:
	LED_ON
	JMP ERR

