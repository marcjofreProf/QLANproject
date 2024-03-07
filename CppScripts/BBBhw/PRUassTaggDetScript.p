// PRUassTaggDetScript.p
// Time Tagging functionality on PRU0 with Shared Memory Access (not Direct Memory Access nor DDR)
// It is pending studying if it is worth making use of DMA or DDR in terms of larger memory space or transfer speed

.origin 0
.entrypoint INITIATIONS

#include "PRUassTaggDetScript.hp"

#define MASKevents 0x0F // P9_28-31, which corresponds to r31 bits 0,1,2,3

// Length of acquisition:
#define RECORDS 2048 // readings and it matches in the host c++ script
#define MAX_VALUE_BEFORE_RESETmostsigByte 0x80 // 128 in decimal
// *** LED routines, so that LED USR0 can be used for some simple debugging
// *** Affects: r28, r29. Each PRU has its of 32 registers
.macro LED_OFF
	MOV	r28, 1<<21
	MOV	r29, GPIO1_BANK | GPIO_CLEARDATAOUToffset
	SBBO	r28, r29, 0, 4
.endm

.macro LED_ON
	MOV	r28, 1<<21
	MOV	r29, GPIO1_BANK | GPIO_SETDATAOUToffset
	SBBO	r28, r29, 0, 4
.endm

//Notice:
// - xBCO instructions when using constant table pointers. Faster, probably 3 clock cycles
// - xBBO instructions when using directly registers. Slower, probably between 3 and 5 clock cycles

// r0 is arbitrary used for operations
// r1 reserved pointing to SHARED
// r2 reserved mapping control register
// r3 reserved for overflow DWT_CYCCNT counter
// r4 reserved for holding the RECORDS (re-loaded at each iteration)
// r5 reserved for holding the DWT_CYCCNT count value
// r6 reserved because detected channels are concatenated with r5 in the write to SHARED RAM
// r7 reserved for 0 value (zeroing registers)
// r8 reserved for cycle count initial skew
// r9 reserved for cycle count final skew
// r10 reserved for cycle count initial threshold reset
// r11 reserved for cycle count final threshold reset
// r16 reserved for raising edge detection operation together with r6
// r17 reserved for puting a 1
// r18 reserved for synch countdown

//// If using the cycle counte rin the PRU (not adjusted to synchronization protocols)
// We cannot use Constan table pointers since the base addresses are too far
// r12 reserved for 0x22000 Control register
// r13 reserved for 0x2200C DWT_CYCCNT
//// If using IET timer (potentially adjusted to synchronization protocols)
// We can use Constant table pointers C26
// CONST_IETREG 0x0002E000
// IET Count 0xC offset

// r14 is arbitrary used for operations
// r15 is arbitrary used for operations

