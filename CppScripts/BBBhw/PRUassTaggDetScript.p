// PRUassTaggDetScript.p
// Time Tagging functionality on PRU0 with Shared Memory Access (not Direct Memory Access nor DDR)
                                 

.origin 0
.entrypoint INITIATIONS

#include "PRUassTaggDetScript.hp"

#define MASKevents 0x2E // P9_27-30, which corresponds to r31 bits 1,2,3 and 5

// Length of acquisition:
#define RECORDS 1000 // Seems 1000 readings and it matches in the host c++ script
#define MAX_VALUE_BEFORE_RESET 0xFFFFFFFF // It has to be improved, because overflow will occur in the time of execution probably
// *** LED routines, so that LED USR0 can be used for some simple debugging
// *** Affects: r28, r29. Each PRU has its of 32 registers
.macro LED_OFF
	MOV	r28, 1<<21
	MOV	r29, GPIO1_BANK | GPIO_CLEARDATAOUT
	SBBO	r28, r29, 0, 4
.endm

.macro LED_ON
	MOV	r28, 1<<21
	MOV	r29, GPIO1_BANK | GPIO_SETDATAOUT
	SBBO	r28, r29, 0, 4
.endm

// r0 is arbitrary used for operations
// r1 reserved pointing to SHARED
// r2 reserved mapping CYCLECNT
// r3 reserved for overflow counter
// r4 reserved for holding the RECORDS (re-loaded at each iteration)
// r5 reserved for holding the auxiliary CYCLECNT count

// r28 is mainly used for LED indicators operations
// r29 is mainly used for LED indicators operations
// r30 is reserved for output pins
// r31 is reserved for inputs pin
INITIATIONS:// This is only run once    
	LBCO	r0, CONST_PRUCFG, 4, 4 // Enable OCP master port
	// OCP master port is the protocol to enable communication between the PRUs and the host processor
	CLR	r0, r0, 4         // Clear SYSCFG[STANDBY_INIT] to enable OCP master port
	SBCO	r0, CONST_PRUCFG, 4, 4
    
	// Configure the programmable pointer register for PRU by setting c24_pointer // related to pru data RAM, where the commands will be found
	// This will make C24 point to 0x00000000 (PRU data RAM).
	MOV	r0, OWNRAM
	SBBO	r0, CONST_PRUDRAM, 0, 4  // Load the base address of PRU0 Data RAM into C24

	// Configure the programmable pointer register for PRU by setting c28_pointer[15:0] // related to shared RAM
	// This will make C28 point to 0x00010000 (PRU shared RAM).
	// http://www.embedded-things.com/bbb/understanding-bbb-pru-shared-memory-access/	
	MOV	r0, SHARED_RAM                  // Set C28 to point to shared RAM
	SBBO	r0, CONST_PRUSHAREDRAM, 0, 4
	
	// Make C29 point to the PRU control registers
	MOV	r0, PRU0_CTRLREG	
	SBBO	r0, CONST_PRUCTRLREG, 0, 4

//	// Configure the programmable pointer register for PRU by setting c31_pointer[15:0] // related to ddr.
//	// This will make C31 point to 0x80001000 (DDR memory). 0x80000000 is where DDR starts, but we leave some offset (0x00001000) to avoid conflicts with other critical data present
//	https://groups.google.com/g/beagleboard/c/ukEEblzz9Gk
//	MOV	r0, DDR_MEM                    // Set C31 to point to ddr
//	SBBO    r0, CONST_DDR, 0, 4

	//Load values from external DDR Memory into Registers R0/R1/R2
	//LBCO      r0, CONST_DDR, 0, 12
	//Store values from read from the DDR memory into PRU shared RAM
	//SBCO      r0, CONST_PRUSHAREDRAM, 0, 12

        LED_ON	// just for signaling initiations
	LED_OFF	// just for signaling initiations

	MOV	r1, SHARED  // PRU shared RAM
	MOV	r3, 0  // Initialize overflow counter in r3	
	SUB	r3, r3, 1    // Initially decrement overflow counter because at least it goes through RESET_CYCLECNT once which will increment the overflow counter

RESET_CYCLECNT:
        // Reset CYCLECNT here by writing to its control register or using the appropriate mechanism    
        // Disabling and Enable cycle counter by setting bit 3 (COUNTENABLE) of the control register to re-start cycle counter
        LBCO	r2, CONST_PRUCTRLREG, 0, 4 // r2 maps de CYCLECNT	
	CLR	r2.t3
	SBCO	r2, CONST_PRUCTRLREG, 0, 4
	LBCO	r2, CONST_PRUCTRLREG, 0, 4 // r2 maps de CYCLECNT
	SET	r2.t3
	SBCO	r2, CONST_PRUCTRLREG, 0, 4		
	ADD	r3, r3, 1    // Increment overflow counter

