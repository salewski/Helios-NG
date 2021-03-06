#ifdef USE_NORCROFT_PRAGMAS
#pragma force_top_level
#pragma include_only_once
#endif
/*
 * C compiler file aeops.h, Version 35
 * Copyright (C) Codemist Ltd., 1988.
 */

/*
 * RCS $Revision: 1.1 $
 * Checkin $Date: 1992/03/23 14:59:38 $
 * Revising $Author: nickc $
 */

#ifndef _aeops_LOADED
#define _aeops_LOADED 1

/* ACN 31-10-88: add some things needed for BCPL    */
/* ACN 11-12-86: add some things needed for Pascal. */

/* Notes (AM):
   1) some lexical operators, like ++ and & correspond to more
      than one AE tree operation (pre/post incr. and bitand/addrof).
      The lexical routines are all assumed to return the operator
      corresponding to the binary or prefix form.
      Macros unaryop_() and postop_() are here provided for the syntax
      analyser to use for getting the other form.
   2) Assignment operators are treated similarly.  This time by
      the lexical routines which use assignop_() to get the
      assignment form.
   3) It goes without saying that the caller must only apply
      these macros to valid arguments.
   4) s_and provides a good view of this.
   5) some AE operators do not have tokens (e.g. s_fnap, s_block).
      I have tried to enclose these in parens in the following.
*/

/* Here is a list of all tokens types used in C                        */

#define  s_nothing              0L

/* identifier, constants: */
#define  s_identifier           1L
#define  s_integer              2L
#define  s_floatcon             3L
#define  s_character            4L
#define  s_string               5L
#define  s_wcharacter           6L
#define  s_wstring              7L
/* s_binder heads binding records - see type Binder */
#define  s_binder               8L
#define  s_error                9L
#define  s_tagbind              10L /* AM may want this soon */

#ifdef RANGECHECK_SUPPORTED
#  define hastypefield_(op)       ((s_invisible<=(op) && (op)<=s_cast) || (op)==s_rangecheck || (op)==s_checknot)
#else
#  define hastypefield_(op)       ((s_invisible<=(op) && (op)<=s_cast))
#endif
#define  s_invisible            11L
#define isdiad_(op)             (s_andand<=(op) && (op)<=s_assign)
/* expression operators, mainly non-letter symbols */
#define  s_andand               12L
#define  s_comma                13L
#define  s_oror                 14L

/* relations: reorder to make NOT or SWAP easier? */
#define isrelational_(op) (s_equalequal<=(op) && (op)<=s_lessequal)
#define  s_equalequal           15L
#define  s_notequal             16L
#define isinequality_(op) (s_greater<=(op) && (op)<=s_lessequal)
#define  s_greater              17L
#define  s_greaterequal         18L
#define  s_less                 19L
#define  s_lessequal            20L

/* NB: the next 3 blocks of opcodes must correspond. */

#define floatableop_(op) \
    (isrelational_(op)||(s_times<=(op) && (op)<=s_div)||(op) == s_cond)

#define  s_and                  21L
#define  s_times                22L
#define  s_plus                 23L
#define  s_minus                24L
#define  s_power                25L     /* Needed for Fortran */
#define  s_div                  26L
#define  s_leftshift            27L
#define  s_or                   28L
#define  s_idiv                 29L
#define  s_rem                  30L
#define  s_rightshift           31L
#define  s_xor                  32L

#define can_have_becomes(x) (((x)>= s_and) && ((x) <= s_xor))
#define and_becomes(x)  ((x)+(s_andequal-s_and))
#define assignop_(op)   ((op)+(s_andequal-s_and))
#define unassignop_(op) ((op)-(s_andequal-s_and))
#define isassignop_(x) (((x)>= s_andequal) && ((x) <= s_xorequal))

#define  s_andequal             33L
#define  s_timesequal           34L
#define  s_plusequal            35L
#define  s_minusequal           36L
#define  s_powerequal           37L     /* here for consistency - do not remove */
#define  s_divequal             38L
#define  s_leftshiftequal       39L
#define  s_orequal              40L
#define  s_idivequal            41L
#define  s_remequal             42L
#define  s_rightshiftequal      43L
#define  s_xorequal             44L

#define diadneedslvalue_(op)    (s_andequal<=(op) && (op)<=s_assign)
#define  s_displace             45L     /* no repn in C,  ++ is spec case */
#define  s_assign               46L

/* unary ops.  note that the first 4 correspond (via unaryop_()) to diads */
#define ismonad_(op)   (s_addrof<=(op) && (op)<=s_postdec)
#define unaryop_(x)    ((x) + (s_addrof-s_and))
#define  s_addrof               47L
#define  s_content              48L
#define  s_monplus              49L
#define  s_neg                  50L
#define  s_bitnot               51L
#define  s_boolnot              52L
/* move the next block of 4 to just before s_addrof? */
#define monadneedslvalue_(op)   (isincdec_(op) || (op)==s_addrof)
#define isincdec_(op)           (s_plusplus<=(op) && (op)<=s_postdec)
#define incdecisinc_(op)        ((op) <= s_postinc)
#define incdecispre_(op)        ((op) & 1L)
#define  postop_(preop)         ((preop)+(s_postinc-s_plusplus))
#define  s_plusplus             53L
#define  s_postinc              54L
#define  s_minusminus           55L
#define  s_postdec              56L
/* end of unary ops */
#define  s_dot                  57L
#define  s_arrow                58L
#define  s_cond                 59L

