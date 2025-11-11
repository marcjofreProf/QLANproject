.origin 0				// start of program in PRU memory
.entrypoint EXIT			// program entry point (for debbuger)
EXIT:
	MOV	r30.b0, 0x00000000 // Turn off all pins
	HALT
ERR:	// Signal error
	MOV	r30.b0, 0x00000000 // Turn off all pins
	HALT