START1:
//	SET r30.t11	// disable the data bus. it may be necessary to disable the bus to one peripheral while another is in use to prevent conflicts or manage bandwidth.
	MOV	r4, RECORDS // This will be the loop counter to read the entire set of data		
		
// Assuming CYCLECNT is mapped or accessible directly in PRU assembly, and there's a way to reset it, which might involve writing to a control register
CHECK_CYCLECNT:
	LBCO	r2, CONST_PRUCTRLREG, 0, 4 // r2 maps de CYCLECNT
	QBGT	RESET_CYCLECNT, r2, MAX_VALUE_BEFORE_RESET // If r2 > MAX_VALUE_BEFORE_RESET, go to reset

CMDLOOP:
	LBCO	r0, CONST_PRUDRAM, 0, 4 // Load to r12 the content of CONST_PRUDRAM with offset 0, and the 4 bytes
	QBEQ	CHECK_CYCLECNT, r0, 0 // loop until we get an instruction
	QBEQ	CHECK_CYCLECNT, r0, 1 // loop until we get an instruction
	// ok, we have an instruction. Assume it means 'begin capture'
	LED_OFF // Indicate that we start acquisiton of timetagging
	LBCO	r2, CONST_PRUCTRLREG, 0, 4 // store the current value of CYCLECOUNT into r2.
	MOV	r5, r2 // store the current value of CYCLECOUNT into r5.
	// Reset CYCLECNT // We make CYCLECNT relative from this point to maximize the chances that it will not overflow
	CLR	r2.t3
	SBCO	r2, CONST_PRUCTRLREG, 0, 4
	LBCO	r2, CONST_PRUCTRLREG, 0, 4 // r2 maps de CYCLECNT
	SET	r2.t3
	SBCO  	r2, CONST_PRUCTRLREG, 0, 4
		
WAIT_FOR_EVENT: // At least dark counts will be detected so detections will happen
	// Load the value of R31 into a working register, say R0
	MOV 	r0, r31
	// Mask the relevant bits you're interested in
	// For example, if you're interested in any of the first 8 bits being high, you could use 0xFF as the mask
	AND 	r0, r0, MASKevents
	// Compare the result with 0. If it's 0, no relevant bits are high, so loop
	QBNE 	WAIT_FOR_EVENT, r0, 0
	// If the program reaches this point, at least one of the bits is high
	// Proceed with the rest of the program

TIMETAG:
	// Time part
	LBCO	r2, CONST_PRUCTRLREG, 0, 4 // r2 maps de CYCLECNT
	SBBO 	r2, r1, 0, 4 // Put contents of CYCLEcount into the address at r1.
	ADD 	r1, r1, 4 // increment address by 4 bytes // This can be improved
	// Here include the auxiliary CYCLECNT count
	SBBO 	r5, r1, 0, 4 // Put contents of overflowCYCLEcount into the address at r1
	ADD 	r1, r1, 4 // increment address by 4 bytes // This can be improved
	// Here include the overflow register
	SBBO 	r3, r1, 0, 4 // Put contents of overflowCYCLEcount into the address at r1
	ADD 	r1, r1, 4 // increment address by 4 bytes // This can be improved	
	// Channels detection
	SBBO 	r0.b0, r1, 0, 1 // Put contents of r0 into the address at r1
	ADD 	r1, r1, 4 // increment address by 4 bytes // This can be improved
	
	// Check to see if we still need to read more data
	SUB 	r4, r4, 1
	QBNE 	WAIT_FOR_EVENT, r4, 0 // loop if we've not finished
//	SET r30.t11	// disable the data bus
	
	// we're done. Signal to the application		
	LED_ON// this signals that we are done with the timetagging acqusition
	MOV 	r0, 1
	SBCO 	r0, CONST_PRUDRAM, 0, 4 // Put contents of r1 into CONST_PRUDRAM
	JMP 	START1 // finished, wait for next command. So it continuosly loops	
	
EXIT:
	// Send notification to Host for program completion
	MOV 	R31.b0, PRU0_R31_VEC_VALID | PRU_EVTOUT_0
	MOV 	r31.b0, PRU0_ARM_INTERRUPT+16

	// Halt the processor
	HALT

ERR:
	LED_ON
	JMP ERR
