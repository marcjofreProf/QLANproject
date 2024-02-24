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
// r6 reserved Control register
// r7 reserved for zeroing registers

// r10 is arbitrary used for operations

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

	// Configure the programmable pointer register for PRU by setting c28_pointer[15:0] // related to shared RAM
	// This will make C28 point to 0x00010000 (PRU shared RAM).
	// http://www.embedded-things.com/bbb/understanding-bbb-pru-shared-memory-access/	
	MOV	r0, SHARED_RAM // 0x100                  // Set C28 to point to shared RAM
	MOV	r10, 0x22000+0x28//PRU0_CTRL | C28add //CONST_PRUSHAREDRAM
	SBBO 	r0, r10, 0, 4//SBCO	r0, CONST_PRUSHAREDRAM, 0, 4 //SBBO r0, r10, 0, 4
	
	// Make c30_pointer point to the PRU control registers
	//MOV	r0, 0x00022000//PRU0_CTRL
	//MOV	r10, 0x22000+0x2C// //CONST_PRUCTRLREG
	//SBBO 	r0, r10, 0, 4//SBCO	r0, CONST_PRUCTRLREG, 0, 4
	//SBCO	r0, CONST_PRUCTRLREG, 0, 4
	MOV 	r6, 0x22000

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
	
	ZERO	&r3, 4 //MOV	r3, 0  // Initialize overflow counter in r3	
//	SUB	r3, r3, 1  Maybe not possible, so account it in c++ code // Initially decrement overflow counter because at least it goes through RESET_CYCLECNT once which will increment the overflow counter
	// Initial Re-initialization of DWT_CYCCNT
//	LBBO	r2, r6, 0, 1 // r2 maps b0 control register
//	CLR	r2.t3
//	SBBO	r2, r6, 0, 4 // stops DWT_CYCCNT
	LBBO	r2, r6, 0, 1 // r2 maps b0 control register
	SET	r2.t3
	SBBO	r2, r6, 0, 1 // Restarts DWT_CYCCNT
	ZERO	&r7, 4 //MOV	r7, 0 // Register for clearing other registers

RESET_CYCLECNT:// This instruciton block has to contain the minimum number of lines and the most simple possible, to better approximate the DWT_CYCCNT clock skew
	// The below could be optimized - then change the skew number in c++ code
	// LBCO and SBCO instructions with byte count 4 take 2 cycles. 
//      LBBO	r2, r6, 0, 4 // r2 maps b0 control register	
//	CLR	r2.t3
//	SBBO	r2, r6, 0, 4 // stops DWT_CYCCNT
//	LBBO	r2, r6, 0, 4 // r2 maps b0 control register
//	SET	r2.t3
//	SBBO	r2, r6, 0, 4 // Restarts DWT_CYCCNT
	SBBO	r7, r6, 0xC, 4 // Resets DWT_CYCNT.Account that we lose 2 cycle counts
	// Non critical but necessary instructions once DWT_CYCCNT has been reset	
	ADD	r3, r3, 1    // Increment overflow counter. Account that we lose 1 cycle count

//START1:
//	SET r30.t11	// disable the data bus. it may be necessary to disable the bus to one peripheral while another is in use to prevent conflicts or manage bandwidth.
	
// Assuming CYCLECNT is mapped or accessible directly in PRU assembly, and there's a way to reset it, which might involve writing to a control register
CHECK_CYCLECNT: // This instruciton block has to contain the minimum number of lines and the most simple possible, to better approximate the DWT_CYCCNT clock skew
	LBBO	r5, r6, 0xC, 4 // r5 maps the value of DWT_CYCCNT // from here, if a reset of DWT_CYCCNT happens we will lose some counts. Account that we lose 1 cycle count here
	QBLT	RESET_CYCLECNT, r5.b3, MAX_VALUE_BEFORE_RESETmostsigByte // If MAX_VALUE_BEFORE_RESETmostsigByte < r5.b3, go to RESET_CYCLECNT. Account that we lose 2 cycle counts

CMDLOOP:
	LBCO	r0, CONST_PRUDRAM, 0, 4 // Load to r0 the content of CONST_PRUDRAM with offset 0, and 4 bytes
	QBEQ	CHECK_CYCLECNT, r0, 0 // loop until we get an instruction
	QBEQ	CHECK_CYCLECNT, r0, 1 // loop until we get an instruction
	// ok, we have an instruction. Assume it means 'begin capture'
	// We remove the command from the host (in case there is a reset from host, we are saved)
	MOV 	r0, 0 
	SBCO 	r0, CONST_PRUDRAM, 0, 4 // Put contents of r0 into CONST_PRUDRAM
//	LED_ON // Indicate that we start acquisiton of timetagging
	ZERO	&r1, 4 //MOV	r1, 0  // reset r1 address to point at the beggining of PRU shared RAM
	MOV	r4, RECORDS // This will be the loop counter to read the entire set of data
//	CLR     r30.t11	// disable the data bus. it may be necessary to disable the bus to one peripheral while another is in use to prevent conflicts or manage bandwidth.
	// Here include once the overflow register
	SBCO 	r3, CONST_PRUSHAREDRAM, r1, 4 // Put contents of overflow DWT_CYCCNT into the address offset at r1
	ADD 	r1, r1, 4 // increment address by 4 bytes
		
WAIT_FOR_EVENT: // At least dark counts will be detected so detections will happen
	// Load the value of R31 into a working register, say R0
	MOV 	r0.b0, r31.b0
	// Mask the relevant bits you're interested in
	// For example, if you're interested in any of the first 8 bits being high, you could use 0xFF as the mask
	AND 	r0.b0, r0.b0, MASKevents // Interested specifically to the bits with MASKevents
	// Compare the result with 0. If it's 0, no relevant bits are high, so loop
	QBEQ 	WAIT_FOR_EVENT, r0.b0, 0
	// If the program reaches this point, at least one of the bits is high
	// Proceed with the rest of the program

TIMETAG:
	// Time counter part
	LBBO	r5, r6, 0xC, 4 // r5 maps the value of DWT_CYCCNT
	SBCO 	r5, CONST_PRUSHAREDRAM, r1, 4 // Put contents of DWT_CYCCNT into the address offset at r1.
	ADD 	r1, r1, 4 // increment address by 4 bytes		
	// Channels detection
	SBCO 	r0.b0, CONST_PRUSHAREDRAM, r1, 1 // Put contents of r0.b0 into the address offset at r1
	ADD 	r1, r1, 1 // increment address by 1 bytes	
	// Check to see if we still need to read more data
	SUB 	r4, r4, 1
	QBNE 	WAIT_FOR_EVENT, r4, 0 // loop if we've not finished
//	SET     r30.t11	// enable the data bus. it may be necessary to disable the bus to one peripheral while another is in use to prevent conflicts or manage bandwidth.
	// we're done. Signal to the application	
	MOV 	r0, 1
	SBCO 	r0, CONST_PRUDRAM, 0, 4 // Put contents of r0 into CONST_PRUDRAM
	LED_ON // For signaling the end visually and also to give time to put the command in the OWN-RAM memory
	LED_OFF
	JMP 	CHECK_CYCLECNT // finished, wait for next command. So it continuosly loops	
	
EXIT:
	// Send notification to Host for program completion
	MOV 	R31.b0, PRU0_R31_VEC_VALID | PRU_EVTOUT_0
	MOV 	r31.b0, PRU0_ARM_INTERRUPT+16

	// Halt the processor
	HALT

ERR:
	LED_ON
	JMP ERR
