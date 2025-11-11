.origin 0				// start of program in PRU memory
.entrypoint EXIT			// program entry point (for debbuger)
EXIT:
	HALT
ERR:	// Signal error
	HALT
