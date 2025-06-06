// PRU-ICSS program to generate adjusted clock signal - Triggered every time by host interrupts
 //  
 // Assemble in BBB with:  
 // pasm -b PRUassClockPhysicalAdj.p
 // To be run on PRU1
 
.origin 0				// start of program in PRU memory
.entrypoint INITIATIONS			// program entry point (for debbuger)

#define GPIO0_BANK 0x44E07000
#define GPIO1_BANK 0x4804c000 // this is the address of the BB GPIO1 Bank Register for PR0
#define GPIO2_BANK 0x481ac000 // this is the address of the BBB GPIO2 Bank Register for PRU1. We set bits in special locations in offsets here to put a GPIO high or low.
#define GPIO3_BANK 0x481AE000

#define GPIO_SETDATAOUToffset 0x194 // at this offset various GPIOs are associated with a bit position. Writing a 32 bit value to this offset enables them (sets them high) if there is a 1 in a corresponding bit. A zero in a bit position here is ignored - it does NOT turn the associated GPIO off.

#define GPIO_CLEARDATAOUToffset 0x190 //We set a GPIO low by writing to this offset. In the 32 bit value we write, if a bit is 1 the 
// GPIO goes low. If a bit is 0 it is ignored.
#define PRU1QuarterClocks	10
// adjust to longest path so that the period of the signal is exact. The longest path is when in the OFF state the system has to check for an interrupt

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
#define CONST_PRUSHAREDRAM   C28 // mapping shared memory

#define OWN_RAM              0x00000000 // current PRU data RAM
#define OWN_RAMoffset	     0x00000200 // Offset from Base OWN_RAM to avoid collision with some data tht PRU might store
#define PRU1_CTRL            0x240
#define SHARED_RAM           0x100

// Beaglebone Black has 32 bit registers (for instance Beaglebone AI has 64 bits and more than 2 PRU)
#define AllOutputInterestPinsHigh 0xFF// For the defined output pins to set them high in block (and not the ones that are allocated by other processes)
#define AllOutputInterestPinsLow 0x00// For the defined output pins to set them low in block (and not the ones that are allocated by other processes)

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
// r1 reserved for storing the actual Quarter period value
//// If using the cycle counte rin the PRU (not adjusted to synchronization protocols)
// We cannot use Constan table pointers since the base addresses are too far
// r2 reserved mapping control register
// r4 reserved for zeroing registers
// r5 reserved for 0xFFFFFFFF
// r6 reserved for 0x22000 Control register
// r7 reserved for 0x2200C DWT_CYCCNT

// r10 is arbitrary used for operations

// r28 is mainly used for LED indicators operations
// r29 is mainly used for LED indicators operations
// r30 is reserved for output pins
// r31 is reserved for inputs pins
INITIATIONS:
//	MOV r1, GPIO2_BANK | GPIO_SETDATAOUToffset  // load the address to we wish to set to r1. Note that the operation GPIO2_BANK+GPIO_SETDATAOUT is performed by the assembler at compile time and the resulting constant value is used. The addition is NOT done at runtime by the PRU!
//	MOV r2, GPIO2_BANK | GPIO_CLEARDATAOUToffset // load the address we wish to cleare to r2. Note that every bit that is a 1 will turn off the associated GPIO we do NOT write a 0 to turn it off. 0's are simply ignored.
		
	LBCO	r0, CONST_PRUCFG, 4, 4 // Enable OCP master port
	// OCP master port is the protocol to enable communication between the PRUs and the host processor
	CLR		r0, r0, 4         // Clear SYSCFG[STANDBY_INIT] to enable OCP master port
	SBCO	r0, CONST_PRUCFG, 4, 4

	// Configure the programmable pointer register for PRU by setting c24_pointer // related to pru data RAM. Where the commands will be found
	// This will make C24 point to 0x00000000 (PRU data RAM).
	MOV		r0, OWN_RAM | OWN_RAMoffset
	MOV		r10, 0x24000+0x20// | C24add//CONST_PRUDRAM
	SBBO	r0, r10, 0, 4//SBCO	r0, CONST_PRUDRAM, 0, 4  // Load the base address of PRU1 Data RAM into C24
	
	// Configure the programmable pointer register for PRU by setting c28_pointer[15:0] // related to shared RAM
	// This will make C28 point to 0x00010000 (PRU shared RAM).
	// http://www.embedded-things.com/bbb/understanding-bbb-pru-shared-memory-access/	
	MOV	r0, SHARED_RAM // 0x100                  // Set C28 to point to shared RAM
	MOV	r10, 0x24000+0x28//PRU1_CTRL | C28add //CONST_PRUSHAREDRAM
	SBBO 	r0, r10, 0, 4//SBCO	r0, CONST_PRUSHAREDRAM, 0, 4 //SBBO r0, r10, 0, 4
	
