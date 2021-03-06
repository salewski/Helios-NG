
/* C compiler file xpuops.h :  Copyright (C) A.Mycroft and A.C.Norman */
/* version 0.02a */
/* $Id: xpuops.h,v 1.1 1995/05/19 11:38:17 nickc Exp $ */

#ifndef _XPUOPS_LOADED

#define _XPUOPS_LOADED 1


#define MinInt	0x80000000


/* Transputer opcodes... */

/* direct functions */
#define f_j            0x0
#define f_ldlp         0x1
#define f_pfix         0x2
#define f_ldnl         0x3
#define f_ldc          0x4
#define f_ldnlp        0x5
#define f_nfix         0x6
#define f_ldl          0x7
#define f_adc          0x8
#define f_call         0x9
#define f_cj           0xa
#define f_ajw          0xb
#define f_eqc          0xc
#define f_stl          0xd
#define f_stnl         0xe
#define f_opr          0xf

/* operations are 0x00 + operand	*/
#define op_rev           0x00
#define op_lb            0x01
#define op_bsub          0x02
#define op_endp          0x03
#define op_diff          0x04
#define op_add           0x05
#define op_gcall         0x06
#define op_in            0x07
#define op_prod          0x08
#define op_gt            0x09
#define op_wsub          0x0a
#define op_out           0x0b
#define op_sub           0x0c
#define op_startp        0x0d
#define op_outbyte       0x0e
#define op_outword       0x0f

#define op_seterr        0x10

#define op_resetch       0x12
#define op_csub0         0x13

#define op_stopp         0x15
#define op_ladd          0x16
#define op_stlb          0x17
#define op_sthf          0x18
#define op_norm          0x19
#define op_ldiv          0x1a
#define op_ldpi          0x1b
#define op_stlf          0x1c
#define op_xdble         0x1d
#define op_ldpri         0x1e
#define op_rem           0x1f
#define op_ret           0x20
#define op_lend          0x21
#define op_ldtimer       0x22
#define op_testlds	 0x23
#define op_testlde	 0x24
#define op_testldd	 0x25
#define op_teststs	 0x26
#define op_testste	 0x27
#define op_teststd	 0x28
#define op_testerr       0x29
#define op_testpranal    0x2a
#define op_tin           0x2b
#define op_div           0x2c
#define op_testhardchan  0x2d

#define op_dist          0x2e
#define op_disc          0x2f
#define op_diss          0x30
#define op_lmul          0x31
#define op_not           0x32
#define op_xor           0x33
#define op_bcnt          0x34
#define op_lshr          0x35
#define op_lshl          0x36
#define op_lsum          0x37
#define op_lsub          0x38
#define op_runp          0x39
#define op_xword         0x3a
#define op_sb            0x3b
#define op_gajw          0x3c
#define op_savel         0x3d
#define op_saveh         0x3e
#define op_wcnt          0x3f
#define op_shr           0x40
#define op_shl           0x41
#define op_mint          0x42
#define op_alt           0x43
#define op_altwt         0x44
#define op_altend        0x45
#define op_and           0x46
#define op_enbt          0x47
#define op_enbc          0x48  
#define op_enbs          0x49
#define op_move          0x4a
#define op_or            0x4b
#define op_csngl         0x4c
#define op_ccnt1         0x4d
#define op_talt          0x4e
#define op_ldiff         0x4f
#define op_sthb          0x50
#define op_taltwt        0x51
#define op_sum           0x52
#define op_mul           0x53
#define op_sttimer       0x54
#define op_stoperr       0x55
#define op_cword         0x56
#define op_clrhalterr    0x57
#define op_sethalterr    0x58
#define op_testhalterr   0x59
#define op_dup           0x5a    /* Only on T800 */
#define op_move2dinit    0x5b    /* Only on T800 */
#define op_move2dall     0x5c    /* Only on T800 */
#define op_move2dnonzero 0x5d    /* Only on T800 */
#define op_move2dzero    0x5e    /* Only on T800 */
#define op_gtu		 0x5f	 /* T9000 only */

#define op_unpacksn      0x63    /* Not on T800 */

#define op_postnormsn    0x6c    /* Not on T800 */
#define op_roundsn       0x6d    /* Not on T800 */

#define op_ldinf         0x71    /* Not on T800 */
#define op_fmul          0x72    /* Not on 16-bit Transputers (?) */
#define op_cflerr        0x73    /* Not on T800 */
#define op_crcword       0x74    /* Only on T800 */
#define op_crcbyte       0x75    /* Only on T800 */
#define op_bitcnt        0x76    /* Only on T800 */
#define op_bitrevword    0x77    /* Only on T800 */
#define op_bitrevnbits   0x78    /* Only on T800 */
#define op_pop		 0x79	  /* T425 only */
#define op_timerdisableh 0x7a	  /* T425 only */
#define op_timerdisablel 0x7b	  /* T425 only */
#define op_timerenableh  0x7c	  /* T425 only */
#define op_timerenablel  0x7d	  /* T425 only */
#define op_ldmemstartval 0x7e	  /* T425 only */

