// PRUassTaggDetScriptSimple.p
// Time Tagging functionality on PRU0 with Shared Memory Access (not Direct Memory Access nor DDR)
// It is pending studying if it is worth making use of DMA or DDR in terms of larger memory space or transfer speed

.origin 0
.entrypoint INITIATIONS

#include "PRUassTaggDetScript.hp"

#define MASKevents 0x0F // P9_28-31, which corresponds to r31 bits 0,1,2,3

// Length of acquisition:
#define RECORDS 1024 // readings and it matches in the host c++ script. Not really used because updated from cpp host

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
// r3 reserved for mapping the initial synch period
// r4 reserved for holding the RECORDS (re-loaded at each iteration)
// r5 reserved for holding the DWT_CYCCNT count value
// r6 reserved because detected channels are concatenated with r5 in the write to SHARED RAM
// r7 reserved for 0 value (zeroing registers)
// r8 reserved for cycle count final threshold reset
// r9 reserved for offset correction

// r10 is arbitrary used for operations

// r11 reserved for detection pins mask
//// If using the cycle counte rin the PRU (not adjusted to synchronization protocols)
// We cannot use Constan table pointers since the base addresses are too far
// r12 reserved for 0x22000 Control register
// r13 reserved for 0x2200C DWT_CYCCNT
// r14 reserved for storing the substraction of offset value

// r16 reserved for raising edge detection operation together with r6
// r17 might be used for some intermediate operations

//// If using IET timer (potentially adjusted to synchronization protocols)
// We can use Constant table pointers C26
// CONST_IETREG 0x0002E000
// IET Count 0xC offset

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
	MOV	r0, OWN_RAM | OWN_RAMoffset// | OWN_RAMoffset // When using assembler, the PRU does not put data in the first addresses of OWN_RAM (when using c++ PRU direct programming the PRU  might use some initial addresses of OWN_RAM space
	MOV	r10, 0x24000+0x20// | C24add//CONST_PRUDRAM
	SBBO	r0, r10, 0, 4//SBCO	r0, CONST_PRUDRAM, 0, 4  // Load the base address of PRU0 Data RAM into C24
	
//	// This will make C26 point to 0x0002E000 (IEP).
//	MOV	r0, 0x0002E000//
//	SBCO	r0, CONST_IETREG, 0, 4  // Load the base address of IEP

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
	LDI 	r5, 0 // Initialize for the first time r5
	LDI	r6, 0 // Initialization
	LDI	r16, 0 // Initialization
	LDI	r1, 0 //MOV	r1, 0  // reset r1 address to point at the beggining of PRU shared RAM
	MOV	r4, RECORDS // This will be the loop counter to read the entire set of data
	// Initializations for faster execution
	LDI	r7, 0 // Register for clearing other registers
	// Initiate to zero for counters of skew and offset
	LDI	r8, 0
	LDI	r9, 0
	MOV	r14, 0xFFFFFFFF
	MOV	r11, 0xC000C0FF // detection mask. Bits might be moved out of position
	LDI	r17, 0
	
	// Initial Re-initialization of DWT_CYCCNT
	LBBO	r2, r12, 0, 1 // r2 maps b0 control register
	CLR	r2.t3
	SBBO	r2, r12, 0, 1 // stops DWT_CYCCNT
	SBBO	r7, r13, 0, 4 // reset DWT_CYCNT
	LBBO	r2, r12, 0, 1 // r2 maps b0 control register
	SET	r2.t3
	//SBBO	r2, r12, 0, 1 // Enables DWT_CYCCNT. We start it when the commands enters
		
	// Initial Re-initialization for IET counter. Used in the other PRU1
	// The Clock gating Register controls the state of Clock Management
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
//	SBCO 	r7, CONST_IETREG, 0x08, 4
	
CMDLOOP:
	QBBC	CMDLOOP, r31, 30	// Reception or not of the host interrupt
	SBCO	r7.b0, C0, 0x24, 1 // Reset host interrupt	
CMDLOOP2:// Double verification of host sending start command
	LBCO	r0.b0, CONST_PRUDRAM, 0, 1 // Load to r0 the content of CONST_PRUDRAM with offset 0, and 1 bytes. It is the command to start
	QBEQ	CMDLOOP, r0.b0, 0 // loop until we get an instruction
	//MOV 	r31.b0, PRU0_ARM_INTERRUPT+16// Here send interrupt to host to measure time
