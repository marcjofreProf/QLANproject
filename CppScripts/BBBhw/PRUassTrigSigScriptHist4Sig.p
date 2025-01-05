// This signals scripts outputs iteratively channel 1, then channel 2, channel 3, channel 4 and then a time off (also to allow for some management); copied for two output signals . Outputing at maxium 100 MHz.
// PRU-ICSS program to control realtime GPIO pins
// But only if the Pinmux Mode has been set correctly with a device  
 // tree overlay!  
 //  
 // Assemble in BBB with:  
 // pasm -b PRUassTrigSigScriptHist4Sig.p
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
#define NUM_REPETITIONS		1024	//Not used 4294967295	// Maximum value possible storable to limit the number of cycles in 32 bits register. This is wuite limited in number but very controllable (maybe more than one register can be used). This defines the Maximum Transmission Unit - coul dbe named Quantum MTU (defined together with the clock)
#define DELAYMODULE	4096// Slightly above the std jitter of the 24 MHz hardware clock//65536 // Not used. The whole histogram period. It has to be divisible by the 2^32 total clock register.
#define DELAYHALFMODULE	4095//65535 // Not used. One unit less than DELAYMODULE the power of two required of the period of the histogram which is 65535-1=2^16-1.

// Refer to this mapping in the file - pruss_intc_mapping.h
#define PRU0_PRU1_INTERRUPT     17
#define PRU1_PRU0_INTERRUPT     18
#define PRU0_ARM_INTERRUPT      19
#define PRU1_ARM_INTERRUPT      20
#define ARM_PRU0_INTERRUPT      21
#define ARM_PRU1_INTERRUPT      22


// The constant table registers are common for both PRU (so they share the same values)
#define CONST_PRUCFG         C4
#define CONST_PRUDRAM        C24 // allow the PRU to map portions of the system's memory into its own address space. In particular we will map its own data RAM
#define CONST_IETREG	     C26 //

#define OWN_RAM              0x00000000 // current PRU data RAM
#define OWN_RAMoffset	     0x00000200 // Offset from Base OWN_RAM to avoid collision with some data tht PRU might store
#define PRU1_CTRL            0x240

// Beaglebone Black has 32 bit registers (for instance Beaglebone AI has 64 bits and more than 2 PRU)

// *** LED routines, so that LED USR0 can be used for some simple debugging
// *** Affects: r28, r29. Each PRU has its of 32 registers
.macro LED_OFF
	MOV		r28, 1<<21
	MOV		r29, GPIO2_BANK | GPIO_CLEARDATAOUToffset
	SBBO	r28, r29, 0, 4
.endm

.macro LED_ON
	MOV		r28, 1<<21
	MOV		r29, GPIO2_BANK | GPIO_SETDATAOUToffset
	SBBO	r28, r29, 0, 4
.endm

// r0 is arbitrary used for operations
// r1 is reserved with the number NUM_REPETITIONS - storing the PRU 1 DATA number of repetitions
//// If using the cycle counte rin the PRU (not adjusted to synchronization protocols)
// We cannot use Constan table pointers since the base addresses are too far
// r2 reserved for 0x22000 Control register. Not really needed
// r3 reserved for 0x2200C DWT_CYCCNT. Not really needed
// r4 reserved for zeroing registers
// r5 reserved for delay count
// r6 reserved for half period of delay module
// r7 reserved for period of delay module
// r9 reserved for count value of when it need to be corrected the intra relative frequency difference

// r10 is arbitrary used for operations

// r11 is reserved for emitting state 1 and 2 (w0 and w1)
// r12 is reserved for emitting state 3 and 4 (w0 and w1)

// r13 is reserved for enabling IEP counter

// r14 is reserved for ON state time
// r15 is reserved for OFF state time
// r16 is reserved for periodic offset and frequency correction

// r19 is reserved for counter of when to correct the intra relative frequency difference
// r20 is reserved for storing the absolute correction value for intra relative frequency difference correction