// r28 is mainly used for LED indicators operations
// r29 is mainly used for LED indicators operations
// r30 is reserved for output pins
// r31 is reserved for inputs pin
INITIATIONS:// This is only run once    
	LBCO	r0, CONST_PRUCFG, 4, 4 // Enable OCP master port
	// OCP master port is the protocol to enable communication between the PRUs and the host processor
	CLR	r0, r0, 4         // Clear SYSCFG[STANDBY_INIT] to enable OCP master port
	SBCO	r0, CONST_PRUCFG, 4, 4
    
	// Configure the programmable pointer register for PRU by setting c24_pointer[3:0] // related to pru data RAM, where the commands will be found
	// This will make C24 point to 0x00000000 (PRU data RAM).
	MOV	r0, OWN_RAM// | OWN_RAMoffset // When using assembler, the PRU does not put data in the first addresses of OWN_RAM (when using c++ PRU direct programming the PRU  might use some initial addresses of OWN_RAM space
	//MOV	r10, 0x22000+0x20// | C24add//CONST_PRUDRAM
	SBCO	r0, CONST_PRUDRAM, 0, 4  // Load the base address of PRU0 Data RAM into C24
	
	// This will make C26 point to 0x0002E000 (IET).
	MOV	r0, 0x0002E000// | OWN_RAMoffset // When using assembler, the PRU does not put data in the first addresses of OWN_RAM (when using c++ PRU direct programming the PRU  might use some initial addresses of OWN_RAM space
	//MOV	r10, 0x22000+0x20// | C24add//CONST_PRUDRAM
	SBCO	r0, CONST_IETREG, 0, 4  // Load the base address of PRU0 Data RAM into C24

	// Configure the programmable pointer register for PRU by setting c28_pointer[15:0] // related to shared RAM
	// This will make C28 point to 0x00010000 (PRU shared RAM).
	// http://www.embedded-things.com/bbb/understanding-bbb-pru-shared-memory-access/	
	MOV	r0, SHARED_RAM // 0x100                  // Set C28 to point to shared RAM
	MOV	r14, 0x22000+0x28//PRU0_CTRL | C28add //CONST_PRUSHAREDRAM
	SBBO 	r0, r14, 0, 4//SBCO	r0, CONST_PRUSHAREDRAM, 0, 4 //SBBO r0, r10, 0, 4
		
//	// Configure the programmable pointer register for PRU by setting c31_pointer[15:0] // related to ddr.
//	// This will make C31 point to 0x80001000 (DDR memory). 0x80000000 is where DDR starts, but we leave some offset (0x00001000) to avoid conflicts with other critical data present
//	https://groups.google.com/g/beagleboard/c/ukEEblzz9Gk
//	MOV	r0, DDR_MEM                    // Set C31 to point to ddr
//	//MOV	r10, PRU0_CTRL | C31add
//	SBCO    r0, CONST_DDR, 0, 4

	//Load values from external DDR Memory into Registers R0/R1/R2
	//LBCO      r0, CONST_DDR, 0, 12
	//Store values from read from the DDR memory into PRU shared RAM
	//SBCO      r0, CONST_PRUSHAREDRAM, 0, 12

//      LED_ON	// just for signaling initiations
//	LED_OFF	// just for signaling initiations
	// Using cycle counter
	MOV	r12, 0x22000
	MOV	r13, 0x2200C
	// Initializations
	LDI	r3, 0 //MOV	r3, 0  // Initialize overflow counter in r3	
	LDI 	r5, 0 // Initialize for the first time r5
	LDI	r6, 0 // Initialization
	LDI	r16, 0 // Initialization
	LDI	r17, 1
//	SUB	r3, r3, 1  Maybe not possible, so account it in c++ code // Initially decrement overflow counter because at least it goes through RESET_CYCLECNT once which will increment the overflow counter	
	// Initializations for faster execution
	LDI	r7, 0 //MOV	r6, 0 // Register for clearing other registers
	LDI	r8, 0
	MOV	r9, 0xFFFFFFFF // For the initial skew count monitor
	LDI	r10, 0
	MOV	r11, 0xFFFFFFFF // For the initial thresdhol reset count we need to start with a high number
	
	// Initial Re-initialization of DWT_CYCCNT
	LBBO	r2, r12, 0, 1 // r2 maps b0 control register
	CLR	r2.t3
	SBBO	r2, r12, 0, 1 // stops DWT_CYCCNT
	LBBO	r2, r12, 0, 1 // r2 maps b0 control register
	SET	r2.t3
	SBBO	r2, r12, 0, 1 // Enables DWT_CYCCNT
		
	// Initial Re-initialization for IET counter
	//LBCO	r2, CONST_IETREG, 0, 1 //
	MOV	r0, 0x11 // Enable and Define increment value to 1
	SBCO	r0, CONST_IETREG, 0, 1 // Enables IET count
	
	// Keep close together the clearing of the counters (keep order)
	SBCO	r7, CONST_IETREG, 0xC, 4 // Clear IEP timer count
	SBBO	r7, r13, 0, 4 // Clear DWT_CYCNT. Account that we lose 2 cycle counts		
	
	// REad once the counters (keep the reading order along the script)
	LBCO	r5, CONST_IETREG, 0xC, 4 // Read once IEP timer count
	//LBBO	r9, r13, 0 , 4 // Read DWT_CYCCNT	

