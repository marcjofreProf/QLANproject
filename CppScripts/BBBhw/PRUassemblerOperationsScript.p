// PRU-ICSS program to operate and allow TimeTagging and Direct Memory Access management
// But only if the Pinmux Mode has been set correctly with a device  
 // tree overlay!  
 //  
 // Assemble in BBB with:  
 // pasm -b PRUassemblerOperationsScript.p
 
.origin 0				// start of program in PRU memory
.entrypoint START			// program entry point (for debbuger)

#define INS_PER_US		200	// 5ns per instruction
#define INS_PER_DELAY_LOOP	2	// two instructions per delay loop