// r28 is mainly used for LED indicators operations
// r29 is mainly used for LED indicators operations
// r30 is reserved for output pins
// r31 is reserved for inputs pins
INITIATIONS:
	SET     r30.t11	// enable the data bus for initiating the OCP master. it may be necessary to disable the bus to one peripheral while another is in use to prevent conflicts or manage bandwidth.
//	MOV r1, GPIO2_BANK | GPIO_SETDATAOUToffset  // load the address to we wish to set to r1. Note that the operation GPIO2_BANK+GPIO_SETDATAOUT is performed by the assembler at compile time and the resulting constant value is used. The addition is NOT done at runtime by the PRU!
//	MOV r2, GPIO2_BANK | GPIO_CLEARDATAOUToffset // load the address we wish to cleare to r2. Note that every bit that is a 1 will turn off the associated GPIO we do NOT write a 0 to turn it off. 0's are simply ignored.
		
	LBCO	r0, CONST_PRUCFG, 4, 4 // Enable OCP master port
	// OCP master port is the protocol to enable communication between the PRUs and the host processor
	CLR		r0, r0, 4         // Clear SYSCFG[STANDBY_INIT] to enable OCP master port
	SBCO	r0, CONST_PRUCFG, 4, 4

	// Configure the programmable pointer register for PRU by setting c24_pointer // related to pru data RAM. Where the commands will be found
	// This will make C24 point to 0x00000000 (PRU data RAM).
	MOV	r0, OWN_RAM | OWN_RAMoffset// | OWN_RAMoffset
	MOV	r10, 0x24000+0x20// | C24add//CONST_PRUDRAM
	SBBO	r0, r10, 0, 4//SBCO	r0, CONST_PRUDRAM, 0, 4  // Load the base address of PRU0 Data RAM into C24
	
	// Initial initializations
	LDI	r4, 0 // zeroing
	LDI r13, 0x00000111
	
	// Initial Re-initialization for IET counter
	// The Clock gating Register controls the state of Clock Management. 
	//LBCO 	r0, CONST_PRUCFG, 0x10, 4                    
	MOV 	r0, 0x24924
	SBCO 	r0, CONST_PRUCFG, 0x10, 4 
	//LBCO	r2, CONST_IETREG, 0, 1 //
	//SET ocp_clk:1 or of iep_clk:0// It is important to select the clock source to be in synch with the PRU clock. Seems that ocp_clk allows adjustments and other controls, but eip_clk runs at 200 MHz and much more robust.
	// https://mythopoeic.org/BBB-PRU/am335xPruReferenceGuide.pdf
	LDI		r0, 1
	SBCO 	r0, CONST_PRUCFG, 0x30, 4
	// IEP configuration
	MOV		r0, 0x111 // Enable and Define increment value to 1
	SBCO	r0, CONST_IETREG, 0, 4 // Enables IET count and sets configuration
	// Deactivate IEP compensation
	SBCO 	r4, CONST_IETREG, 0x08, 4

	// Using cycle counter// Not really using it
	MOV	r2, 0x22000
	MOV	r3, 0x2200C
	
	// Initializations - some, just in case
	LDI	r30, 0 // All signal pins down
	LDI	r4, 0 // zeroing
	MOV	r1, NUM_REPETITIONS// Initial initialization jus tin case// Cannot be done with LDI instruction because it may be a value larger than 65535. load r3 with the number of cycles. For the time being only up to 65535 ->develop so that it can be higher
	MOV	r6, DELAYHALFMODULE
	MOV	r7, DELAYMODULE
	LDI	r0, 0 // Ensure reset commands
	MOV	r9, 0xFFFFFFFF
	LDI	r14, 6 // ON state
	LDI	r15, 6 // OFF state
	LDI r16, 0 // Periodic offset and frequency correction
	MOV	r19, 0xFFFFFFFF // update counter of when to correct intra relative frequency difference
	MOV	r20, DELAYMODULE
	
	MOV	r11, 0x02220111
	MOV	r12, 0x08880444
	
//	LED_ON	// just for signaling initiations
//	LED_OFF	// just for signaling initiations

