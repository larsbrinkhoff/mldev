#define MLDEV_PORT 83


#define COPENI	1	/* OPEN for input
			   arg1:	Device
			   arg2:	FN1
			   arg3:	FN2
			   arg4:	SNAME
			   arg5:	OPEN mode */

#define COPENO	2	/* OPEN for output
			   arg1:	Device
			   arg2:	FN1
			   arg3:	FN2
			   arg4:	SNAME
			   arg5:	OPEN mode */

#define CDATA	3	/* Write data (Word count includes data words and
			   byte count word)
			   arg1:	# of bytes of data
			   args:	Data, packed ILDB-style into at most
					200 words */

#define CALLOC	4	/* Read allocate
			   Allocation in bytes */

#define CICLOS	5	/* Input CLOSE
			   Ignored argument */

#define COCLOS	6	/* Output CLOSE
			   Ignored argument */

#define CFDELE	7	/* DELETE or RENAME
			   arg1:	Device
			   arg2:	FN1
			   arg3:	FN2
			   arg4:	0 or new FN1
			   arg5:	0 or new FN2
			   arg6:	SNAME */

#define CRNMWO	010	/* RENAME while open
			   arg1:	new FN1
			   arg2:	new FN2 */

#define CNOOP	011	/* No-op
			   arg:	Echoed by the no-op reply */

#define CACCES	12	/* ACCESS
			   arg1:	Transaction number
			   arg2:	Access pointer
			   arg3:	New allocation */

#define CSYSCL	13	/* Random system call.  11 (decimal) arguments
			   arg1:	System call name in sixbit
			   arg2:	Flags (control-bit argument)
			   arg3:	Number of following arguments that
					have significance
			   arg4:	1st argument to system call
			   ...
			   arg11:	8th argument to system call */

#define CREUSE	14	/* Reinitialize yourself, because we are about to
			   be reused */