/* pseudo expression operators */
#define  s_fnap                 60L
#define  s_fnapstruct           61L
#define  s_subscript            62L
#define  s_let                  63L
#ifdef EXTENSION_VALOF
#define  s_valof                64L     /* BCPL-like valof block support */
#endif
/* @@@@@@@@ AM: s_rangecheck should go here!  (see hastypefield_().)     */
#define  s_cast                 65L
/* see hastypefield_() above */
#define  s_sizeoftype           66L
#define  s_sizeofexpr           67L
#define  s_typeoftype           68L
#define  s_typeofexpr           69L
#define  s_ptrdiff              70L

/* command nodes (mainly keywords): */
#define  s_array                71L
#define  s_begin                72L
#define  s_break                73L
#define  s_case                 74L
#define  s_colon                75L
#define  s_continue             76L
#define  s_default              77L
#define  s_do                   78L
#define  s_downto               79L
#define  s_else                 80L
#define  s_end                  81L
#define  s_endcase              82L  /* C switch break = bcpl endcase */
#define  s_file                 83L
#define  s_for                  84L
#define  s_function             85L
#define  s_goto                 86L
#define  s_if                   87L
#define  s_in                   88L
#define  s_label                89L
#define  s_nil                  90L
#define  s_of                   91L
#define  s_packed               92L
#define  s_procedure            93L
#define  s_program              94L
#define  s_record               95L
#define  s_repeat               96L
#define  s_return               97L
#define  s_semicolon            98L
#define  s_set                  99L
#define  s_switch              100L
#define  s_then                101L
#define  s_to                  102L
#define  s_type                103L
#define  s_until               104L
#define  s_var                 105L
#define  s_while               106L
#define  s_with                107L
#define  s_block              (108L)

#ifdef EXTENSION_VALOF
#define  s_resultis            109L     /* used with valof blocks */
#endif

/* declaration nodes: */
#define  s_decl                110L
#define  s_fndef               111L
#define  s_typespec            112L

/* note the next two blocks must be adjacent for the next 3 tests. */
#define istypestarter_(x)  (s_char<=(x) && (x)<=s_volatile)
#define isstorageclass_(x) (s_auto<=(x) && (x)<=s_register)
#define isdeclstarter_(x)  (s_char<=(x) && (x)<=s_typestartsym)
#define shiftoftype_(x)    ((x)-s_char)
#define bitoftype_(x)      (1L<<((x)-s_char))
#define  s_char                113L
#define  s_double              114L
#define  s_enum                115L
#define  s_float               116L
#define  s_int                 117L
#define  s_struct              118L
#define  s_union               119L
#define  s_void                120L
#define  s_typedefname        (121L)
#define NONQUALTYPEBITS      0x1FFL
/* modifiers last (high bits for m&-m trick) */
#define  s_long                122L
#define  s_short               123L
#define  s_signed              124L
#define  s_unsigned            125L
#define TYPEDEFINHIBITORS    0x1FFFL  /* ie everything above here */
#define  s_const               126L
#define  s_volatile            127L
#define NUM_OF_TYPES       (s_volatile-s_char+1)
/* storage classes and qualifiers */
#define bitofstg_(x)        bitoftype_(x)
#define PRINCSTGBITS       (bitoftype_(s_register)-bitoftype_(s_auto))
#define STGBITS            (bitoftype_(s_register+1)-bitoftype_(s_auto))
#define  s_auto                128L
#define  s_extern              129L
#define  s_static              130L
#define  s_typedef             131L
#define  s_register            132L
/* N.B. s_register is equivalent to 0x80000.  See h.modes for other bits */
#define  s_typestartsym        133L   /* used to help error reporting */

/* impedementia (not appearing in parse trees) */
#define  s_toplevel            134L
#define  s_lbrace              135L
#define  s_lbracket            136L
#define  s_lpar                137L
#define  s_rbrace              138L
#define  s_rbracket            139L
#define  s_rpar                140L
#define  s_typeof              141L  /* 2 special cases above */
#define  s_sizeof              142L  /* 2 special cases above */
#define  s_ellipsis            143L
#define  s_eol                 144L
#define  s_eof                 145L

/* spare: 146-151                                                       */

/* Here are some symbols that BCPL seems to want...                 */
/* inserted so that an experiment with a BCPL parser can happen.    */

#define  s_global              152L
#define  s_manifest            153L
#define  s_abs                 154L
#define  s_get                 155L
#define  s_eqv                 156L
#define  s_query               157L
#define  s_vecap               158L
#define  s_andlet              159L
#define  s_be                  160L
#define  s_by                  161L
#define  s_false               162L
#define  s_finish              163L
#define  s_into                164L
#define  s_repeatwhile         165L
#define  s_repeatuntil         166L
#define  s_test                167L
#define  s_true                168L
#define  s_table               169L
#define  s_unless              170L
#define  s_vec                 171L
#define  s_valdef              172L

#define  s_boolean             173L      /* needed for Fortran */
#define  s_complex             174L      /* needed for Fortran */

#define  s_rangecheck         (175L)     /* This DOES appear in parse trees */
#define  s_checknot           (176L)     /* and so does this */

/* remember NUMSYMS is 1 greater than the last number used.
*/
#define  s_NUMSYMS             177L

extern char *sym_name_table[s_NUMSYMS];

/* synonyms (used in types for clarity) */

#define t_fnap s_fnap
#define t_subscript s_subscript
#define t_content s_content

#endif

/* end of file aeops.h */