CMDLOOP:
	QBBC	CMDLOOP, r31, 31
	SBCO	r4.b0, C0, 0x24, 1 // Reset host interrupt
CMDLOOP2:// Double verification of host sending start command
	LBCO	r0.b0, CONST_PRUDRAM, 0, 1 // Load to r0 the content of CONST_PRUDRAM with offset 4, and 1 bytes
	QBEQ	CMDLOOP, r0.b0, 0 // loop until we get an instruction
	SBCO	r4.b0, CONST_PRUDRAM, 0, 1 // We remove the command from the host (in case there is a reset from host, we are saved) 1 bytes.
	//MOV 	r31.b0, PRU1_ARM_INTERRUPT+16// Here send interrupt to host to measure time
	// Start executing
	//CLR     r30.t11	// disable the data bus. it may be necessary to disable the bus to one peripheral while another is in use to prevent conflicts or manage bandwidth.
CMDSEL:
	QBEQ	PERIODICTIMESYNCHCHECK, r0.b0, 10 // 10 command is measure IEP timer status and so a check
	QBEQ	QUADEMT1, r0.b0, 1 // QBEQ	PSEUDOSYNCH, r0.b0, 1 // 1 command is generate signals
	QBEQ	QUADEMT2, r0.b0, 2 // QBEQ	PSEUDOSYNCH, r0.b0, 1 // 1 command is generate signals
	QBEQ	QUADEMT3, r0.b0, 3 // QBEQ	PSEUDOSYNCH, r0.b0, 1 // 1 command is generate signals
	QBEQ	QUADEMT4, r0.b0, 4 // QBEQ	PSEUDOSYNCH, r0.b0, 1 // 1 command is generate signals
	QBEQ	QUADEMT5, r0.b0, 5 // QBEQ	PSEUDOSYNCH, r0.b0, 1 // 1 command is generate signals
	QBEQ	QUADEMT6, r0.b0, 6 // QBEQ	PSEUDOSYNCH, r0.b0, 1 // 1 command is generate signals
	QBEQ	QUADEMT7, r0.b0, 7 // QBEQ	PSEUDOSYNCH, r0.b0, 1 // 1 command is generate signals
	QBEQ	PERIODICTIMESYNCHSUB, r0.b0, 8 // 8 command is measure IEP timer status and so a substraction correction
	QBEQ	PERIODICTIMESYNCHADD, r0.b0, 9 // 9 command is measure IEP timer status and so a addition correction	
	QBEQ	PERIODICTIMESYNCHSET, r0.b0, 11 // 11 command is measure IEP timer set synch
PERIODICTIMESYNCHSET: // with command coded 11 means setting synch
	CLR     r30.t11	// disable the data bus. it may be necessary to disable the bus to one peripheral while another is in use to prevent conflicts or manage bandwidth.
	SBCO	r4.b0, CONST_IETREG, 0, 1 // Stop the counter
	LBCO	r0, CONST_IETREG, 0xC, 4 // Sample IEP counter periodically
	//LBCO	r7, CONST_PRUDRAM, 12, 4 // Read from PRU RAM value
	SBCO	r4, CONST_IETREG, 0xC, 4 // Correct IEP counter periodically
	SBCO	r13.b0, CONST_IETREG, 0, 1 // Enable the counter
	SBCO	r0, CONST_PRUDRAM, 8, 4 // Store in PRU RAM position the IEP current sample
	SET     r30.t11	// enable the data bus. it may be necessary to disable the bus to one peripheral while another is in use to prevent conflicts or manage bandwidth.	
	MOV 	r31.b0, PRU1_ARM_INTERRUPT+16// Send finish interrupt to host
	JMP		CMDLOOP
PERIODICTIMESYNCHCHECK: // with command coded 10 means chech synch only	(but also store the offset correction value)
	CLR     r30.t11	// disable the data bus. it may be necessary to disable the bus to one peripheral while another is in use to prevent conflicts or manage bandwidth.
	LBCO	r0, CONST_IETREG, 0xC, 4 // Sample IEP counter periodically
	SBCO	r0, CONST_PRUDRAM, 8, 4 // Store in PRU RAM position the IEP current sample
	SET     r30.t11	// enable the data bus. it may be necessary to disable the bus to one peripheral while another is in use to prevent conflicts or manage bandwidth.
	MOV 	r31.b0, PRU1_ARM_INTERRUPT+16// Send finish interrupt to host
	JMP		CMDLOOP
