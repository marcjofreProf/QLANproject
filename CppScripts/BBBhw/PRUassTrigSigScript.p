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

#define GPIO_SETDATAOUToffset 0x194 // at this offset various GPIOs are associated with a bit position. Writing a 32 bit value to this offset enables them (sets them high) if there is a 1 in a corresponding bit. A zero in a bit position here is ignored - it does NOT turn the associated GPIO off.

#define GPIO_CLEARDATAOUToffset 0x190 //We set a GPIO low by writing to this offset. In the 32 bit value we write, if a bit is 1 the 
// GPIO goes low. If a bit is 0 it is ignored.

#define INS_PER_US		200		// 5ns per instruction for Beaglebone black
#define INS_PER_DELAY_LOOP	2		// two instructions per delay loop
#define NUM_REPETITIONS		1024	//Not used. 4294967295	// Maximum value possible storable to limit the number of cycles in 32 bits register. This is wuite limited in number but very controllable (maybe more than one register can be used). This defines the Maximum Transmission Unit - coul dbe named Quantum MTU (defined together with the clock)
#define DELAY 1//1 * (INS_PER_US / INS_PER_DELAY_LOOP) // in microseconds

// Refer to this mapping in the file - pruss_intc_mapping.h
#define PRU0_PRU1_INTERRUPT     17
#define PRU1_PRU0_INTERRUPT     18
#define PRU0_ARM_INTERRUPT      19
#define PRU1_ARM_INTERRUPT      20
#define ARM_PRU0_INTERRUPT      21
#define ARM_PRU1_INTERRUPT      22

#define CONST_PRUCFG         C4
#define CONST_PRUDRAM        C24 // allow the PRU to map portions of the system's memory into its own address space. In particular we will map its own data RAM
#define CONST_IETREG	     C26

#define OWN_RAM              0x00000000 // current PRU data RAM
#define OWN_RAMoffset	     0x00000200 // Offset from Base OWN_RAM to avoid collision with some data tht PRU might store
#define PRU1_CTRL            0x240

// Beaglebone Black has 32 bit registers (for instance Beaglebone AI has 64 bits and more than 2 PRU)
#define AllOutputInterestPinsHigh 0xFF// For the defined output pins to set them high in block (and not the ones that are allocated by other processes)
#define AllOutputInterestPinsLow 0x00// For the defined output pins to set them high in block (and not the ones that are allocated by other processes)

// *** LED routines, so that LED USR0 can be used for some simple debugging
// *** Affects: r28, r29. Each PRU has its of 32 registers
.macro LED_OFF
	MOV	r28, 1<<21
	MOV	r29, GPIO2_BANK | GPIO_CLEARDATAOUToffset
	SBBO	r28, r29, 0, 4
.endm

.macro LED_ON
	MOV	r28, 1<<21
	MOV	r29, GPIO2_BANK | GPIO_SETDATAOUToffset
	SBBO	r28, r29, 0, 4
.endm

// r0 is arbitrary used for operations
// r1 is reserved with the number of NUM_REPETITIONS - storing the PRU 1 DATA number of repetitions
//// If using the cycle counte rin the PRU (not adjusted to synchronization protocols)
// We cannot use Constan table pointers since the base addresses are too far
// r2 reserved for 0x22000 Control register
// r3 reserved for 0x2200C DWT_CYCCNT
// r4 reserved for zeroing registers

// r10 is arbitrary used for operations

// r28 is mainly used for LED indicators operations
// r29 is mainly used for LED indicators operations
// r30 is reserved for output pins
// r31 is reserved for inputs pins
INITIATIONS:
///	MOV r1, GPIO2_BANK | GPIO_SETDATAOUToffset  // load the address to we wish to set to r1. Note that the operation GPIO2_BANK+GPIO_SETDATAOUT is performed by the assembler at compile time and the resulting constant value is used. The addition is NOT done at runtime by the PRU!
//	MOV r2, GPIO2_BANK | GPIO_CLEARDATAOUToffset // load the address we wish to cleare to r2. Note that every bit that is a 1 will turn off the associated GPIO we do NOT write a 0 to turn it off. 0's are simply ignored.
		
	LBCO	r0, CONST_PRUCFG, 4, 4 // Enable OCP master port
	// OCP master port is the protocol to enable communication between the PRUs and the host processor
	CLR	r0, r0, 4         // Clear SYSCFG[STANDBY_INIT] to enable OCP master port
	SBCO	r0, CONST_PRUCFG, 4, 4

	// Configure the programmable pointer register for PRU by setting c24_pointer // related to pru data RAM. Where the commands will be found
	// This will make C24 point to 0x00000000 (PRU data RAM).
	MOV	r0, OWN_RAM | OWN_RAMoffset// | OWN_RAMoffset
	MOV	r10, 0x24000+0x20// | C24add//CONST_PRUDRAM
	SBBO	r0, r10, 0, 4//SBCO	r0, CONST_PRUDRAM, 0, 4  // Load the base address of PRU0 Data RAM into C24
	
	// Initial initializations
	LDI	r4, 0 // zeroing
	
	// Initial Re-initialization for IET counter
	// The Clock gating Register controls the state of Clock Management. 
	//LBCO 	r0, CONST_PRUCFG, 0x10, 4                    
	MOV 	r0, 0x24924
	SBCO 	r0, CONST_PRUCFG, 0x10, 4 
	//LBCO	r2, CONST_IETREG, 0, 1 //
	//SET ocp_clk:1 or of iep_clk:0// It is important ot select the clock source to be in synch with the PRU clock. I believe it should be ocp_clk
	LDI	r0, 1
	SBCO 	r0, CONST_PRUCFG, 0x30, 4
	// IEP configuration
	MOV	r0, 0x111 // Enable and Define increment value to 1
	SBCO	r0, CONST_IETREG, 0, 4 // Enables IET count and sets configuration
	// Deactivate IEP compensation
	SBCO 	r4, CONST_IETREG, 0x08, 4
	
	// Using cycle counter
	MOV	r2, 0x22000
	MOV	r3, 0x2200C
	
	// Initializations
	LDI	r30, 0 // All signal pins down
	LDI	r4, 0 // zeroing
	MOV	r1, NUM_REPETITIONS// Initial initialization jus tin case// Cannot be done with LDI instruction because it may be a value larger than 65535. load r3 with the number of cycles. For the time being only up to 65535 ->develop so that it can be higher
	