DWTSTART:
	// Re-start DWT_CYCNT
	SBBO	r2, r12, 0, 1 // Enables DWT_CYCCNT
//	CLR     r30.t11	// disable the data bus. it may be necessary to disable the bus to one peripheral while another is in use to prevent conflicts or manage bandwidth.
	// Some loadings and resets
	LBCO	r4, CONST_PRUDRAM, 4, 4 // Load to r4 the content of CONST_PRUDRAM with offset 4, and 4 bytes. It is the number of RECORDS
	SBCO	r7.b0, CONST_PRUDRAM, 0, 1 // Store a 0 in CONST_PRUDRAM with offset 0, and 1 bytes. Reset the command to start
PSEUDOSYNCH:// Only needed at the beggining to remove the unsynchronisms of starting to receiving at specific bins for the histogram or signal. It is not meant to correct the absolute time, but to correct for the difference in time of emission due to entering through an interrupt. So the period should be small (not 65536). For instance (power of 2) larger than the below calculations and slightly larger than the interrupt time (maybe 40 60 counts). Maybe 64 is a good number.
	// Read the number of RECORDS from positon 0 of PRU1 DATA RAM and stored it
	LBCO	r10, CONST_PRUDRAM, 8, 4 // Read from PRU RAM offset signal period
	LBCO	r9, CONST_PRUDRAM, 12, 4 // Read from PRU RAM offset correction
	// To give some sense of synchronization with the other PRU time tagging, wait for IEP timer (which has been enabled and nobody resets it and so it wraps around)
	SUB	r3, r10, 1 // Generate the value for r3 from r10
	LBCO	r0, CONST_IETREG, 0xC, 4//LBCO	r0, CONST_IETREG, 0xC, 4//LBBO	r0, r3, 0, 4//LBCO	r0.b0, CONST_IETREG, 0xC, 4
	AND	r0, r0, r3 //Maybe it can not be done because larger than 255. Implement module of power of 2 on the histogram period// Since the signals have a minimum period of 2 clock cycles and there are 4 combinations (Ch1, Ch2, Ch3, Ch4, NoCh) but with a long periodicity of for example 1024 we can get a value between 0 and 7
	SUB	r0, r10, r0 // Substract to find how long to wait	
	LSR	r0, r0, 1// Divide by two because the PSEUDOSYNCHLOOP consumes double
	ADD	r0, r0, 1// ADD 1 to not have a substraction below zero which halts
PSEUDOSYNCHLOOP:
	SUB	r0, r0, 1
	QBNE	PSEUDOSYNCHLOOP, r0, 0 // Coincides with a 0
FINETIMEOFFSETADJ:
	MOV	r0, r9 // For security work with register r0
	LSR	r0, r0, 1// Divide by two because the FINETIMEOFFSETADJLOOP consumes double
	ADD	r0, r0, 1// ADD 1 to not have a substraction below zero which halts
FINETIMEOFFSETADJLOOP:
	SUB	r0, r0, 1
	QBNE	FINETIMEOFFSETADJLOOP, r0, 0 // Coincides with a 0
FIRSTREF:	
	// Store a calibration timetagg
	LBBO	r5, r13, 0, 4 // Read the value of DWT_CYCNT
	SBCO	r5, CONST_PRUDRAM, 8, 4// Calibration time tag (together with the acumulated synchronization error)	
	/// Relative synch count down