PERIODICTIMESYNCHADD: // with command coded 9 means synch by reseting the IEP timer
	CLR     r30.t11	// disable the data bus. it may be necessary to disable the bus to one peripheral while another is in use to prevent conflicts or manage bandwidth.
	LBCO	r0, CONST_IETREG, 0xC, 4 // Sample IEP counter periodically
	LBCO	r7, CONST_PRUDRAM, 12, 4 // Read from PRU RAM value
	ADD		r0, r0, r7 // Apply correction
	//SBCO	r4.b0, CONST_IETREG, 0, 1 // Stop the counter
	SBCO	r0, CONST_IETREG, 0xC, 4 // Correct IEP counter periodically
	//SBCO	r13.b0, CONST_IETREG, 0, 1 // Enable the counter
	SBCO	r0, CONST_PRUDRAM, 8, 4 // Store in PRU RAM position the IEP current sample
	SET     r30.t11	// enable the data bus. it may be necessary to disable the bus to one peripheral while another is in use to prevent conflicts or manage bandwidth.	
	MOV 	r31.b0, PRU1_ARM_INTERRUPT+16// Send finish interrupt to host
	JMP		CMDLOOP
PERIODICTIMESYNCHSUB: // with command coded 8 means synch by reseting the IEP timer
	CLR     r30.t11	// disable the data bus. it may be necessary to disable the bus to one peripheral while another is in use to prevent conflicts or manage bandwidth.
	LBCO	r0, CONST_IETREG, 0xC, 4 // Sample IEP counter periodically
	LBCO	r7, CONST_PRUDRAM, 12, 4 // Read from PRU RAM value
	SUB		r0, r0, r7 // Apply correction
	//SBCO	r4.b0, CONST_IETREG, 0, 1 // Stop the counter
	SBCO	r0, CONST_IETREG, 0xC, 4 // Correct IEP counter periodically
	//SBCO	r13.b0, CONST_IETREG, 0, 1 // Enable the counter
	SBCO	r0, CONST_PRUDRAM, 8, 4 // Store in PRU RAM position the IEP current sample
	SET     r30.t11	// enable the data bus. it may be necessary to disable the bus to one peripheral while another is in use to prevent conflicts or manage bandwidth.
	MOV 	r31.b0, PRU1_ARM_INTERRUPT+16// Send finish interrupt to host
	JMP		CMDLOOP
QUADEMT7:
	MOV	r11, 0x02220111
	MOV	r12, 0x08880444
	JMP	PSEUDOSYNCH
QUADEMT6:
	MOV	r11, 0x02200110
	MOV	r12, 0x08800440
	JMP	PSEUDOSYNCH
QUADEMT5:
	MOV	r11, 0x02020101
	MOV	r12, 0x08080404
	JMP	PSEUDOSYNCH
QUADEMT4:
	MOV	r11, 0x02000100
	MOV	r12, 0x08000400
	JMP	PSEUDOSYNCH
QUADEMT3:
	MOV	r11, 0x00220011
	MOV	r12, 0x00880044
	JMP	PSEUDOSYNCH
QUADEMT2:
	MOV	r11, 0x00200010
	MOV	r12, 0x00800040
	JMP	PSEUDOSYNCH
QUADEMT1:
	MOV	r11, 0x00020001
	MOV	r12, 0x00080004
	JMP	PSEUDOSYNCH
