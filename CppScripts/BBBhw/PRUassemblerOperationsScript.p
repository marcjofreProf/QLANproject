// prucode_adc.p
//
// BBB Schematic  BBB port Assign Bit
// -------------  -------- ------ ------------
// LCD_DATA0      P8.45    D0     PRU1_R31_0
// LCD_DATA1      P8.46    D1     PRU1_R31_1
// LCD_DATA2      P8.43    D2     PRU1_R31_2
// LCD_DATA3      P8.44    D3     PRU1_R31_3
// LCD_DATA4      P8.41    D4     PRU1_R31_4
// LCD_DATA5      P8.42    D5     PRU1_R31_5
// LCD_DATA6      P8.39    D6     PRU1_R31_6
// LCD_DATA7      P8.40    D7     PRU1_R31_7
// LCD_PCLK       P8.28    CLK    PRU1_R31_10
// LCD_DE         P8.30    *EN    PRU1_R30_11
                                  

.origin 0
.entrypoint START

#include "prucode_adc.hp"

#define GPIO1 0x4804c000
#define GPIO_CLEARDATAOUT 0x190
#define GPIO_SETDATAOUT 0x194

#define SHARED 0x10000

#define MASK0 0x000000ff

// Memory location of command:
#define CMD 0x00010000

// Memory location where to store the data to be acquired:
#define ACQRAM 0x00010004
// Length of acquisition:
#define RECORDS 2000

// *** LED routines, so that LED USR0 can be used for some simple debugging
// *** Affects: r2, r3
.macro LED_OFF
		MOV r2, 1<<21
    MOV r3, GPIO1 | GPIO_CLEARDATAOUT
    SBBO r2, r3, 0, 4
.endm

.macro LED_ON
		MOV r2, 1<<21
    MOV r3, GPIO1 | GPIO_SETDATAOUT
    SBBO r2, r3, 0, 4
.endm

.macro UDEL
UDEL:
		MOV r14, 5
UDEL1:
		SUB r14, r14, 1
		QBNE UDEL1, r14, 0 // loop if we've not finished
.endm

.macro DEL
DEL:
		MOV r1, 50000
DEL1:
		SUB r1, r1, 1
		QBNE DEL1, r1, 0 // loop if we've not finished
.endm


START:

    // Enable OCP master port
    LBCO      r0, CONST_PRUCFG, 4, 4
    CLR     r0, r0, 4         // Clear SYSCFG[STANDBY_INIT] to enable OCP master port
    SBCO      r0, CONST_PRUCFG, 4, 4

    // Configure the programmable pointer register for PRU0 by setting c28_pointer[15:0]
    // field to 0x0100.  This will make C28 point to 0x00010000 (PRU shared RAM).
    MOV     r0, 0x00000100
    MOV       r1, CTPPR_0
    ST32      r0, r1

    // Configure the programmable pointer register for PRU0 by setting c31_pointer[15:0]
    // field to 0x0010.  This will make C31 point to 0x80001000 (DDR memory).
    MOV     r0, 0x00100000
    MOV       r1, CTPPR_1
    ST32      r0, r1

    //Load values from external DDR Memory into Registers R0/R1/R2
    LBCO      r0, CONST_DDR, 0, 12

    //Store values from read from the DDR memory into PRU shared RAM
    //SBCO      r0, CONST_PRUSHAREDRAM, 0, 12


		LED_OFF


START1:
		SET r30.t11	// disable the data bus
		MOV r6, ACQRAM // PRU shared RAM
		MOV r4, MASK0
		MOV r7, RECORDS // This will be the loop counter to read the entire set of data
		MOV r0, 7	// pipeline bit counter
		MOV r11, CMD
		// wait for command
CMDLOOP:
		DEL
		LBCO r12, CONST_PRUSHAREDRAM, 0, 4
		QBEQ CMDLOOP, r12, 0 // loop until we get an instruction
		QBEQ CMDLOOP, r12, 1 // loop until we get an instruction
		// ok, we have an instruction. Assume it means 'begin capture'
		

CAPTURE:
		CLR r30.t11	// enable data bus
CAPTURE1:
		// wait for 7th rising edge, since we have a pipeline in the ADC
		WBC r31.t10 // wait for clock to be low
		WBS r31.t10 // wait for clock rising edge
		SUB r0, r0, 1 // decrement pipeline counter
		QBNE CAPTURE1, r0, 0 // loop until we have completed the pipeline delay
		// ok, on the next (i.e. 8th) rising edge, we must start capturing

CAPTURELOOP:
		// First byte:
		WBC r31.t10	// Ensure CLK is actually low
WAITRISE1:
		// Now wait until CLK rising edge
		WBS r31.t10
		MOV r2, r31	// Read in the data (i.e. after 5nsec of clock rising edge)
		MOV r2, r31	// Read in the data (i.e. after 5nsec of clock rising edge)
		MOV r2, r31	// Read in the data (i.e. after 5nsec of clock rising edge)
		//AND r1, r2, r4 // We're just interested in a portion of r31 (i.e. 8 bits)
		// we can now write it to RAM (address in r6)
		SBBO r2.b0, r6, 0, 1 // Put contents of r1 into the address at r6
		ADD r6, r6, 4 // increment DDR address by 4 bytes
		// Check to see if we still need to read more data
		SUB r7, r7, 1
		QBNE CAPTURELOOP, r7, 0 // loop if we've not finished
		SET r30.t11	// disable the data bus
		
		// we're done. Signal to the application		
		LED_ON
		MOV r1, 1
		SBCO r1, CONST_PRUSHAREDRAM, 0, 4 // Put contents of r1 into shared RAM
		JMP START1 // finished, wait for next command		
		



EXIT:
    // Send notification to Host for program completion
    MOV       r31.b0, PRU1_ARM_INTERRUPT+16

    // Halt the processor
    HALT

ERR:
	LED_ON
	JMP ERR