	.globl	_init

_init:
**************************************************************************
*      Start Initialization code                                         *
**************************************************************************
	or	800h,ST			;Enable the Cache

*********************************************
*      Enable interrupts                    *
*********************************************
	ldi	9h,IIF			;Configure iof pins

	rets

	.end