PSEUDOSYNCH:// Neutralizing interrupt jitter time //I belive this synch first because it depends on IEP counter// Only needed at the beggining to remove the unsynchronisms of starting to emit at specific bins for the histogram or signal. It is not meant to correct the absolute time, but to correct for the difference in time of emission due to entering thorugh an interrupt. So the period should be small (not 65536). For instance (power of 2) larger than the below calculations and slightly larger than the interrupt time (maybe 40 60 counts). Maybe 64 is a good number.
	CLR     r30.t11	// disable the data bus. it may be necessary to disable the bus to one peripheral while another is in use to prevent conflicts or manage bandwidth.
	LBCO 	r1, CONST_PRUDRAM, 4, 4 // Load number of repetitons of the signal
	LBCO	r7, CONST_PRUDRAM, 12, 4 // Read from PRU RAM sequence guard period
	LBCO	r14, CONST_PRUDRAM, 20, 4 // ON state time
	LBCO	r15, CONST_PRUDRAM, 28, 4 // OFF state time
	SUB 	r14, r14, 4 // Substract 4 because is the compensation value
	LSR		r14, r14, 1 // Divide by two because loop consumes double	
//	ADD 	r14, r14, 1 // ADD 1 to not have a substraction below zero which halts. Do not add 1 because it adds a relative frequency difference
	SUB 	r15, r15, 4 // Substract 4 because is the compensation value
	LSR		r15, r15, 1 // Divide by two because loop consumes double	
//	ADD 	r15, r15, 1 // ADD 1 to not have a substraction below zero which halts. Do not add 1 because it adds a relative frequency difference
PERIODICOFFSET:// Neutralizing hardware clock relative frequency difference and offset drift//
	LBCO	r16, CONST_PRUDRAM, 8, 4 // Read from PRU RAM periodic offset correction
MANAGECALC: // To be develop to correct for intra pulses frequency variation
	LBCO	r9, CONST_PRUDRAM, 16, 4 // Load from PRU RAM position the count of when to correct intra relative frequency difference
	MOV 	r19, r9 // update counter of when to correct intra relative frequency difference
	LBCO	r20, CONST_PRUDRAM, 24, 4 // Load from PRU RAM position the absolute correction to correct for intra relative frequency difference
	// To give some sense of synchronization with the other PRU time tagging, wait for IEP timer (which has been enabled and nobody resets it and so it wraps around)
	SUB		r6, r7, 1 // Generate the value for r6
ABSSYNCH:	// From this point synchronization is very important. If the previous operations takes longer than the period below to synch, in the cpp script it can be added some extra periods to compensate for frequency relative offset
	LBCO	r0, CONST_IETREG, 0xC, 4//LBCO	r0, CONST_IETREG, 0xC, 4//LBBO	r0, r3, 0, 4//LBCO	r0.b0, CONST_IETREG, 0xC, 4. Read the IEP counter
	AND		r0, r0, r6 //Maybe it can not be done because larger than 255. Implement module of power of 2 on the histogram period// Since the signals have a minimum period of 2 clock cycles and there are 4 combinations (Ch1, Ch2, Ch3, Ch4, NoCh) but with a long periodicity of for example 1024 we can get a value between 0 and 7
	SUB		r0, r7, r0 // Substract to find how long to wait	
	LSR		r0, r0, 1// Divide by two because the PSEUDOSYNCHLOOP consumes double
	ADD		r0, r0, 1// ADD 1 to not have a substraction below zero which halts
PSEUDOSYNCHLOOP:
	SUB		r0, r0, 1
	QBNE	PSEUDOSYNCHLOOP, r0, 0 // Coincides with a 0
PERIODICOFFSETLOOP:
	SUB		r16, r16, 1
	QBNE	PERIODICOFFSETLOOP, r16, 0 // Coincides with a 0