WAIT_FOR_EVENT: // At least dark counts will be detected so detections will happen
	// Load the value of R31 into a working register
	// Edge detection - No step in between (pulses have 1/3 of detection), can work with pulse rates of 75 MHz If we put one step in between we allow pulses to be detected with 1/2 chance. Neverthelss, separating by one operation, also makes the detection window to two steps hence 10ns, instead of 5ns.
	// MEasuring all pins of interest
	//MOV	r16.b3, r30.b1 // This wants to be zeros for edge detection to read the isolated ones in the other (bits 15 and 14) - also the time to read might be larger since using PRU1 pinouts. Limits the pulse rate to 50 MHz.
	MOV 	r16.w0, r31.w0 // This wants to be zeros for edge detection (bits 15, 14 and 7 to 0)
	//MOV	r6.b3, r30.b1 // Consecutive red for edge detection to read the isolated ones in the other (bits 15 and 14) - also the time to read might be larger since using PRU1 pinouts
	MOV	r6.w0, r31.w0 // Consecutive red for edge detection (bits 15, 14 and 7 to 0)	
	QBEQ 	WAIT_FOR_EVENT, r6, 0 // Do not lose time with the below if there are no detections
	// Combining all reading pins
	//AND	r16, r16, r11 // Mask to make sure there are no other info
	//AND	r6, r6, r11 // Mask to make sure there are no other info
	// Faster operations and less resources but maybe dangerous
	//LSR	r16.b3, r16.b3, 2
	//LSR	r6.b3, r6.b3, 2
	//OR	r16.b1, r16.b1, r16.b3// Combine the registers
	//OR	r6.b1, r6.b1, r6.b3// Combine the registers
	// Safer operations but mabe slower and more resources
	//MOV	r17.b0, r16.b3
	//MOV	r17.b1, r6.b3
	//LSR	r17, r17, 2
	//OR	r16.b1, r16.b1, r17.b0// Combine the registers
	//OR	r6.b1, r6.b1, r17.b1// Combine the registers
	// Edge detection with the pins of interest
	NOT	r16.w0, r16.w0 // 0s converted to 1s. This step can be placed here to increase chances of detection.	
	AND	r6.b0, r6.b0, r16.w0 // Only does complying with a rising edge// AND has to be done with the whole register, not a byte of it!!!!
CHECKDET:		
	QBEQ 	WAIT_FOR_EVENT, r6.w0, 0 //all the b0 above can be converted to w0 to capture more channels, but then in the chennel tag recorded has to be increaed and appropiatelly handled in c++ (also the number of tags per run has to be reduced)
	// If the program reaches this point, at least one of the bits is high
	LBBO	r5, r13, 0, 4 // Read the value of DWT_CYCNT
TIMETAG:
	// Faster Concatenated Time counter and Detection channels
	SBCO 	r5, CONST_PRUSHAREDRAM, r1, 6 // Put contents of r5 and r6.w0 of DWT_CYCCNT into the address offset at r1.
	ADD 	r1, r1, 6 // increment address by 6 bytes	
	// Check to see if we still need to read more data
	SUB 	r4, r4, 1
	QBNE 	WAIT_FOR_EVENT, r4, 0 // loop if we've not finished
FINISH:
	// Faster Concatenated Checks writting	
	SBCO 	r8, CONST_PRUSHAREDRAM, r1, 4 // writes values of r8
//	SET     r30.t11	// enable the data bus. it may be necessary to disable the bus to one peripheral while another is in use to prevent conflicts or manage bandwidth.
	LDI	r1, 0 //MOV	r1, 0  // reset r1 address to point at the beggining of PRU shared RAM
	// Prepare DWT_CYCNT for next round
	LBBO	r2, r12, 0, 1 // r2 maps b0 control register
	CLR	r2.t3
	SBBO	r2, r12, 0, 1 // stops DWT_CYCCNT
	SBBO	r7, r13, 0, 4 // reset DWT_CYCNT
	SET	r2.t3
	////////////////////////////////////////
	MOV	r31.b0, PRU0_ARM_INTERRUPT+16// Notification sent at the beginning of the signal//SBCO 	r17.b0, CONST_PRUDRAM, 4, 1 // Put contents of r0 into CONST_PRUDRAM// code 1 means that we have finished. This can be substituted by an interrupt: MOV 	r31.b0, PRU0_ARM_INTERRUPT+16
	//LED_ON // For signaling the end visually and also to give time to put the command in the OWN-RAM memory
	//LED_OFF	
	LBBO	r14, r13, 0, 4//LBCO	r9, CONST_IETREG, 0xC, 4 // read IEP	 // LBBO	r9, r13, 0, 4 // read DWT_CYCNT
	SUB	r8, r14, r5 // Use the last value of DWT_CYCNT
	JMP 	CMDLOOP // finished, wait for next command. So it continuosly loops	
EXIT:
	// Send notification (interrupt) to Host for program completion
	MOV 	r31.b0, PRU0_ARM_INTERRUPT+16
	// Halt the processor
	HALT

ERR:
	LED_ON
	JMP ERR