//	// This will make C26 point to 0x0002E000 (IEP).
//	MOV	r0, 0x0002E000//
//	SBCO	r0, CONST_IETREG, 0, 4  // Load the base address of IEP
	
	// Initializations
	LDI	r30, 0 // All signal pins down
	LDI	r4, 0
	MOV	r5, 0xFFFFFFFF
	MOV	r6, 0x22000
	MOV	r7, 0x2200C
	MOV	r1, PRU1QuarterClocks
	
	// This scripts initiates first the timers
	// Initial Re-initialization of DWT_CYCCNT
//	LBBO	r2, r6, 0, 1 // r2 maps b0 control register
//	CLR	r2.t3
//	SBBO	r2, r6, 0, 1 // stops DWT_CYCCNT
//	SBBO	r4, r7, 0, 4 // Clear DWT_CYCNT. Account that we lose 2 cycle counts
//	LBBO	r2, r6, 0, 1 // r2 maps b0 control register
//	SET	r2.t3
//	SBBO	r2, r6, 0, 1 // Enables DWT_CYCCNT
		
//	// Initial Re-initialization for IET counter
//	// The Clock gating Register controls the state of Clock Management
//	//LBCO 	r0, CONST_PRUCFG, 0x10, 4                    
//	MOV 	r0, 0x24924
//	SBCO 	r0, CONST_PRUCFG, 0x10, 4 
//	//LBCO	r2, CONST_IETREG, 0, 1 //
//	//SET ocp_clk:1 or of iep_clk:0
//	MOV	r0, 0
//	SBCO 	r0, CONST_PRUCFG, 0x30, 4
//	// IEP configuration
//	MOV	r0, 0x111 // Enable and Define increment value to 1
//	SBCO	r0, CONST_IETREG, 0, 4 // Enables IET count and sets configuration
//	// Deactivate IEP compensation
//	SBCO 	r4, CONST_IETREG, 0x08, 4
	
	// Keep close together the clearing of the counters (keep order)	
//	SBCO	r5, CONST_IETREG, 0xC, 4 // Clear IEP timer count
//	SBBO	r4, r7, 0, 4 // Clear DWT_CYCNT. Account that we lose 2 cycle counts	
		
//	LED_ON	// just for signaling initiations
//	LED_OFF	// just for signaling initiations

CMDLOOP:
	QBBC	CMDLOOP, r31, 31	//Reception or not of the host interrupt
	// We remove the interrupt from the host (in case there is a reset from host, we are saved)
	SBCO	r4.b0, C0, 0x24, 1 // Reset host interrupt
CMDLOOP2:// Double verification of host sending start command
	LBCO	r0.b0, CONST_PRUDRAM, 4, 1 // Load to r0 the content of CONST_PRUDRAM with offset 8, and 4 bytes
	QBEQ	CMDLOOP, r0.b0, 0 // loop until we get an instruction
	SBCO	r4.b0, CONST_PRUDRAM, 4, 1 // Store a 0 in CONST_PRUDRAM with offset 8, and 4 bytes. Remove the command
//	// Read the number of clocks that defines the period from positon 0 of PRU1 DATA RAM and stored it
//	LBCO 	r1, CONST_PRUDRAM, 0, 4 // Value of quarter period
	MOV 	r1, PRU1QuarterClocks
//PSEUDOSYNCH:// Only needed at the beggining to remove the slow drift	
//	LBBO	r0, r7, 0, 4// read the DWT_CYCCNT
//	MOV	r8, CYCLESRESYNCH
//	LSL	r10, r9, 2 // Multiply by 4 to have the total period
//	SUB	r0, r10, r0 // Substract to find how long to wait	
//	LSR	r0, r0, 1// Divide by two because the PSEUDOSYNCH consumes double
//	ADD	r0, r0, 1// ADD 1 to not have a substraction below zero which halts
//PSEUDOSYNCHLOOP:
//	SUB	r0, r0, 1
//	QBNE	PSEUDOSYNCHLOOP, r0, 0 // Coincides with a 0
	//MOV 	r31.b0, PRU1_ARM_INTERRUPT+16// Here send interrupt to host to measure time
SIGNALON:
	MOV		r30.b0, AllOutputInterestPinsHigh // write the contents to magic r30 output byte 0
DELAYON:
	SUB 	r1, r1, 1
	QBNE	DELAYON, r1, 0

SIGNALOFF:
	MOV		r30.b0, AllOutputInterestPinsLow // write the contents to magic r30 byte 0

FINISHLOOP:
//	LBCO 	r1, CONST_PRUDRAM, 0, 4 // Value of quarter period updated
	// Send notification (interrupt) to Host for program completion
	MOV 	r31.b0, PRU1_ARM_INTERRUPT+16// Notification sent at the beginning of the signal
	JMP	CMDLOOP

EXIT:
	// Send notification (interrupt) to Host for program completion
//	MOV 	r31.b0, PRU1_ARM_INTERRUPT+16
	// Halt the processor
	HALT

ERR:	// Signal error
	LED_ON
//	JMP ERR
	HALT