//BASICPSEUDOSYNCH:
//	AND	r0, r0, 0x07 // Implement module of power of 2 on the histogram period// Since the signals have a minimum period of 2 clock cycles and there are 4 combinations (Ch1, Ch2, Ch3, Ch4, NoCh) we can get a value between 0 and 7
//	QBEQ	SIGNALON1, r0.b0, 7 // Coincides with a 7
//	QBEQ	SIGNALON1, r0.b0, 6 // Coincides with a 6
//	QBEQ	SIGNALON1, r0.b0, 5 // Coincides with a 5
//	QBEQ	SIGNALON1, r0.b0, 4 // Coincides with a 4
//	QBEQ	SIGNALON1, r0.b0, 3 // Coincides with a 3
//	QBEQ	SIGNALON1, r0.b0, 2 // Coincides with a 2
//	QBEQ	SIGNALON1, r0.b0, 1 // Coincides with a 1
//	QBEQ	SIGNALON1, r0.b0, 0 // Coincides with a 0
SIGNALON1:	// The odd signals actually carry the signal (so it is half of the period, adjusting the on time); while the even signals are the half period alway off
	MOV		r30.w0, r11.w0 // Double channels 1. write to magic r30 output byte 0. Half word bytes= 7,6,5,4,3,2,1,0 bits
	MOV		r5, r14
	LDI		r4, 0 // Intentionally controlled delay to adjust all sequences (in particular to the last one)
	LDI		r4, 0 // Intentionally controlled delay to adjust all sequences (in particular to the last one)
//	MOV		r30.w0, 0x0000 // All off
SIGNALON1DEL:
	SUB		r5, r5, 1
	QBNE	SIGNALON1DEL, r5, 0
//	LDI		r4, 0 // Controlled intentional delay to account for the fact that QBNE takes one extra count when it does not go through the barrier
SIGNALON2: // Make use of this dead time to instantly correct for intra relative frequency sifference
	MOV		r30.w0, 0x0000 // All off
	MOV		r5, r15
//	LDI		r4, 0 // Intentionally controlled delay to adjust all sequences (in particular to the last one)
	SUB 	r19, r19, 1 // Decrement counter of when to correct for intra relative frequency difference
//	LDI		r4, 0 // Intentionally controlled delay to adjust all sequences (in particular to the last one)
	QBNE	SIGNALON2DEL, r19, 0 // If it does not coincide with a zero, jump to SIGNALON2DEL
//	LDI		r4, 0 // Intentionally controlled delay to adjust all sequences (in particular to the last one)
INTRACORRFREQ:
	MOV		r19, r9 // REload the counter value
	MOV		r5, r20 // Reload the exclusive value for this correction
INTRACORRFREQLOOP:
	SUB		r5, r5, 1
	QBNE	INTRACORRFREQLOOP, r5, 0
	LDI		r4, 0 // Intentionally controlled delay to adjust all sequences (in particular to the last one)
	QBEQ	SIGNALON3, r4, 0 //JMP		SIGNALON3
SIGNALON2DEL:
	SUB		r5, r5, 1
	QBNE	SIGNALON2DEL, r5, 0
//	LDI		r4, 0 // Controlled intentional delay to account for the fact that QBNE takes one extra count when it does not go through the barrier
SIGNALON3:
	MOV		r30.w0, r11.w2 // Double channels 2. write to magic r30 output byte 0. Half word=15,14,13,12,11,10,9,8 bits
	MOV		r5, r14
	LDI		r4, 0 // Intentionally controlled delay to adjust all sequences (in particular to the last one)
	LDI		r4, 0 // Intentionally controlled delay to adjust all sequences (in particular to the last one)
//	MOV		r30.w0, 0x0000 // All off
SIGNALON3DEL:
	SUB		r5, r5, 1
	QBNE	SIGNALON3DEL, r5, 0
//	LDI		r4, 0 // Controlled intentional delay to account for the fact that QBNE takes one extra count when it does not go through the barrier
SIGNALON4:
	MOV		r30.w0, 0x0000 // All off
	MOV		r5, r15
	LDI		r4, 0 // Intentionally controlled delay to adjust all sequences (in particular to the last one)
	LDI		r4, 0 // Intentionally controlled delay to adjust all sequences (in particular to the last one)
//	LDI		r4, 0 // Intentionally controlled delay to adjust all sequences (in particular to the last one)
SIGNALON4DEL:
	SUB		r5, r5, 1
	QBNE	SIGNALON4DEL, r5, 0