//	LED_ON	// just for signaling initiations
//	LED_OFF	// just for signaling initiations

CMDLOOP:
	//LBCO	r0.b0, CONST_PRUDRAM, 4, 1 // Load to r0 the content of CONST_PRUDRAM with offset 0, and 4 bytes
	//QBEQ	CMDLOOP, r0.b0, 0 // loop until we get an instruction. Code 0 means idle
	//QBEQ	CMDLOOP, r0.b0, 1 // loop until we get an instruction. Code 1 means finished (to inform the ARM host)
	QBBC	CMDLOOP, r31, 31	//
	// ok, we have an instruction (code 2). Assume it means 'begin signals'
	// We remove the command from the host (in case there is a reset from host, we are saved)
	//SBCO 	r4.b0, CONST_PRUDRAM, 4, 1 // Put contents of r0 into CONST_PRUDRAM
	SBCO	r4.b0, C0, 0x24, 1 // Reset host interrupt
CMDLOOP2:// Double verification of host sending start command
	LBCO	r0.b0, CONST_PRUDRAM, 4, 1 // Load to r0 the content of CONST_PRUDRAM with offset 8, and 4 bytes
	QBEQ	CMDLOOP, r0.b0, 0 // loop until we get an instruction	
	LBCO 	r1, CONST_PRUDRAM, 0, 4// Read the number of NUM_REPETITIONS from positon 0 of PRU1 DATA RAM and stored it
	SBCO	r4.b0, CONST_PRUDRAM, 4, 1 // Store a 0 in CONST_PRUDRAM with offset 8, and 4 bytes. Reset the command
	//MOV	r31.b0, PRU1_ARM_INTERRUPT+16// Here send interrupt to host to measure time
//PSEUDOSYNCH:
//	// To give some sense of synchronization with the other PRU time tagging, wait for DWT_CYCNT or IEP timer (which has been enabled and keeps disciplined with IEP timer counter by the other PRU)
//	LBBO	r0.b0, r3, 0, 1//LBBO	r0.b0, r3, 0, 1//LBCO	r0.b0, CONST_IETREG, 0xC, 1
//	AND	r0, r0, 0x03 // Since the signals have a minimum period of 4 clock cycles
//	QBEQ	SIGNALON, r0.b0, 3 // Coincides with a 3
//	QBEQ	SIGNALON, r0.b0, 2 // Coincides with a 2
//	QBEQ	SIGNALON, r0.b0, 1 // Coincides with a 1
//	QBEQ	SIGNALON, r0.b0, 0 // Coincides with a 0
SIGNALON:	
	MOV	r30.b0, AllOutputInterestPinsHigh // write the contents of r1 byte 0 to magic
	SUB	r1, r1, 1	// Substract 1 count cycle r30 output byte 0
//	MOV	r0, DELAY
//DELAYON:
//	SUB 	r0, r0, 1
//	QBNE	DELAYON, r0, 0
SIGNALOFF:
	MOV	r30.b0, AllOutputInterestPinsLow // write the contents of r2 byte 0 to magic r30 byte 0
	QBNE	SIGNALON, r1, 0 // condition jump to SIGNALON because we have not finished the number of repetitions
//	MOV	r0, DELAY
//DELAYOFF:
//	SUB 	r0, r0, 1
//	QBNE 	DELAYOFF, r0, 0
FINISHLOOP:
	// The following lines do not consume "signal speed"
	MOV 	r31.b0, PRU1_ARM_INTERRUPT+16// Notification sent at the beginning of the signal//SBCO	r5.b0, CONST_PRUDRAM, 4, 1 // Put contents of r0 into CONST_PRUDRAM// code 1 means that we have finished.This can be substituted by an interrupt: MOV 	r31.b0, PRU1_ARM_INTERRUPT+16
	//MOV	r1, r5// Cannot be done with LDI instruction because it may be a value larger than 65535. load r3 with the number of cycles. For the time being only up to 65535 ->develop so that it can be higher
	JMP	CMDLOOP // Might consume more than one clock (maybe 3) but always the same amount

EXIT:
	MOV	r31.b0, PRU1_ARM_INTERRUPT+16
	HALT

ERR:	// Signal error
	LED_ON
	JMP ERR

