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

#define RDATA	1	/* Input data (word count includes data words and
			   byte count word)
			   arg1:	# of bytes of data
			   args:	Data, packed ILDB-style into words.
			   Note:  Slave must always send fully populated
			   words except when EOF is reached. */

#define ROPENI	2	/* Input OPEN status; 1 or 11 arguments
			   arg1: -1 => succeeded >0 => OPEN loss number
			   arg2-arg11 are present only if arg1 is -1
			   arg2:	Real device name (usually SIXBIT/DSK/)
			   arg3:	 Real FN1
			   arg4:	Real FN2
			   arg5:	Real SNAME
			   arg6:	File length
			   arg7:	Byte size open in
			   arg8:	File length in byte size written in
			   arg9:	Byte size written in
			   arg10:	FILDTP - -1 if RFDATE wins on this file
			   arg11:	Creation data (if RFDATE wins) */

#define ROPENO	3	/* Output OPEN status.  1 or 11 arguments
			   arg1:	-1 => succeeded >0 => OPEN loss number
			   arg2-arg11 present only if arg1 is -1
			   arg2:	Real device name
			   arg3:	Real FN1
			   arg4:	Real FN2
			   arg5:	Real SNAME
			   arg6:	File length (-1 => FILLEN fails on
					this device)
			   arg7:	Byte size open in
			   arg8:	File length in byte size written in
			   arg9:	Byte size written in
			   arg10:	FILDTP - -1 if RFDATE wins on this file
			   arg11:	Creation data (if RFDATE wins) */

#define REOF	4	/* EOF on input
			   Ignored argument */

#define RFDELE	5	/* DELETE/RENAME status.
			   1st argument:
			   RH:	-1 => succeeded
				0 => failure code
			   LH:	-1 => DELETE or RENAME
				0 => RENAME while open
			   If RENAME while open call succeeded, there are
			   4 more arguments:
			   arg2:	Real device name
			   arg3:	Real FN1 (as renamed)
			   arg4:	Real FN2
			   arg5:	Real SNAME */

#define RNOOP	6	/* No-op reply
			   arg: Echo argument send by no-op command */

#define RACCES	7	/* Acknowledge access
			   arg:	Transaction number from CACCES */

#define RSYSCL	010	/* Respond to system call.  9 arguments
			   arg1:	<error code or 0>,,<# of times call
					skipped>
			   arg2:	1st value from system call
			   ...
			   arg9:	8th value from system call */

#define RICLOS	011	/* Respond to input CLOSE */

#define ROCLOS	012	/* Respond to output CLOSE */

#define RIOC	013	/* Reflect an IOC error
			   arg:	IOC error code, right-justified in word */

#define RREUSE	014	/* Acknowledge a CREUSE
			   Ignored argument */

#define DIR_MAX 204 /* Maximum number of files in a directory. */

typedef long long word_t;

extern int protoc_init (char *);
extern int protoc_open (int, char *, char *, char *, char *, int);
extern int protoc_iclose (int);
extern int protoc_oclose (int);
extern int protoc_read (int, word_t *, int);
extern int protoc_write (int, word_t *, int);