//	LDI		r4, 0 // Controlled intentional delay to account for the fact that QBNE takes one extra count when it does not go through the barrier
SIGNALON5:
	MOV		r30.w0, r12.w0 // Double channels 3. write to magic r30 output byte 0
	MOV		r5, r14
	LDI		r4, 0 // Intentionally controlled delay to adjust all sequences (in particular to the last one)
	LDI		r4, 0 // Intentionally controlled delay to adjust all sequences (in particular to the last one)
//	MOV		r30.w0, 0x0000 // All off
SIGNALON5DEL:
	SUB		r5, r5, 1
	QBNE	SIGNALON5DEL, r5, 0
//	LDI		r4, 0 // Controlled intentional delay to account for the fact that QBNE takes one extra count when it does not go through the barrier
SIGNALON6:
	MOV		r30.w0, 0x0000 // All off
	MOV		r5, r15
	LDI		r4, 0 // Intentionally controlled delay to adjust all sequences (in particular to the last one)
	LDI		r4, 0 // Intentionally controlled delay to adjust all sequences (in particular to the last one)
//	LDI		r4, 0 // Intentionally controlled delay to adjust all sequences (in particular to the last one)
SIGNALON6DEL:
	SUB		r5, r5, 1
	QBNE	SIGNALON6DEL, r5, 0
//	LDI		r4, 0 // Controlled intentional delay to account for the fact that QBNE takes one extra count when it does not go through the barrier
SIGNALON7:
	MOV		r30.w0, r12.w2 // Double channels 4. write to magic r30 output byte 0
	MOV		r5, r14
	LDI		r4, 0 // Intentionally controlled delay to adjust all sequences (in particular to the last one)
	LDI		r4, 0 // Intentionally controlled delay to adjust all sequences (in particular to the last one)
//	MOV		r30.w0, 0x0000 // All off
SIGNALON7DEL:
	SUB		r5, r5, 1
	QBNE	SIGNALON7DEL, r5, 0
//	LDI		r4, 0 // Controlled intentional delay to account for the fact that QBNE takes one extra count when it does not go through the barrier
SIGNALON8:
	MOV		r30.w0, 0x0000 // All off
	MOV		r5, r15 // No extra controlled delay since two instructions below as well in FINISH and one here
//	LDI		r4, 0 // Intentionally controlled delay to adjust all sequences (in particular to the last one)
SIGNALON8DEL:
	SUB		r5, r5, 1
	QBNE	SIGNALON8DEL, r5, 0
//	LDI		r4, 0 // Controlled intentional delay to adjust
FINISH:
	SUB		r1, r1, 1 // Decrement counter
	QBNE	SIGNALON1, r1, 0 // condition jump to SIGNALON because we have not finished the number of repetitions
//	LDI		r4, 0 // Controlled intentional delay to account for the fact that QBNE takes one extra count when it does not go through the barrier
	//QBA	SIGNALON1//PSEUDOSYNCH// Debbuging - Infinite loop
//	MOV		r0, DELAY
//DELAYOFF:
//	SUB 	r0, r0, 1
//	QBNE 	DELAYOFF, r0, 0
FINISHLOOP:
	// The following lines do not consume "signal speed"
	SET     r30.t11	// enable the data bus. it may be necessary to disable the bus to one peripheral while another is in use to prevent conflicts or manage bandwidth.
	MOV 	r31.b0, PRU1_ARM_INTERRUPT+16// Notification sent at the beginning of the signal//SBCO	r5.b0, CONST_PRUDRAM, 4, 1 // Put contents of r0 into CONST_PRUDRAM// code 1 means that we have finished.This can be substituted by an interrupt: MOV 	r31.b0, PRU1_ARM_INTERRUPT+16
	JMP		CMDLOOP // Might consume more than one clock (maybe 3) but always the same amount
EXIT:
	SET     r30.t11	// enable the data bus. it may be necessary to disable the bus to one peripheral while another is in use to prevent conflicts or manage bandwidth.
	HALT
ERR:	// Signal error
	SET     r30.t11	// enable the data bus. it may be necessary to disable the bus to one peripheral while another is in use to prevent conflicts or manage bandwidth.
	LED_ON
//	JMP INITIATIONS
//	JMP ERR
	HALT
