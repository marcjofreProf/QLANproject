// PRUassTaggDetScript.p
// Time Tagging functionality on PRU0 with Shared Memory Access (not Direct Memory Access nor DDR)
                                 

.origin 0
.entrypoint INITIATIONS

#include "PRUassTaggDetScript.hp"

#define MASKevents 0x2E // P9_27-30, which corresponds to r31 bits 1,2,3 and 5

// Length of acquisition:
#define RECORDS 1024 // 1024 readings and it matches in the host c++ script
#define MAX_VALUE_BEFORE_RESET 0x0FFFFFFF // Using one bit less of the length of the register to avoid overflow occuring in the time of execution of TIMETAG
#define MAX_VALUE_BEFORE_RESETmostsigByte 0x7F // 127 in decimal
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
// - xBCO instructions when using constant table pointers
// - xBBO instructions when using directly registers

// r0 is arbitrary used for operations
// r1 reserved pointing to SHARED
// r2 reserved mapping control register
// r3 reserved for overflow DWT_CYCCNT counter
// r4 reserved for holding the RECORDS (re-loaded at each iteration)
// r5 reserved for holding the DWT_CYCCNT count value

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
	MOV	r0, OWN_RAM
//	MOV	r10, PRU0_CTRL | C24add//CONST_PRUDRAM
	SBCO	r0, CONST_PRUDRAM, 0, 4  // Load the base address of PRU0 Data RAM into C24

	// Configure the programmable pointer register for PRU by setting c28_pointer[15:0] // related to shared RAM
	// This will make C28 point to 0x00010000 (PRU shared RAM).
	// http://www.embedded-things.com/bbb/understanding-bbb-pru-shared-memory-access/	
	MOV	r0, SHARED_RAM                  // Set C28 to point to shared RAM
	//MOV	r10, PRU0_CTRL | C28add //CONST_PRUSHAREDRAM
	SBCO	r0, CONST_PRUSHAREDRAM, 0, 4
	
	// Make c29_pointer[15:0] point to the PRU control registers
	MOV	r0, PRU0_CTRL
//	MOV	r10, PRU0_CTRL | C29add //CONST_PRUCTRLREG
	SBCO	r0, CONST_PRUCTRLREG, 0, 4

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

        LED_ON	// just for signaling initiations
	LED_OFF	// just for signaling initiations
	
	MOV	r3, 0  // Initialize overflow counter in r3	
	SUB	r3, r3, 1  // Initially decrement overflow counter because at least it goes through RESET_CYCLECNT once which will increment the overflow counter
	// Initial Re-initialization of DWT_CYCCNT
	LBCO	r2.b0, CONST_PRUCTRLREG, 0, 1 // r2 maps b0 control register
	CLR	r2.t3
	SBCO	r2.b0, CONST_PRUCTRLREG, 0, 1 // stops DWT_CYCCNT
	//LBCO	r2.b0, CONST_PRUCTRLREG, 0, 1 // r2 maps b0 control register
	SET	r2.t3
	SBCO	r2.b0, CONST_PRUCTRLREG, 0, 1 // Restarts DWT_CYCCNT

RESET_CYCLECNT:// This instruciton block has to contain the minimum number of lines and the most simple possible, to better approximate the DWT_CYCCNT clock skew
	// The below could be optimized - then change the skew number in c++ code
        //LBCO	r2.b0, CONST_PRUCTRLREG, 0, 1 // r2 maps b0 control register	
	CLR	r2.t3
	SBCO	r2.b0, CONST_PRUCTRLREG, 0, 1 // stops DWT_CYCCNT
	//LBCO	r2.b0, CONST_PRUCTRLREG, 0, 1 // r2 maps b0 control register
	SET	r2.t3
	SBCO	r2.b0, CONST_PRUCTRLREG, 0, 1 // Restarts DWT_CYCCNT
	// Non critical but necessary instructions once DWT_CYCCNT has been reset	
	ADD	r3, r3, 1    // Increment overflow counter

//START1:
//	SET r30.t11	// disable the data bus. it may be necessary to disable the bus to one peripheral while another is in use to prevent conflicts or manage bandwidth.
	
// Assuming CYCLECNT is mapped or accessible directly in PRU assembly, and there's a way to reset it, which might involve writing to a control register
CHECK_CYCLECNT: // This instruciton block has to contain the minimum number of lines and the most simple possible, to better approximate the DWT_CYCCNT clock skew
	LBCO	r5, CONST_PRUCTRLREG, 0xC, 4 // r5 maps the value of DWT_CYCCNT // from here, if a reset of DWT_CYCCNT happens we will lose some counts
	QBLT	RESET_CYCLECNT, r5.b3, MAX_VALUE_BEFORE_RESETmostsigByte // If MAX_VALUE_BEFORE_RESETmostsigByte < r5.b3, go to RESET_CYCLECNT

CMDLOOP:
	LBCO	r0, CONST_PRUDRAM, 0, 4 // Load to r0 the content of CONST_PRUDRAM with offset 0, and 4 bytes
	QBEQ	CHECK_CYCLECNT, r0, 0 // loop until we get an instruction
	QBEQ	CHECK_CYCLECNT, r0, 1 // loop until we get an instruction
	// ok, we have an instruction. Assume it means 'begin capture'
	LED_ON // Indicate that we start acquisiton of timetagging
	MOV	r1, 0  // reset r1 address to point at the beggining of PRU shared RAM
	MOV	r4, RECORDS // This will be the loop counter to read the entire set of data
//	CLR     r30.t11	// disable the data bus. it may be necessary to disable the bus to one peripheral while another is in use to prevent conflicts or manage bandwidth.
		
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
	// Time part
	LBCO	r5, CONST_PRUCTRLREG, 0xC, 4 // r5 maps the value of DWT_CYCCNT
	SBCO 	r5, CONST_PRUSHAREDRAM, r1, 4 // Put contents of DWT_CYCCNT into the address at r1.
	ADD 	r1, r1, 4 // increment address by 4 bytes // This can be improved
	// Here include the overflow register
	SBCO 	r3, CONST_PRUSHAREDRAM, r1, 4 // Put contents of overflow DWT_CYCCNT into the address at r1
	ADD 	r1, r1, 4 // increment address by 4 bytes // This can be improved	
	// Channels detection
	SBCO 	r0.w0, CONST_PRUSHAREDRAM, r1, 2 // Put contents of r0 into the address at r1
	ADD 	r1, r1, 2 // increment address by 2 bytes // This can be improved	
	// Check to see if we still need to read more data
	SUB 	r4, r4, 1
	QBNE 	WAIT_FOR_EVENT, r4, 0 // loop if we've not finished
//	SET     r30.t11	// enable the data bus. it may be necessary to disable the bus to one peripheral while another is in use to prevent conflicts or manage bandwidth.
	// we're done. Signal to the application	
	MOV 	r0, 1
	SBCO 	r0, CONST_PRUDRAM, 0, 4 // Put contents of r0 into CONST_PRUDRAM
	LED_OFF// this signals that we are done with the timetagging acqusition
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