#define op_wsubdb        0x81    /* Only on T800 */
#define op_fpldndbi      0x82    /* All the rest are T800 only */
#define op_fpchkerr      0x83
#define op_fpstnldb      0x84

#define op_fpldnlsni     0x86
#define op_fpadd         0x87
#define op_fpstnlsn      0x88
#define op_fpsub         0x89
#define op_fpldnldb      0x8a
#define op_fpmul         0x8b
#define op_fpdiv         0x8c
#define op_fprange	 0x8d	/* T9000 only */
#define op_fpldnlsn      0x8e
#define op_fpremfirst    0x8f
#define op_fpremstep     0x90
#define op_fpnan         0x91
#define op_fpordered     0x92
#define op_fpnotfinite   0x93
#define op_fpgt          0x94
#define op_fpeq          0x95
#define op_fpi32tor32    0x96
#define op_fpge		 0x97	/* T9000 only */
#define op_fpi32tor64    0x98

#define op_fpb32tor64    0x9a
#define op_fplg		 0x9b	/* T9000 only */
#define op_fptesterr     0x9c
#define op_fprtoi32      0x9d
#define op_fpstnli32     0x9e
#define op_fpldzerosn    0x9f
#define op_fpldzerodb    0xa0
#define op_fpint         0xa1

#define op_fpdup         0xa3
#define op_fprev         0xa4

#define op_fpldnladddb   0xa6

#define op_fpldnlmuldb   0xa8

#define op_fpldnladdsn   0xaa
#define op_fpentry       0xab
#define op_fpldnlmulsn   0xac

#define op_settimeslice	 0xb0	/* T9000 */
#define op_break         0xb1	/* T425 only */
#define op_clrj0break    0xb2	/* T425 only */
#define op_setj0break    0xb3	/* T425 only */
#define op_testj0break   0xb4	/* T425 only */

#define op_ldflags	0xb6	/* T9000 only */
#define op_stflags	0xb7	/* T9000 only */
#define op_xbword	0xb8	/* T9000 only */
#define op_lbx		0xb9	/* T9000 only */
#define op_cb		0xba	/* T9000 only */
#define op_cbu		0xbb	/* T9000 only */
#define op_insphdr	0xbc	/* T9000 only */
#define op_readbfr	0xbd	/* T9000 only */
#define op_ldconf	0xbe	/* T9000 only */
#define op_stconf	0xbf	/* T9000 only */
#define op_ldcnt	0xc0	/* T9000 only */
#define op_ssub		0xc1	/* T9000 only */
#define op_ldth		0xc2	/* T9000 only */
#define op_ldchstatus	0xc3	/* T9000 only */
#define op_intdis	0xc4	/* T9000 only */
#define op_intenb	0xc5	/* T9000 only */
#define op_cir		0xc7	/* T9000 only */
#define op_ss		0xc8	/* T9000 only */
#define op_chantype	0xc9	/* T9000 only */
#define op_ls		0xca	/* T9000 only */
#define op_fpseterr	0xcb	/* T9000 only */
#define op_ciru		0xcc	/* T9000 only */
#define op_fprem	0xcf	/* T9000 only */
#define op_fprn		0xd0	/* T9000 only */
#define op_fpdivby2	0xd1	/* T9000 only */
#define op_fpmulby2	0xd2	/* T9000 only */
#define op_fpsqrt	0xd3	/* T9000 only */
#define op_fprp		0xd4	/* T9000 only */
#define op_fprm		0xd5	/* T9000 only */
#define op_fprz		0xd6	/* T9000 only */
#define op_fpr32tor64	0xd7	/* T9000 only */
#define op_fpr64tor32	0xd8	/* T9000 only */
#define op_fpexpdec32	0xd9	/* T9000 only */
#define op_fpexpinc32	0xda	/* T9000 only */
#define op_fpabs	0xdb	/* T9000 only */
#define op_fpclrerr	0xdc	/* T9000 only */
#define op_fpadddbsn	0xdd	/* T9000 only */
#define op_fpchki32	0xde	/* T9000 only */
#define op_fpchki64	0xdf	/* T9000 only */
#define op_devlb	0xf0	/* T9000 only */
#define op_devsb	0xf1	/* T9000 only */
#define op_devls	0xf2	/* T9000 only */
#define op_devss	0xf3	/* T9000 only */
#define op_devlw	0xf4	/* T9000 only */
#define op_devsw	0xf5	/* T9000 only */
#define op_xsword	0xf8	/* T9000 only */
#define op_lsx		0xf9	/* T9000 only */
#define op_cs		0xfa	/* T9000 only */
#define op_csu		0xfb	/* T9000 only */

#define negop(p,o)	(((~(p))<<4)|(o))