NORMSTEPS: // So that always takes the same amount of counts for reset
	QBA     CHECK_CYCLECNT
RESET_CYCLECNT:// This instruction block has to contain the minimum number of lines and the most simple possible, to better approximate the DWT_CYCCNT clock skew
	//SUB	r10, r9, r5 // Make the difference between counters
	LBBO	r8, r13, 0, 4 // read DWT_CYCNT
	SBCO	r7, CONST_IETREG, 0xC, 4 // Reset IEP counter to account for difference with DWT_CYCCNT. Account that we lose 12 cycle counts
	// Non critical but necessary instructions once IEP counter and DWT_CYCCNT have been reset
	LBBO	r9, r13, 0, 4 // read DWT_CYCNT	
	SBBO	r7, r13, 0, 4 // Update DWT_CYCNT. Account that we lose 2 cycle counts				
	ADD	r3, r3, 1    // Increment overflow counter. Account that we lose 1 cycle count

//START1:
//	SET r30.t11	// disable the data bus. it may be necessary to disable the bus to one peripheral while another is in use to prevent conflicts or manage bandwidth.
	
// Assuming CYCLECNT is mapped or accessible directly in PRU assembly, and there's a way to reset it, which might involve writing to a control register
CHECK_CYCLECNT: // This instruciton block has to contain the minimum number of lines and the most simple possible, to better approximate the DWT_CYCCNT clock skew	
	LBCO	r5, CONST_IETREG, 0xC, 4 // LBBO	// from here, if a reset of count we will lose some counts.		
	QBLE	RESET_CYCLECNT, r5.b3, MAX_VALUE_BEFORE_RESETmostsigByte // If MAX_VALUE_BEFORE_RESETmostsigByte <= r5.b3, go to RESET_CYCLECNT. Account that we lose 1 cycle counts
CMDLOOP:
	LBCO	r0.b0, CONST_PRUDRAM, 4, 1 // Load to r0 the content of CONST_PRUDRAM with offset 0, and 4 bytes
	QBEQ	NORMSTEPS, r0.b0, 0 // loop until we get an instruction
	QBEQ	CHECK_CYCLECNT, r0.b0, 1 // loop until we get an instruction
	// ok, we have an instruction. Assume it means 'begin capture'
	// We remove the command from the host (in case there is a reset from host, we are saved)
	SBCO 	r7.b0, CONST_PRUDRAM, 4, 1 // Put contents of r7 into CONST_PRUDRAM
	/// Relative synch count down
	LBCO	r18, CONST_PRUDRAM, 0, 4
COUNTDOWN:
	SUB	r18, r18, 1
	QBNE	COUNTDOWN, r18, 0
//	LED_ON // Indicate that we start acquisiton of timetagging
	LDI	r1, 0 //MOV	r1, 0  // reset r1 address to point at the beggining of PRU shared RAM
	MOV	r4, RECORDS // This will be the loop counter to read the entire set of data
//	CLR     r30.t11	// disable the data bus. it may be necessary to disable the bus to one peripheral while another is in use to prevent conflicts or manage bandwidth.
	// Here include once the overflow register
	SBCO 	r3, CONST_PRUSHAREDRAM, r1, 4 // Put contents of overflow DWT_CYCCNT into the address offset at r1
	ADD 	r1, r1, 4 // increment address by 4 bytes
		