#define op_fpstall	negop(0x0,0xf)
#define op_fpldall	negop(0x0,0xe)
#define op_stshadow	negop(0x0,0xd)
#define	op_ldshadow	negop(0x0,0xc)
#define	op_tret		negop(0x0,0xb)
#define	op_goprot	negop(0x0,0xa)
#define	op_selth	negop(0x0,0x9)
#define	op_syscall	negop(0x0,0x8)
#define	op_swapgstatus	negop(0x0,0x7)
#define	op_wait		negop(0x0,0x5)
#define	op_signal	negop(0x0,0x4)
#define	op_timeslice	negop(0x0,0x3)
#define	op_insertqueue	negop(0x0,0x2)
#define	op_swaptimer	negop(0x0,0x1)
#define	op_swapqueue	negop(0x0,0x0)
#define	op_stopch	negop(0x1,0xe)
#define	op_vout		negop(0x1,0xd)
#define	op_vin		negop(0x1,0xc)
#define	op_swapbfr	negop(0x1,0x9)
#define	op_sethdr	negop(0x1,0x8)
#define	op_setchmode	negop(0x1,0x7)
#define	op_initvlcb	negop(0x1,0x6)
#define	op_writehdr	negop(0x1,0x5)
#define	op_readhdr	negop(0x1,0x4)
#define	op_disg		negop(0x1,0x3)
#define	op_enbg		negop(0x1,0x2)
#define	op_grant	negop(0x1,0x1)
#define	op_stmove2dinit	negop(0x1,0x0)
#define	op_causeerror	negop(0x2,0xf)
#define	op_unmkrc	negop(0x2,0xd)
#define	op_mkrc		negop(0x2,0xc)
#define	op_irdsq	negop(0x2,0xb)
#define	op_erdsq	negop(0x2,0xa)
#define	op_stresptr	negop(0x2,0x9)
#define	op_ldresptr	negop(0x2,0x8)
#define	op_devmove	negop(0x2,0x4)
#define	op_icl		negop(0x2,0x3)
#define	op_fdcl		negop(0x2,0x2)
#define	op_ica		negop(0x2,0x1)
#define	op_fdca		negop(0x2,0x0)
#define	op_nop		negop(0x3,0x0)
#define op_ldprodid	negop(0x8,0xc)

/* fpu operations are 0x20000 + operand	*/

#define op_fpusqrtfirst   0x20001
#define op_fpusqrtstep    0x20002
#define op_fpusqrtlast    0x20003
#define op_fpurp          0x20004
#define op_fpurm          0x20005
#define op_fpurz          0x20006
#define op_fpur32tor64    0x20007
#define op_fpur64tor32    0x20008
#define op_fpuexpdec32    0x20009
#define op_fpuexpinc32    0x2000a
#define op_fpuabs         0x2000b

#define op_fpunoround     0x2000d
#define op_fpuchki32      0x2000e
#define op_fpuchki64      0x2000f

#define op_fpudivby2      0x20011
#define op_fpumulby2      0x20012

#define op_fpurn          0x20022
#define op_fpuseterr      0x20023

#define op_fpuclrerr      0x2009c

/* the following are 3 byte operates */
#define op_start	  0x200ff
#define op_lddevid	  0x2007c

/* pseudo operators, expand to real ones in back end */

#define p_infoline	0x30000		/* line info		*/
#define p_call		0x30004		/* fn call macro	*/
#define p_callpv	0x30005		/* proc var call macro	*/
#define p_fnstack	0x30006		/* stack adjustment	*/
#define p_ldx		0x30007		/* load external	*/
#define p_stx		0x30008		/* store external	*/
#define p_ldxp		0x30009		/* load external pointer*/
#define p_ret		0x3000a		/* return macro		*/
#define p_ldpi		0x3000b		/* load pointer to code */
#define p_ldpf		0x3000c		/* load pointer to func */
#define p_setv1		0x3000d		/* initialise vector	*/
#define p_setv2		0x3000e		/* initialise vector	*/
#define p_lds		0x3000f		/* load static		*/
#define p_sts		0x30010		/* store static		*/
#define p_ldsp		0x30011		/* load static pointer	*/
#define p_j             0x30012         /* ldc 0; cj            */
#define p_noop          0x30013         /* no op                */
#define p_ldvl          0x30014         /* load virtual local   */
#define p_ldvlp         0x30015         /* load virtual local   */
#define p_stvl          0x30016         /* store virtual local  */
#define p_fpcall	0x30017		/* fp emulator call	*/
#define p_case		0x30018		/* case jump table	*/
#define p_noop2		0x30019		/* for non-empty blocks */
#define p_stackframe	0x3001a		/* load stackframe	*/
#define	p_xword		0x3001b		/* Sign extend	 	*/
#define	p_mask		0x3001c		/* Mask down 		*/

#endif

/* end of xpuops.h */