WAIT_FOR_EVENT: // At least dark counts will be detected so detections will happen
	// Load the value of R31 into a working register, say R0
	// Edge detection
	MOV 	r16.b0, r31.b0 // This wants to be zeros for edge detection
	MOV	r6.b0, r31.b0 // Consecutive red for edge detection
	NOT	r16.b0, r16.b0 // 0s converted to 1s
	AND	r6.b0, r6.b0, r16.b0 // Only does complying with a rising edge
	// Mask the relevant bits you're interested in	
	// For example, if you're interested in any of the first 8 bits being high, you could use 0xFF as the mask
	//AND 	r6.b0, r6.b0, MASKevents // Interested specifically to the bits with MASKevents. MAybe there are never counts in this first 8 bits if there is not explicitly a signal.
	// Compare the result with 0. If it's 0, no relevant bits are high, so loop
	QBEQ 	WAIT_FOR_EVENT, r6.b0, 0
	// If the program reaches this point, at least one of the bits is high
	// Proceed with the rest of the program

TIMETAG:
	// Faster Concatenated Time counter and Detection channels
	LBCO	r5, CONST_IETREG, 0xC, 4 // r5 maps the value of DWT_CYCCNT. From here account for counts until reset to adjust the threshold in GPIO c++
	SBCO 	r5, CONST_PRUSHAREDRAM, r1, 5 // Put contents of r5 and r6.b0 of DWT_CYCCNT into the address offset at r1.
	ADD 	r1, r1, 5 // increment address by 5 bytes	
	// Check to see if we still need to read more data
	SUB 	r4, r4, 1
	QBNE 	WAIT_FOR_EVENT, r4, 0 // loop if we've not finished	
	SUB	r15, r11, r10 // Threshold reset counts
	LBBO	r10, r13, 0, 4 // read DWT_CYCNT
//	SET     r30.t11	// enable the data bus. it may be necessary to disable the bus to one peripheral while another is in use to prevent conflicts or manage bandwidth.
	//// For checking control, place as the last value the current counter of DWT_CYCCNT as well as the last IEP timer count - DWT_CYCCNT comparison
	//LBBO	r9, r13, 0 , 4 // Read DWT_CYCCNT
	//SBCO 	r9, CONST_PRUSHAREDRAM, r1, 4
	//ADD 	r1, r1, 4 // increment address by 4 bytes
	//SBCO 	r10, CONST_PRUSHAREDRAM, r1, 4
	//ADD 	r1, r1, 4 // increment address by 4 bytes
	//// For checking control, place as the last value the current estimated skew counts and threshold reset counts
	SUB	r14, r9, r8 // Skew counts
	SBCO 	r14, CONST_PRUSHAREDRAM, r1, 4
	ADD 	r1, r1, 4 // increment address by 4 bytes	
	SBCO 	r15, CONST_PRUSHAREDRAM, r1, 4
	ADD 	r1, r1, 4 // increment address by 4 bytes
	// we're done. Signal to the application
	SBCO 	r17.b0, CONST_PRUDRAM, 4, 1 // Put contents of r0 into CONST_PRUDRAM// code 1 means that we have finished. This can be substituted by an interrupt: MOV 	r31.b0, PRU0_ARM_INTERRUPT
	//LED_ON // For signaling the end visually and also to give time to put the command in the OWN-RAM memory
	//LED_OFF
	//// Make sure that counters are enabled
	LBBO	r2, r12, 0, 1 // r2 maps b0 control register
	SET	r2.t3
	SBBO	r2, r12, 0, 1 // Enables DWT_CYCCNT
	MOV	r0, 0x11 // Enable and Define increment value to 1
	SBCO	r0, CONST_IETREG, 0, 1 // Enables IET count	
	LBBO	r11, r13, 0, 4 // read DWT_CYCNT
	JMP 	CHECK_CYCLECNT // finished, wait for next command. So it continuosly loops	
	
EXIT:
	// Send notification (interrupt) to Host for program completion
	MOV 	r31.b0, PRU0_ARM_INTERRUPT
	// Halt the processor
	HALT

ERR:
	LED_ON
	JMP ERR
