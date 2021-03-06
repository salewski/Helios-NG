/*
 * C pre-processor, cfe/pp.c
 * Copyright (C) Codemist Ltd., 1988.
 * Copyright (C) Acorn Computers Ltd., 1988
 */

/*
 * RCS $Revision: 1.11 $ Codemist 163
 * Checkin $Date: 1992/10/20 09:05:58 $
 * Revising $Author: nickc $
 */

/* AM, july 90: fix pp recursion problem in macro args.                 */

/* AM: ansi ambiguities:                                                */
/*  #defines: i(x)=[x]  a=( b=) does i(i a 3 b)->[[3]] or [i(3)]?       */
/* AM: bugs:                                                            */
/* 1. #error/#ident/#pragma do not parse strings.                       */
/* 2. In PCC mode, two characters are stolen from the (255) char set    */
/*    for PP_TOKSEP/PP_NOEXPAND.  In ANSI mode we use \f and \v since   */
/*    these get converted to \n quite early.  (Find RARE_char).         */
/*    Note also that char 0 is also stolen.  The default setting        */
/*    of CORRECT_IS_BETTER_THAN_FAST may upset Japanese/Inmos.          */
/*    Clearly this edifice is in need of neatening.                     */
/*    AM is looking at using LDS's suggestion of using TAB as esc char. */
/* 3. #define: j(x)=i(x) i(x)=h##x f=g+f  then j(f) *should* be hg+f!   */
/* AM: beware the different ANSI/PCC rules for PP_TOKSEP.               */
/* AM: memo: rework/kill PP_NOEXPAND in favour of pp_noexpand.  Also,   */
/*    does the use of pp_hashalive resolve ANSI ambiguities as we want? */
/* AM: memo: rework the pp_unrdch() call in pp_checkid(no '(').         */

#ifndef NO_VERSION_STRINGS
extern char pp_version[];
char pp_version[] = "\npp.c $Revision: 1.11 $ 163\n";
#endif

/* The following hack supports ebcdic, pure ascii or iso8859.           */
/* We could (more portably) turn each macro into a test on compiler     */
/* (not preprocessor) character constants.                              */
#if 'A' == 193          /* nasty, ansi-non-guaranteed test for ebcdic   */
#  define RARE_char1 ((char) 1)         /* only used in PCC mode        */
#  define RARE_char2 ((char) 2)         /* only used in PCC mode        */
#  define legal_non_isprint(ch) 0
#else                                   /* ascii presumed               */
/*
 * Note the use of -ve values: conversion to both signed and unsigned
 * chars is defined and yields bit patterns corresponding to ISO8859
 * top-bit-set-control characters (effectively not in the source char set).
 * Sadly, the conversion of 129 and 130 to signed char is implementation
 * defined and, consequently a portability hazard.  But not half as much
 * as the (in retrospect!) mistake of stealing 2 source char codes!
 */
#  define RARE_char1 ((char) -127)      /* only used in PCC mode        */
#  define RARE_char2 ((char) -126)      /* only used in PCC mode        */
/* legal_non_isprint has the same input type requirement as 'isprint',  */
/* etc.  i.e. a value in the range 0..UCHAR_MAX or EOF.                 */


#  ifdef INMOSC
     /* On IBM PC-like hosts all chars seem legal so we merely ensure   */
     /* that 'magic' chars are faulted.  In ANSI mode there are none of */
     /* such.  Please discuss this code with Codemist.                  */
#    define legal_non_isprint(ch) \
           ((ch) >= 128 && (ch) != PP_TOKSEP && (ch) != PP_NOEXPAND)
#    define CORRECT_IS_BETTER_THAN_FAST 1
#  else
#    define legal_non_isprint(ch) ((ch) >= 128+32)
#  endif
#endif

#if defined COMPILING_ON_UNIX || defined __hp9000s700
/*
 * XXX - added by NC as not defining this seems to corrupt the parsing
 * of include files in ANSI mode.  Still not quite sure why this define
 * is not on by default
 */
#define CORRECT_IS_BETTER_THAN_FAST 1
#undef legal_non_isprint
#    define legal_non_isprint(ch) \
           ((ch) >= 128 && (ch) != PP_TOKSEP && (ch) != PP_NOEXPAND)

#endif


/*
 * NOTE: This file will need adjustment for use with Pascal and Fortran.
 * For now, it is left almost unaltered, but with some conversions disabled
 * in a crude manner, contingent on #ifdef PASCAL, #ifdef FORTRAN, etc.
 */

/* N.B. This pre-processor conforms to the May 86 draft ANSI spec.       */
/* However, there it was defined by example and the Oct 86 draft gave    */
/* a more stringent definition by (more) examples!  I doubt that it is   */
/* up to the latter spec, but propose to do nothing about this until the */
/* next draft spec or standard as further changes are proposed.          */
/* AM Jan 90: AM believes that the macro processing is very close to     */
/* the latest (Dec 88) ANSI draft modulo ambiguities.                    */
/* 29-Dec-87: AM has put the profile_xxx stuff here while it is in flux  */
/* so that he can try to understand them!  In a separate file one day?   */
/* Observation: the profile instrumentation fails (due to strcmp) if     */
/* a file is included more than once or #line is used.                   */
/* Memo: chase out '@@@'s.  Also abuf for non-essential uses.            */

#include <stdio.h>
#include <ctype.h>
#include <stddef.h>
#include <time.h>
#ifdef __STDC__
#  include <string.h>
#  include <stdlib.h>
#  include <limits.h>             /* for UCHAR_MAX */
#else
#  define UCHAR_MAX 255
#  include <strings.h>
extern char *malloc();
extern void free();
#endif
#include "globals.h"
#include "pp.h"
#include "syn.h"
#include "store.h"
#include "errors.h"

#ifndef NO_INSTORE_FILES

typedef struct {            /* this typedef should be in headers.c now  */
    char *name;
    int content;
} header_files;

#include "headers.c"        /* Tables for in-store headers */
#endif

#define NOT_A_CHARACTER  (512)                 /* cannot be saved in a char */
                                               /* only to be used in exprs  */
#ifdef PASCAL
#  define PASCAL_OR_FORTRAN  1
#  define COMMENT_START     '('
#  define COMMENT_END       ')'
#else
#  ifdef FORTRAN
#    define PASCAL_OR_FORTRAN  1
#    define COMMENT_START  NOT_A_CHARACTER     /* this will NEVER be found! */
#    define COMMENT_END    NOT_A_CHARACTER     /* this will NEVER be found! */
#  else
#    define COMMENT_START   '/'
#    define COMMENT_END     '/'
#  endif
#endif

/*
 * Note that there is much nastiness (probably necessary in C) in the odd
 * interface of the pre-processor having a back-route into the parser
 * and HENCE back into itself!!!
 */

#define PP_HASHSIZE  259L
#define PP_DIRLEN     16L
/*
 * PP_DEFLEN is max number of significant chars in an identifier.
 * It must be LESS than min(PP_ABUFINITSIZ,PP_DBUFSIZ)-UNRDCHMAX.
 */
#define PP_DEFLEN    250L
#define PP_UNRDCHMAX  16L  /* see pp_checkid and #pragma force_top_level */
/*
 * PP_NOEXPAND is a pseudo character to inhibit re-expansion of the
 * following name (may change to preceding soon).  It conspires with
 * the list pp_noexpand to inhibit recursive expansion.
 * Two mechanisms are needed in so that in #define i(x) x; #define f +f
 * the call i(f) causes arg f to re-write to +f(NOEXPAND) and the
 * re-scan of +f does not cause re-expansion to ++f.
 * Similarly, PP_TOKSEP is used to represent invisible space in
 *  "M-- where #define M -" which need to be spaces to lex but vanish
 * in stringify.  It also represents elided comments in pcc mode.
 * AM regards it as odd that the draft (dec 88) seems to say that
 * stringification (#s) of the above should yield "---" not "- --".
 * Changing PP_TOKSEP to ' ' in ANSI mode restores the more logical code.
 * Note the use of -ve values: conversion to both signed and unsigned
 * chars is defined and yields bit patterns corresponding to ISO8859
 * top-bit-set-control characters (effectively not in the source char set).
 * Sadly, the conversion of 129 and 130 to signed char is implementation
 * defined and, consequently a portability hazard.
 */
#ifdef CORRECT_IS_BETTER_THAN_FAST /* yuk --
                                    beware INMOS don't a get subtle bug */
/* In ANSI mode all we do not need to steal chars for PP_NOEXPAND.      */
#  define PP_NOEXPAND  ((feature & FEATURE_PCC) ? RARE_char1 : '\f')
#  define PP_TOKSEP    ((feature & FEATURE_PCC) ? RARE_char2 : '\v')
#else
/* The following setting means that RARE_char1/_char2 probably cause    */
/* the pp to fail in ANSI and PCC modes.  @@@ beware: AM has seen crash */
#  define PP_NOEXPAND  RARE_char1
#  define PP_TOKSEP    RARE_char2
#endif
#define PP_EOM       0          /* value 0 exploited by strcpy/'%s'.    */

#define PP_MACSTART  1
#define PP_CIDCHAR   2
#define PP_WHITE     4
static unsigned char pp_ctype[UCHAR_MAX+1];
#define pp_macstart(ch) (pp_ctype[(unsigned char)ch] & PP_MACSTART)
#define pp_cidchar(ch)  (pp_ctype[(unsigned char)ch] & PP_CIDCHAR)
#define pp_white(ch)    (pp_ctype[(unsigned char)ch] & PP_WHITE)

#define PP_EOLP(ch) ((ch) == '\n')

#define HASH(hash, ch) (((hash * 39) + ch) & 0xffffff)

#define PP_ARGLINES_WARN_VAL  10        /* warn after 10 lines of arguments */

/*
 * structures - local to this file ...
 */

typedef struct arglist {
  char *argname;
  struct arglist *argchain;
  int32 argactual;   /* now offset into pp_abufbase */
#define DUFF_OFFSET 0x01010101
/* beware: it is only our expansion strategy that allows us to
   use shallow binding of actual/formal correspondence.
   See later changes in pp_arg_link.
*/
} PP_ARGENTRY;

#define pp_argname_(p)    ((p)->argname)
#define pp_argchain_(p)   ((p)->argchain)
#define pp_argactual_(p)  ((p)->argactual)

typedef struct hashentry {
  char *name;
  union { char *s; int32 i; } body;  /* #define text or magic (e.g PP__LINE) */
  struct hashentry *chain;
/* I limit the reference count to 16 bits so that there can not be any     */
/* trouble on machines with sizeof(int)==2                                 */
  unsigned int uses:16,   /* incremented on reference */
/* ismagic is 14 bits so that this whole field is 32 bits wide */
               ismagic:13,/* things like __TIME__     */
               noifdef:1, /* 1 if not allowed to #ifdef xxx */
               noargs:1,  /* 1 if noargs              */
               alive:1;   /* 0 => undef'd, 1 => def'd */
  struct hashentry *defchain;  /* chain in definition order */
/* AM: the next two fields solely cope with ANSI inhibition of macro    */
/* definitions during rescanning.  It would be more space efficient     */
/* to use a separate list (next/name/len).  Moreover, this current      */
/* structure requires a definition to be suppressed only once at any    */
/* one time to work.  This would seem to be the case, but consider      */
/* #define f g(f); #define g(x) x; (@@@ seemingly ANSI ambiguity).      */
/* We expand 'f' above to 'f', but there is a good case for the 'f(g)'  */
/* interpretation too.                                                  */
  struct hashentry *unchain;   /* used to inhibit recursive expansion */
  int32 sleepleft;             /* ditto -- chars to read in ebuf.     */
#define PP_NOARGHASHENTRY  offsetof(PP_HASHENTRY,arglist)
  struct arglist *arglist;     /* only if noargs==0   */
} PP_HASHENTRY;

#define pp_hashname_(p) ((p)->name)
#define pp_hasharglist_(p) ((p)->arglist)
#define pp_hashbody_(p) ((p)->body.s)
#define pp_hashmagic_(p) ((p)->body.i)
#define pp_hashchain_(p) ((p)->chain)
#define pp_hashnoargs_(p) ((p)->noargs)
#define pp_hashalive_(p) ((p)->alive)
#define pp_hashismagic_(p) ((p)->ismagic)
#define pp_noifdef_(p) ((p)->noifdef)
#define pp_hashuses_(p)  ((p)->uses)
#define pp_unchain_(p)  ((p)->unchain)
#define pp_sleepleft_(p)  ((p)->sleepleft)
#define pp_hashdefchain_(p) ((p)->defchain)

/* The following constants really are ints, or maybe enums */
#define PP__LINE (-1)
#define PP__FILE (-2)
#define PP__DATE (-3)
#define PP__TIME (-4)
#define PP__STDC (-5)
#define PP__ZERO (-6)
#define PP__ONE  (-7)

typedef struct ifstack {
  struct ifstack *chain;
  bool seenelse;
  bool oldskip;
  bool skipelse;
} PP_IFSTACK;

#define pp_ifchain_(p)    ((p)->chain)
#define pp_ifseenelse_(p) ((p)->seenelse)
#define pp_ifoldskip_(p)  ((p)->oldskip)
#define pp_ifskipelse_(p) ((p)->skipelse)

typedef struct filestack {
  struct filestack *chain;
  FILE *stream;
  char *name;
  int32 line;
#ifndef NO_LISTING_OUTPUT
  int32 propoint;
  int32 filenumber;
#endif
} PP_FILESTACK;

#define pp_filchain_(p)   ((p)->chain)
#define pp_filstream_(p)  ((p)->stream)
#define pp_filename_(p)   ((p)->name)
#define pp_fileline_(p)   ((p)->line)
#define pp_propoint_(p)   ((p)->propoint)
#define pp_filenumber_(p) ((p)->filenumber)

int pp_inhashif;                /* 3-valued: see cfe/pp.h               */

static PP_HASHENTRY *((*pp_hashtable)[PP_HASHSIZE]), *pp_noexpand,
             *pp_hashfirst, *pp_hashlast, *pp_hashone, *pp_hashzero;
static PP_IFSTACK *pp_ifstack, *pp_freeifstack;
static PP_FILESTACK *pp_filestack, *pp_freefilestack;

static int32 pp_nsubsts;
static bool pp_skipping, pp_instring;

FILE *pp_cis;                   /* Exported for performget() in BCPL_syn */
static char pp_datetime[26];

/*
 * Diagnostics and free store package...
 */

static VoidStar pp_alloc(int32 n)
{ return GlobAlloc(SU_PP, n);
}

/*
 * ACN's listing option code...
 */
#ifndef NO_LISTING_OUTPUT
static FILE *pp_listingstream_slave;    /* slave copy for minor speedup    */
bool list_this_file;           /* exported copy of MSB of pp_filenumber    */
/*
 * @@@ This depends on the ARM-dependent output of _write_profile
 * and is read directly in binary. (Hmm).
 */
typedef struct XCount
{ unsigned32 count;
  unsigned line:16,
           filename:16;
} XCount;
static XCount *profile_data = NULL;    /* profile data table         */
static int32 profile_count = 0;        /* size thereof (0 => no map) */
static char **profile_files = NULL;    /* file name table            */
static int32 profile_nfiles = 0;       /* size thereof               */
static int32 profile_ptr = 0;  /* profile_data index ( < profile_count)    */
static int32 pp_filenumber;    /* profile_files index ( < profile_nfiles)  */
                               /* also gets 0x80000000 bit set (>= 0 test) */
                               /* ... only used for listing on/off.        */

static int Exec_Rec_Compare(ConstVoidStar a, ConstVoidStar b)
{
    XCount *aa = (XCount *)(int)a, *bb = (XCount *)(int)b;
    int32 k = (int32)aa->filename - (int32)bb->filename;
    if (k == 0) k = (int32)aa->line - (int32)bb->line;
/* The following line ensures that I return an int value that makes sense  */
/* even if plain int is 16 bits. This is needed if I am to use the built-  */
/* in qsort sort procedure.                                                */
    return k == 0 ? 0 : k < 0 ? -1 : 1;
}

bool map_init(FILE *mapstream)
{
/* Data for annotation source listings to indicate how many times various  */
/* lines of code have been executed are allocated directly using malloc()  */
/* since the information is global to the compilation and parts of it need */
/* to be kept in a contiguous vector.                                      */
    int32 w;
    XCount *data;
    char *namebodies;
    union map_filetable { int32 offset; char *addr; } *names;
    struct map_header { char magic[12];
                        int32 namebytes, nfiles, ncounts; } h;
    profile_count = 0;  /* To disable the option */
    if (mapstream == NULL) return 1;
    if (fread(&h, sizeof(h), 1, mapstream) != 1 ||
        memcmp("\xff*COUNTFILE*", h.magic, 12) != 0) return 0;
    w = h.namebytes + 4*h.nfiles + 8*h.ncounts;
    namebodies = (char *)malloc((size_t)w);
    if (namebodies == NULL) return 0;
    names = (union map_filetable *)(int)(namebodies + h.namebytes);
    data = (XCount *)(names + h.nfiles);
    if (fread(namebodies, 1, (size_t)h.namebytes, mapstream) != h.namebytes ||
        fread(names, 4, (size_t)h.nfiles, mapstream) != h.nfiles ||
        fread(data, 8, (size_t)h.ncounts, mapstream) != h.ncounts ||
        fread(h.magic, 1, 12, mapstream) != 12 ||
        memcmp("\xff*ENDCOUNT*\n", h.magic, 12) != 0)
    {   free(namebodies);
        return 0;
    }
    for (w = 0; w < h.nfiles; w++)
        names[w].addr = namebodies + names[w].offset;
/* Now the data is read in - sort it by file name and line number so it    */
/* will be easier to access later.                                         */
    qsort((VoidStar)data, (size_t)h.ncounts, sizeof(XCount), Exec_Rec_Compare);
    profile_data = data;
    profile_count = h.ncounts;
    profile_files = (char **)names;
    profile_nfiles = h.nfiles;
    return 1;
}

#define CHARS_FOR_COUNTS 16

static void listing_nextline(int32 ll)
{   int32 line = ll + 1;
    int pos = 0;
    while ((int32)profile_data[profile_ptr].line < line &&
           profile_data[profile_ptr].filename == pp_filenumber &&
           profile_ptr < profile_count)
        profile_ptr++;
    while (profile_data[profile_ptr].line == line &&
           profile_data[profile_ptr].filename == pp_filenumber &&
           profile_ptr < profile_count)
        (pos += fprintf(listingstream, "%lu ",
                        profile_data[profile_ptr].count)),
        profile_ptr++;
    if (pos >= CHARS_FOR_COUNTS) fprintf(listingstream, "\n"), pos = 0;
    fprintf(listingstream, "%*s| ", (int)(CHARS_FOR_COUNTS-pos), "");
}

static void profile_find(char *fname)
{   int i = 0;
    int32 p = 0;
    if (profile_count == 0) return;
    while (strcmp(fname, profile_files[i]) != 0 &&
        i < profile_nfiles) i++;            /* syserr(dropping off the end) */
    while (profile_data[p].filename != i &&
           p < profile_count) p++;
    pp_filenumber = i;
    profile_ptr = p;
}

#endif /* NO_LISTING_OUTPUT */


/*
 * buffers:  dbuf (defns) ebuf (expansions) abuf (args)
 * Maybe optimise later.
 */
#define PP_DBUFSIZ         1024L    /* default chunk for definitions */
#define PP_ABUFINITSIZ      512L
#define PP_EBUFINITSIZ      256L

#define pp_new_(type)      (pp_alloc(type))

static char *pp_dbufend, *pp_dbufseg, *pp_dbufptr;
#define pp_stuffid_(ch) \
    (pp_dbufptr==pp_dbufend ? pp_newdbuf(ch) : (*pp_dbufptr++ = (ch)))

static int pp_newdbuf(int x)
{ char *dbufbase;
  unsigned32 size = pp_dbufptr-pp_dbufseg;   /* size used in current dbuf */
  unsigned32 allocsize = PP_DBUFSIZ;         /* default size for new dbuf */
  /* allocate 1024 for small requests, but 2048 for 512-1023, etc. */
  while (size >= allocsize/2) allocsize *= 2;
  if (size > allocsize)
     syserr(syserr_newdbuf, (long)size, (long)allocsize);
  dbufbase = pp_alloc(allocsize);
#ifdef ENABLE_PP                /* spurious: deadcode elimination fixes */
  if (debugging(DEBUG_PP))
      cc_msg("new pp_dbuf(%ld)\n", (long)allocsize);
#endif
  memcpy(dbufbase, pp_dbufseg, (size_t)size);
  pp_dbufseg = dbufbase;
  pp_dbufptr = dbufbase+size;
  pp_dbufend = dbufbase+allocsize;
  return (*pp_dbufptr++ = x);
}

static char *pp_closeid(void)
{ char *result;
  pp_stuffid_(0);
  result = pp_dbufseg;
  pp_dbufseg = pp_dbufptr;
  return result;
}

static char *pp_ebufbase, *pp_ebufptr, *pp_ebuftop, *pp_ebufend;
static char *pp_abufbase, *pp_abufoptr, *pp_abufptr, *pp_abufend;
static int32 pp_scanidx, pp_expand_level;

static void pp_abuf_ensure(int32 n)
{   while (pp_abufptr + n >= pp_abufend)
    {   int32 k = pp_abufend - pp_abufbase;
        char *d = pp_alloc(2*k);
#ifdef ENABLE_PP                /* spurious: deadcode elimination fixes */
        if (debugging(DEBUG_PP))
            cc_msg("up pp_abuf to %ld\n", (long)(2*k));
#endif
        memcpy(d, pp_abufbase, (size_t)k);
        pp_abufend = d + 2*k;
        pp_abufptr = d + (pp_abufptr - pp_abufbase);
        pp_abufoptr = d + (pp_abufoptr - pp_abufbase);
        pp_abufbase = d;
    }
}

static void pp_wrch(int ch)
{   if (pp_abufptr >= pp_abufend) pp_abuf_ensure(1);
    *pp_abufptr++ = ch;
}

static void pp_wrbuf(void *buf, int32 len)
{   pp_abuf_ensure(len);
    pp_abufptr = (char *) memcpy(pp_abufptr, buf, (size_t)len) + len;
}

static void pp_ebuf_ensure(int32 n)
{   while (pp_ebuftop + n >= pp_ebufend)
    {   int32 k = pp_ebufend - pp_ebufbase;
        char *d = pp_alloc(2*k);
#ifdef ENABLE_PP                /* spurious: deadcode elimination fixes */
        if (debugging(DEBUG_PP))
            cc_msg("up pp_ebuf to %ld\n", (long)(2*k));
#endif
        memcpy(d, pp_ebufbase, (size_t)k);
        pp_ebufend = d + 2*k;
        pp_ebuftop = d + (pp_ebuftop - pp_ebufbase);
        pp_ebufptr = d + (pp_ebufptr - pp_ebufbase);
        pp_ebufbase = d;
    }
}

static void pp_savch(int ch)
{   if (pp_ebuftop >= pp_ebufend) pp_ebuf_ensure(1);
    *pp_ebuftop++ = ch;
}

static void pp_savbuf(void *buf, int32 len)
{
  pp_ebuf_ensure(len);
    
  pp_ebuftop = (char *) memcpy(pp_ebuftop, buf, (size_t)len) + len;

}

#ifdef ENABLE_PP
static void pp_showsleep()
{   PP_HASHENTRY *p;
    for (p = pp_noexpand; p != 0; p = pp_unchain_(p))
        fprintf(stderr, "<sleeping %s %ld>", pp_hashname_(p), pp_sleepleft_(p));
}
#else
#define pp_showsleep() 0
#endif

static void pp_addsleep(int32 n)
{   PP_HASHENTRY *p;
    for (p = pp_noexpand; p != 0; p = pp_unchain_(p))
        pp_sleepleft_(p) += n;
}

static void pp_subsleep(int32 n)
{   PP_HASHENTRY *p;
    for (p = pp_noexpand; p != 0; p = pp_unchain_(p))
    {   pp_sleepleft_(p) -= n;
        if (debugging(DEBUG_PP) && pp_sleepleft_(p) < 0)
            cc_msg("<overslept %s %ld>\n", pp_hashname_(p), pp_sleepleft_(p));
    }
}

static void pp_awaken_all()
{   /* Maybe this should be a syserr() one day if pp_noexpand != 0.     */
    while (pp_noexpand)
    {   PP_HASHENTRY *p = pp_noexpand;
        pp_noexpand = pp_unchain_(p), pp_unchain_(p) = 0;
        pp_hashalive_(p) = 1;
    }
}

static void pp_sleep_name(PP_HASHENTRY *p, int32 n)
    {   /* suppress recursive invocations in ANSI mode -- PCC loops!    */
        if (pp_unchain_(p)) syserr(syserr_pp_recursion, pp_hashname_(p));
        pp_unchain_(p) = pp_noexpand;
        pp_sleepleft_(p) = 0;
        pp_hashalive_(p) = 0;
        pp_noexpand = p;
        pp_addsleep(n);
        if (debugging(DEBUG_PP))
            cc_msg("<sleep name %s, result size %ld>\n", pp_hashname_(p),
                   (long)n);
    }

/*
 * Support for built-in header files
 */

static bool string_file_active = NO;

#ifndef NO_INSTORE_FILES

static struct pp_uncompression_record
{
    char *pointer;              /* some of these should be unsigned */
    char compressed;
    unsigned char height;
    char stack[MAXSTACKDEPTH];
} hdrfile;

static int pp_fetch_string_char()
{
    int c, k;
    if (hdrfile.height == 0) c = *hdrfile.pointer++;
    else c = hdrfile.stack[--hdrfile.height];
    for (;;)
    {   c &= 0xff;
        k = compression_info[c];
        if (k == c || hdrfile.compressed == 0) return c;
/*
 * When genhdrs is establishes the greatest possible depth needed in
 * this stack and arranges to define MAXSTACKDEPTH suitably - thus no
 * run-time check for stack overflow is needed.
 */
        hdrfile.stack[hdrfile.height++] = k;
        c = k >> 8;
    }
}

FILE *open_builtin_header(char *name, bool *sf)
{
    int count;
    if (strcmp(name, "strings.h") == 0) name = "string.h";
    for (count=0; builtin_headers[count].name != 0; count++) {
        if (strcmp(name, builtin_headers[count].name) == 0)
        {   *sf = YES;
            hdrfile.height = 0;
            hdrfile.compressed = 1;
            hdrfile.pointer = &string_data[builtin_headers[count].content];
/*
 * The result (which is of type FILE *) must be handed back as a non-zero
 * value since it is compared against zero to check for success here.  But
 * when a string file has been opened the regular file pointer will never be
 * used, so the (non-zero) value returned is not very important.
 * The arbitrary use of 'stdin' is thus OK since ANSI say 'stdin' != NULL.
 */
            return stdin;
        }
    }
    return NULL;
}

#endif

/*
 * I/O routines, including ANSI trigraph routine. Built for speed.
 *
 * The idea we exploit here is that (i) only whole lines are interleaved by
 * #include and (ii) after a #included file has been processed, we continue
 * reading the NEXT line of the including file. By buffering whole lines,
 * we can reduce most overhead to once per line, rather than one per char.
 * As lines average 30-40 chars, this gives a major reduction in per-char
 * overhead. We also take the opportunity to unify in-store files with
 * regular files at this point. The only difference is that we don't copy
 * in-store files to the (partial) line buffer. The critical routines
 * are pp_fillbuf, pp_rdch1 and pp_rdch. pp_translate_1, pp_comment
 * and pp_comment_nl play supporting roles. The best order to read for
 * understanding is pp_rdch, pp_rdch1, pp_fillbuf, others.
 */

static int32 pp_rdcnt;                          /* the input buffer count */
static unsigned char  *pp_rdptr;                       /* the input buffer pointer */
static unsigned char  pp_linebuf[64];                     /* the input line buffer */
static unsigned char  pp_translate[256];
static int   pp_rdch1nls,                  /* count of \<NL>s outstanding */
             pp_rdch3nls;            /* count of <NL>s in current comment */

/* After a call to pp_fillbuf(), pp_rdptr points to the tail of a line or */
/* part thereof, pp_rdcnt is the number of chars left in the buffer and   */
/* the first character of the buffer (or EOF) is returned (we do this     */
/* because EOF cannot be stored as a character).                          */

static int pp_fillbuf(void)
{   int32 n;
    int lastch;
    unsigned char *s;
/*
 * We can only count the \n when we read the NEXT character if we're to
 * have any hope of correct line numbers of (e.g.) identifiers at EOL.
 * (at least this overhead is now only once per line). If pp_rdptr is NULL
 * then the previous read yielded EOF (or there was no previous read).
 */
    if (pp_rdptr != NULL && (lastch = pp_rdptr[-1]) == '\n')
        ++pp_linect;
    else
        lastch = 0;

    s = pp_linebuf;

#ifndef NO_INSTORE_FILES
    if (string_file_active)
/* If reading from a compressed, in-memory, string file then get the next */
/* line. Note that we know that lines are terminated by \n, that a \n     */
/* precedes EOF, and that there are NO imbedded #includes.                */
    {   int ch;
        for (n = 0;  n < sizeof(pp_linebuf);)
        {   ch = pp_fetch_string_char();
            if (ch == 0) break;
            s[n++] = ch;
            if (ch == '\n') break;
        }
    }
    else
#endif
/* Reading from a real file. Here, pp_rdcnt == 0 OR pp_rdcnt == 1 and   */
/* the last character in the buffer should be saved before reading more */
/* (it's either '\' or '?'). Any other value of pp_rdcnt is ignored.    */
    {   FILE *cis = pp_cis;
        int ch;
        if ((n = pp_rdcnt) == 1)
            s[0] = pp_rdptr[-1];
        else n = 0;
        /* Read until EOF OR buffer full OR end of line character found */
        for (;;)
        {   ch = getc(cis);

            if (ch == EOF) break;

            s[n++] = ch;
            if (ch == '\n' ||
#ifdef HOST_USES_CRLF
                ch == '\r' ||
#endif
                n >= sizeof(pp_linebuf)) break;
        }
        if (n == 0)                                      /* end of file */
        {   if (pp_rdptr != NULL && lastch != '\n')
            {   cc_pccwarn(pp_rerr_newline_eof);
                s[n++] = '\n';                    /* fake nl before EOF */
            }
        }
#ifdef HOST_USES_CRLF
        else if ((ch == '\n' || ch == '\r') && !pp_instring)
        {   int nextch = getc(cis);               /* This read-ahead is */
            if ((ch + nextch) == ('\r' + '\n'))   /* tough on cc -S -   */
                s[n-1] = '\n';                    /* if the following   */
            else ungetc(nextch, cis);             /* ungetc() happens.. */
        }
#endif
    }

#ifndef NO_LISTING_OUTPUT
    /* The following predicate is now once/line, rather than once/char. */
    if (pp_listingstream_slave &&
        (n > 0) &&                                        /* => NOT EOF */
        (pp_filenumber >= 0) &&
        (feature & FEATURE_UNEXPANDED_LISTING))
    {   fwrite(s, 1, (size_t)n, listingstream);
        /* we may be listing part of a line, so care with \n condition. */
        if (s[n-1] == '\n')
        {   listing_diagnostics();
            if (profile_count) listing_nextline(pp_linect);
            fprintf(listingstream, "%6lu  ", pp_linect+1);
        }
    }
#endif /* NO_LISTING_OUTPUT */

    if (n == 0)
    {   pp_rdcnt = 0;
        pp_rdptr = NULL;
        return EOF;
    }
    else
    {   pp_rdcnt = n-1;
        pp_rdptr = s+1;

        return *s;
    }
}

static int pp_translate_1(unsigned int ch)
{
/* Translate COMMENT_START, '\n', ANSI tri-glyphs and \<nl>. Note that */
/* \<nl> can't occur in a tri-glyph, but ?? / can occur before \n...   */
#ifdef PASCAL
    if (ch == '{')
        return ch;
#endif
    if (ch == COMMENT_START)
        return ch;
    if (ch == '\n')
    {   if (pp_rdch1nls > 0)        /* return a \n saved up from \<nl> */
        {   --pp_rdch1nls;          /* one less to return next time... */
            ++pp_rdcnt;             /* and unget the pending '\n'...   */
            --pp_rdptr;
        }
        return ch;
    }
#ifndef PASCAL
    else if (ch == '?')                     /* possible triglyph alert */
    {   int32 n = pp_rdcnt;                 /* NB - only in ANSI mode. */
        unsigned char *s = pp_rdptr;
        if (n == 0 || n == 1 && s[0] == '?')
        {   if (n == 0) ++pp_rdcnt;         /* Seen '?' - copy to buffer */
            pp_fillbuf();                   /* skip 1st char of buffer   */
            if (n == 1)                     /* Seen ?? - point to second */
            {   ++pp_rdcnt;  --pp_rdptr;    /* '?'... either way, end up */
            }                               /* pointing at 'x' of ?x...  */
            n = pp_rdcnt;
            if (n < 2) return '?';
            s = pp_rdptr;
        }
        if (s[0] != '?') return '?';
/* If we really wanted to compile this code in an environment where    */
/* 7 bit ascii chars were not available, then we have have to change   */
/* the following character constants to their trigraph form too.       */
        switch(s[1])
        {
default:    return '?';
case '=':   ch = '#';   break;
case '(':   ch = '[';   break;
case '/':   ch = '\\';  break;
case ')':   ch = ']';   break;
case '\'':  ch = '^';   break;
case '<':   ch = '{';   break;
case '!':   ch = '|';   break;
case '>':   ch = '}';   break;
case '-':   ch = '~';   break;
        }
        pp_rdcnt = n - 2;
        pp_rdptr = s + 2;
        cc_warn(pp_warn_triglyph,(int)'?', (int)'?', (int)s[1], (int)ch);
        if (ch != '\\') return ch;
        /* else ch == '\\' so drop through in case next char is '\n' */
    }
    if (ch == '\\')
    {   int32 n = pp_rdcnt;
        if (n == 0)
        {   ++pp_rdcnt;
            pp_fillbuf();
            n = pp_rdcnt;
            if (n == 0) return '\\';
        }
        if (pp_rdptr[0] != '\n') return '\\';
        ++pp_rdptr;
        pp_rdcnt = n - 1;
        ++pp_rdch1nls;
        return PP_TOKSEP;
    }
#endif
    cc_rerr(pp_rerr_nonprint_char, ch);
    return PP_TOKSEP;
}

static int pp_rdch1(void)
{
  unsigned int ch, trch;
  
/* This function embodies much critical per-character overhead so we work    */
/* hard to make it fast in the common cases, even at the cost of making      */
/* uncommon cases (such as '\n', '?', '\\' and EOF) slower. The idea is that */
/* exception (PP_TOKSEP) entries in a translate table flag complex cases.    */

  for (;;)
    {
      if (--pp_rdcnt >= 0)
	{
	  ch = *pp_rdptr++;
	}
      else
	{
	  ch = pp_fillbuf();
	  
	  if (ch == EOF)
	    {
	      return ch;
	    }	  
	}

      if ((trch = pp_translate[(unsigned char)ch]) != PP_TOKSEP)
	{
	  return trch;
	}
	
      trch = pp_translate_1(ch);
	
      if (trch != PP_TOKSEP)
	{
	  return trch;
	}
    }
}

static int pp_comment(int ch)
/* After recognising a comment start, this function is called to read the   */
/* remainder of it. We do this to save per-character overhead in pp_rdch(), */
/* which is, otherwise, burdened with this monster's procedure prologue...  */
{   int comment_nest = 1;
    if (feature & FEATURE_PPCOMMENT)
    {   /* Assert: ch == '{' || ch == '*' */
        /* N.B. output to stdout BY DEFINITION of cc -C. Similarly below */
        if (ch != '{') putc(COMMENT_START, stdout);
        putc(ch, stdout);
    }
    ch = pp_rdch1();
    for (;;)
    {   if (ch != EOF && feature & FEATURE_PPCOMMENT) putc(ch, stdout);
        switch (ch)
        {   case EOF: cc_err(pp_err_eof_comment); return EOF;
            case '\n':
                if (!(feature & FEATURE_PPCOMMENT)) ++pp_rdch3nls;
                break;
            default:
                break;
#ifdef PASCAL
            case '}':
            case '{':
                if (ch == '{')
                {
                    cc_warn(pp_warn_nested_comment);
                } else {
                    --comment_nest;
                    if (comment_nest == 0) {
                        if (feature & FEATURE_PPCOMMENT) putc(ch, stdout);
                        return ' ';
                    }
                }
                break;
#endif
            case COMMENT_START:
                ch = pp_rdch1();
                if (ch == '*')
                {
                    if (!(feature & FEATURE_PCC))
                        cc_warn(pp_warn_nested_comment, "/*");
                }
                continue;
            case '*':
                ch = pp_rdch1();
                if (ch == COMMENT_END) --comment_nest;
                if (comment_nest == 0)
                {
                    if (feature & FEATURE_PPCOMMENT) putc(ch, stdout);
                    if (feature & (FEATURE_PCC | FEATURE_PPCOMMENT))
                        return PP_TOKSEP;      /* comments vanish but have */
                    else                       /* separating value...      */
                        return ' ';            /* comments -> single space */
                }
                continue;
        }
        ch = pp_rdch1();
    }
}

/*
 * pp_rdch() selects the next character from one of two buffers (during
 * macro expansion) or reads the next character from the input stream
 * (notionally pp_rdch1()). If it takes a character from the input stream,
 * its job is also to reduce comments to single spaces (ANSI) or PP_TOKSEP
 * (pcc mode). This is the most critical function called once per character
 * and its implementation is slightly twisted to save cycles.
 */

static int pp_rdch(void)
{
  int ch, trch;


  /* AM: perform special case code if we are rescanning:                  */

  if (!(pp_scanidx < 0 && pp_ebufptr == pp_ebuftop))
    {
      PP_HASHENTRY *p;
      
/* AM: note that we rely on PP_TOKSEP at end of expansions to stop      */
/* read-ahead for more isalnum's reading via pp_rdch() and so           */
/* defeating recursion trapping in #define f f.                         */
/* Note pp_unchain/pp_sleepleft comment re stack nature of sleep.       */
      
      if (pp_noexpand)
        {
	  /* move following code to pp_subsleep? Beware pp_argexpand().  */

	  while (pp_noexpand && pp_sleepleft_(pp_noexpand) <= 0)
            {
	      p = pp_noexpand;
	      pp_noexpand = pp_unchain_(p), pp_unchain_(p) = 0;
	      pp_hashalive_(p) = 1;
            }
	  pp_subsleep(1);
        }
      
      ch = (pp_scanidx >= 0) ? pp_abufbase[pp_scanidx++] : *pp_ebufptr++;
      
      return ch;      
    }
  
  /* pp_noexpand is notionally 0 here -- insert syserr() soon?        */
  /* For the time being leave the nice safe code:                     */

  if (pp_noexpand)
    pp_awaken_all();

  /* And now we drop through into the remnants of pp_rdch3()...            */

  if (pp_rdch3nls > 0)               /* saved nls from within comments */
    {
      --pp_rdch3nls;

      return '\n';
    }

  if (pp_rdcnt > 0)
    /* The usual case - the line buffer is non-empty... */
    {
      ch = *pp_rdptr;

/* The usual case of a non-exceptional character, so take the short cut. */
/* (i.e. not '\n', '\\', COMMENT_START, EOF, or, in ANSI mode, '?').     */
      
      if ((trch = pp_translate[(unsigned char)ch]) != PP_TOKSEP)
        {
	  ++pp_rdptr;
	  --pp_rdcnt;

	  return trch;
        }
    }

  ch = pp_rdch1();

  if ((ch != COMMENT_START
#ifdef PASCAL
       && ch != '{'
#endif
       ) || pp_instring)
    {
      return ch;
    }
    
#ifdef PASCAL
  if (ch != '{')
#endif
    {
      ch = pp_rdch1();

      if (ch != '*')             /* unget ch... */
        {
	  ++pp_rdcnt;            /* which is    */
	  --pp_rdptr;            /* always safe */
      
	  return COMMENT_START;
        }
    }

  ch = pp_comment(ch);

  return ch;  
}

/* Do not call pp_unrdch() more than UNRDCHMAX times.                   */
/* Jun 90: pp_unrdch probably shouldn't write to pp_abufbase[]          */
/* following fix to "fnlikemacro <non lparen>", but leave for now.      */
static void pp_unrdch(int ch)
{   if (ch == EOF);      /* safety when input from file                 */
    else if (pp_scanidx > 0)
    {   pp_addsleep(1);  pp_abufbase[--pp_scanidx] = ch;
    }
    else
    {   pp_addsleep(1);  *--pp_ebufptr = ch;
    }
}

static int pp_skipb0(void)
{   int ch;
    do {ch = pp_rdch();} while (ch != EOF && pp_white(ch));
    return ch;
}

static int pp_skipb1(int ch)
{   while (ch != EOF && pp_white(ch)) ch = pp_rdch();
    return ch;
}

/* Note the following (required by ANSI draft of Dec 88) causes the     */
/* text "0xee+getchar()" NOT to expand getchar.                         */
/* The routine pp_number corresponds to ANSI pp-number save that        */
/* the caller has already passed on any leading '.'.  The pp_ch arg     */
/* is presumed to satisfy 'isdigit'.                                    */
/* In PCC mode we allow characters after E+/e- to be expanded (note     */
/*             this allows X to be expanded both in 0xee+X and 3.4e+X). */
static void pp_number(int pp_ch)
{   while (isdigit(pp_ch) || pp_cidchar(pp_ch) || pp_ch == '.')
    {   int c = pp_ch;
        if (!pp_skipping) pp_wrch(pp_ch);
        pp_ch = pp_rdch();
        if ((c == 'e' || c == 'E') && (pp_ch == '+' || pp_ch == '-') &&
                                      !(feature & FEATURE_PCC))
        {   if (!pp_skipping) pp_wrch(pp_ch);
            pp_ch = pp_rdch();
        }
    }
    pp_unrdch(pp_ch);
}

static int32 pp_savnumber(char *p)
/* p is a buffer holding a number in text form.  Return offset beyond it */
/* and copy into expansion buffer.                                       */
/* Only used from pp_argexpand (and hence in ANSI mode).                 */
{   int32 i = 0;
    int ch = p[i++];
    while (isdigit(ch) || pp_cidchar(ch) || ch == '.')
    {   int c = ch;
        ch = p[i++];
        if ((c == 'e' || c == 'E') && (ch == '+' || ch == '-'))
            ch = p[i++];
    }
    i--;        /* because 'i' is now one beyond first non-digit.       */
    pp_savbuf(p, i);
    return i;
}

#define pp_widestrbeg(a,b) ((a) == 'L' && ((b) == '"' || (b) == '\''))

/* pp_copystring notionally returns type void, but so that things like  */
/*  ... "nonterminated string <nl>#define ..." can be recognised it     */
/* returns a suitable char for pp_process to use as pp_lastch.          */
static int pp_copystring(int quote)
{ int pp_ch;
  pp_instring = 1;
  if (!pp_skipping) pp_wrch(quote);
  while ((pp_ch = pp_rdch()) != quote)
  { switch (pp_ch)
    { case EOF: cc_err(pp_err_eof_string); goto out;
      case '\n': if (feature & FEATURE_PCC)
                   quote = 0;
                 else
                   cc_err(pp_err_eol_string, (int)quote);
                 goto out;
      default: if (!pp_skipping) pp_wrch(pp_ch); break;
#ifndef PASCAL
      case '\\':
        if (!pp_skipping) pp_wrch('\\');
        switch (pp_ch = pp_rdch())
        { case EOF: cc_err(pp_err_eof_escape); goto out;
          case '\n': pp_wrch(pp_ch); break;
          default: if (!pp_skipping) pp_wrch(pp_ch); break;
        }
#endif
    }
  }
out:
  if (!pp_skipping && (quote != 0)) pp_wrch(quote);
  if (pp_ch == '\n') pp_wrch(pp_ch);
  pp_instring = 0;
  return pp_ch;
}

static void pp_stuffstring(int quote, bool dequote, bool discard)
/* dequote = 0: leave quotes alone; dequote = 1: remove " for #include. */
/* For #include we treat \ escapes specially for msdos file specs.      */
/*   [ANSI says escapes in #include/#line are undefined.]               */
/* We arrange that #include both "a:\\foo\\baz" and "a:\foo\baz" work   */
/* by collapsing \\ to \ but \a to \a if dequote == 1.  (Of course,     */
/* things like ...include "abc\<nl>def"... will be glued earlier.)      */
/* For filenames (dequote true) we also discard all whitespace. This is */
/* to be kind to #include <  stdio  .  h  > in case (for instance) it   */
/* has been created via macro-expansion where stray whitespace may be   */
/* hard to control and understand.                                      */
{ int pp_ch; bool err = 0;
  pp_instring = 1;
  if (!dequote & !discard) pp_stuffid_(quote);
  while ((pp_ch = pp_rdch()) != quote)
  { switch (pp_ch)
    { case EOF:
      case '\n': err = 1; goto out;
      default:   if (!discard && (!dequote || !isspace(pp_ch)))
                    pp_stuffid_(pp_ch);
                 break;
#ifndef PASCAL /*ECN*/
      case '\\':
        if (!discard) pp_stuffid_('\\');
        switch (pp_ch = pp_rdch())
        { case EOF:
          case '\n': err = 1; goto out;
          case '\\': if (dequote) break;
                     /* drop through */
          default:   if (!discard) pp_stuffid_(pp_ch);
                     break;
        }
#endif
    }
  }
out:
  if (err) pp_unrdch(pp_ch), cc_err(pp_err_missing_quote, (int)quote);
  if (!dequote && !discard) pp_stuffid_(quote);
  pp_instring = 0;
}

static int32 pp_savstring(char *p)
/* p is a buffer holding a string in text form.  Copy into expansion    */
/* buffer and return length copied.                                     */
{   int quote = *p;
    int32 i = 0;
    for (;;)
    { int ch = p[++i];
      if (ch == quote) { ++i; break; }
      if (ch == 0) break;                           /* really malformed */
#ifndef PASCAL
      if (ch == '\\') if (p[++i] == 0) break;       /* really malformed */
#endif
    }
    pp_savbuf(p, i);
    return i;
}


static bool pp_eqname(char *s, char *v, int32 n)
                 /* like strcmp but 2nd arg is base/length format */
{ while (n-- > 0) if (*s++ != *v++) return 0;
  if (*s != 0) return 0;
  return 1;
}

static char *pp_special(int32 n, char *s)
{   switch (n)
    {   default:       syserr(syserr_pp_special, (long)n);
                       return "";
        case PP__LINE: sprintf(s, "%ld", (long)pp_linect);
                       return s;
        case PP__FILE: /* double '\' from msdos file names for lex.c:   */
                       /* escape '"' to '\"' to keep sane too.          */
                       /* notionally: sprintf(s, "\"%s\"", pp_cisname); */
            {   char *p = s, *q = pp_cisname; int ch;
                *p++ = '\"';
                while ((ch = *q++) != 0)
                {   if (ch == '\\' || ch == '"') *p++ = '\\';
                    *p++ = ch;
                }
                *p++ = '\"';
                *p++ = 0;
                return s;
            }
/* see the spec of asctime for the following numbers */
        case PP__DATE: sprintf(s, "\"%.7s%.4s\"",
                               pp_datetime+4, pp_datetime+20);
                       return s;
        case PP__TIME: sprintf(s, "\"%.8s\"", pp_datetime+11);
                       return s;
        case PP__ZERO: return "0";
        case PP__ONE:  /* drop through */
        case PP__STDC: return "1";
    }
}

/* Some re-ordering of routines desirable... */
static bool pp_checkid(int ch);
static PP_ARGENTRY *pp_findarg(PP_ARGENTRY *a, char *id, int32 n);

#ifdef ENABLE_PP
static void pp_show_buffers(char *msg)
{
  char *s;
  fprintf(stderr, "%s: %*s(ebuf = '", msg, (int)pp_expand_level*2, "");
  for (s = pp_ebufbase+PP_UNRDCHMAX; s != pp_ebufptr; ++s) putc(*s, stderr);
  putc('!', stderr);
  for (;s != pp_ebuftop;  ++s) putc(*s, stderr);
  fprintf(stderr, "', abuf = '");
  for (s = pp_abufbase;  s != pp_abufptr; ++s) putc(*s, stderr);
  fputs("')\n", stderr);
}
#else
#define pp_show_buffers(x)
#endif

static void pp_argexpand(int32 ap)
{
  /* arg (ap) is an offset into abuf, pp_scanidx>=0 iff rescanning.     */
  /* result is put into ebuf.                                           */
  /* AM: (ANSI ambiguity) it is not clear whether expansion should be   */
  /* depth-first or breadth-first: consider a(b) where a=f b=) f(x)=3.  */
  /* We now do depth-first to match top-level.                          */
  /* N.b. there is scope for re-organising buffer (ap) reuse.           */
  int32 scanidx = pp_scanidx;
  int32 doneseg = 0;
  for (;;)      /* retry until no more macros expand.                   */
  { int32 sav_ebuftop = pp_ebuftop - pp_ebufbase;
    int32 sav_abufptr = pp_abufptr - pp_abufbase;
    int32 apsleepbase = ap + doneseg;
    if (debugging(DEBUG_PP))
       cc_msg("pp_argexpand('%s'+%lu) %lu ", pp_abufbase+ap, doneseg, ap),
       pp_showsleep(),
       cc_msg("\n");
    /* copy previously substituted text in arg: */
    if (doneseg != 0)           /* test technically spurious            */
    {   pp_savbuf(pp_abufbase+ap, doneseg), ap += doneseg;
    }
/* copy tokens literally, but stopping after first substitution:        */
/* @@@ nastiness: the direct use of pp_abufbase[] instead of pp_rdch()  */
/* is an efficiency hack beyond its sense.  It causes the miserable     */
/* pp_subsleep(...apsleepbase...) calls.                                */
    for (;;)
    { int ch = pp_abufbase[ap];
      if (ch == PP_EOM)         /* no expansions this time round.       */
      { pp_subsleep(ap-apsleepbase);    /* OK, but ignored by caller.   */
        pp_abufptr = pp_abufbase + sav_abufptr;
        pp_scanidx = scanidx;
        return;
      }
      else if (pp_macstart(ch) || ch == PP_NOEXPAND)
      { bool changes;
        doneseg = (pp_ebuftop - pp_ebufbase) - sav_ebuftop;
        pp_subsleep(ap+1 - apsleepbase);
        pp_scanidx = ap+1;  changes = pp_checkid(ch);  ap = pp_scanidx;
        apsleepbase = ap;
        /* copy chars after any macro substitution and rescan.          */
        if (changes)
        {   int32 n = strlen(pp_abufbase+ap);
            pp_savbuf(pp_abufbase+ap, n+1);
            break;
        }
      }
      else if (isdigit(ch)) ap += pp_savnumber(pp_abufbase+ap);
      else if (ch == '"' || ch == '\'') ap += pp_savstring(pp_abufbase+ap);
      else pp_savch(ch), ++ap;
    }
    pp_abufptr = pp_abufbase + sav_abufptr;
/* reset 'ap' to result of ebuf expansion and repeat.                   */
    ap = pp_abufptr - pp_abufbase;
    pp_wrbuf(pp_ebufbase + sav_ebuftop,
             pp_ebuftop - (pp_ebufbase + sav_ebuftop));
    pp_ebuftop = pp_ebufbase + sav_ebuftop;
    pp_wrch(PP_EOM);
  }
}

/* pp_expand expands a macro whose args are in abuf into ebuf.          */
static void pp_expand(PP_HASHENTRY *p, int32 nlsinargs)
{ int dch;
  int hashflag = 0;             /* always 0 in PCC mode.                */
  char *dp, specialbuf[256];
  int32 aftercallchars = pp_ebuftop - pp_ebufptr;
/* n.b. at top level (scanidx == -1), aftercall chars are the '+asd'    */
/* caused after expanding f with #define f x+asd; #define x <whatever>. */
/* When in pp_argexpand this can include both pre- and post- call chars */
  int32 sav_abufptr = pp_abufptr - pp_abufbase; /* aftercallchars' home */
  
  pp_hashuses_(p)++;
  
  if (debugging( DEBUG_PP ))
    {
      if (aftercallchars != 0)
	{
	  cc_msg( "aftercallchars(idx %ld) = '%.*s' at %d\n",
		  (long)pp_scanidx, (int)aftercallchars, pp_ebufptr,
		  pp_ebufptr - pp_ebufbase );
	}
    }

  if (pp_scanidx < 0)
    {
      /* first copy stuff in ebuf after the macro call to after last actual */

      pp_wrbuf( pp_ebufptr, aftercallchars );
      
      pp_ebufptr = pp_ebuftop = pp_ebufbase + PP_UNRDCHMAX;
    }
  
  dp = pp_hashismagic_(p) ? pp_special(pp_hashmagic_(p), specialbuf)
                          : pp_hashbody_(p);
  
#ifdef ENABLE_PP                /* spurious: deadcode elimination fixes */
  if (debugging( DEBUG_PP ))
    {
      cc_msg("pp_expand(%s) = '%s'\n", pp_hashname_(p), dp);
      
      pp_show_buffers("e1");
    }
  
  ++pp_expand_level;
#endif
  
  if (!(feature & FEATURE_PCC))
    pp_savch( PP_TOKSEP );    /* no glueing   */
  
  while ((dch = *dp) != 0)
    {      
      switch (dch)
	{
	case '#':
	  if (feature & FEATURE_PCC ||
	      pp_hasharglist_( p ) == 0)
	    {
	      /* no '#' operators in pcc-mode macro-expansion... */
	      pp_savch(*dp++);
	    }
	  else
	    {
	      /* consider cases like f(a,b) == a ## # b.       */
	      
	      ++dp;
	      
	      if (*dp == '#')
		{
		  hashflag |= 2;
		  ++dp;
		}
	      else
		hashflag |= 1;
	      
	      while (*dp == ' ')
		dp++;    /* whitespace ANSI normalised.  */
	    }
	  break;
	  
	case '\'':
	case '"':
	  if (!(feature & FEATURE_PCC))
	    {
	      dp += pp_savstring(dp);
	      hashflag = 0;
	      break;
	    }
	  /* if in pcc mode then fall through to default case */
	  
	default:
	  if (pp_macstart(dch))
	    {
	      int32 i = 0;
	      PP_ARGENTRY *a;
	      
	      do i++, dp++;
	      while (pp_cidchar( *dp ));
	      
	      a = pp_hashnoargs_(p) ? 0 :
		pp_findarg(pp_hasharglist_(p), dp-i, i);
	      
	      if (hashflag == 0 &&  /* not # or ## prefixed, maybe suffixed */
		  !(feature & FEATURE_PCC))
		{
		  char *s = dp;
		  
		  while (*s == ' ')
		    ++s;      /* whitespace ANSI normalised.  */
		  
		  if (s[0] == '#' && s[1] == '#' && pp_hasharglist_(p) != 0) hashflag = 2;
		}
	      
	      if (hashflag & 1)
		pp_savch('"');
	      
	      if (a == NULL)
		{
		  pp_savbuf(dp - i, i);               /* not an arg   */
		}	      
	      else
		{
		  /* an arg 'a' to include/expand */
		  
		  int32 ap = pp_argactual_(a);
		  
		  if (hashflag == 0 && !(feature & FEATURE_PCC))
		    {
		      PP_HASHENTRY *oldsleepers = pp_noexpand;
		      
		      pp_noexpand = 0;
		      pp_savch(PP_TOKSEP);                      /* no glueing   */
		      pp_argexpand(ap);
		      pp_savch(PP_TOKSEP);                      /* no glueing   */
		      pp_awaken_all();
		      pp_noexpand = oldsleepers;
		      
		      if (debugging(DEBUG_PP))
			{
			  cc_msg("pp_argexpanded(%s)\n", pp_abufbase+ap);
			  
			  pp_show_buffers("r1");
			}
		    }
		  else
		    {
		      int ch;
		      
		      while ((ch = pp_abufbase[ap++]) != 0)     /* PP_EOM       */
			{
			  if ((hashflag & 1) && (ch == '"' || ch == '\\'))
			    /* Note hashflag == 0 in PCC mode.                  */
			    /* When stringising escape '"' and '\' so that      */
			    /* stringise('\"') and stringise(\) work.           */
			    pp_savch('\\');
			  
			  /* @@@ bug: next line is wrong removing all PP_NOEXPAND: see file head. */
			  
			  if (!(hashflag & 2 && ch == PP_NOEXPAND ||
				hashflag & 1 && ch == PP_TOKSEP)) pp_savch(ch);
			}
		    }
		}
	      
	      if (hashflag & 1)
		pp_savch( '"' );
	    }
	  else        /* i.e. any char but "'# */
	    {
	      /* note: dch cannot be PP_TOKSEP in ANSI mode.                */
	      
	      if (dch != PP_TOKSEP)
		pp_savch(*dp);
	      
	      ++dp;
	    }
	  
	  /* The next line rests on ANSI whitespace normalisation.        */
	  
	  if (dp[0] == ' ' && dp[1] == '#' && dp[2] == '#' &&
	      !(feature & FEATURE_PCC) && pp_hasharglist_(p) != 0) dp++;
	  
	  hashflag = 0;
	  break;
	}
    }
  
  if (!(feature & FEATURE_PCC))
    pp_savch(PP_TOKSEP);    /* no glueing   */
  
  if (!pp_hashnoargs_( p ))
    {
      PP_ARGENTRY *a;

      for (a = pp_hasharglist_(p); a != 0; a = pp_argchain_(a))
	{
	  int32 i = pp_argactual_(a);

	  pp_argactual_(a) = *((int32 *)(int)(pp_abufbase + i - sizeof (int32)));
	}
    }
  
  if (pp_scanidx < 0)
    {
      while (nlsinargs-- > 0)
	pp_savch('\n');  /* save up the NL's in args */
      
      if (!(feature & FEATURE_PCC))
        /* suppress recursive invocations in ANSI mode -- PCC loops!    */
        pp_sleep_name(p, pp_ebuftop - (pp_ebufbase + PP_UNRDCHMAX));
      
      if (debugging(DEBUG_PP))
        cc_msg("restore aftercall(%.*s)\n",
               (int)aftercallchars, pp_abufbase+sav_abufptr);
      
      pp_savbuf(pp_abufbase + sav_abufptr, aftercallchars);

      pp_abufptr = pp_abufbase;    /* clear for (top level) rescan */
    }
  else
    if (!(feature & FEATURE_PCC))
      /* suppress recursive invocations in ANSI mode -- PCC loops!    */
      pp_sleep_name(p, pp_ebuftop - (pp_ebufbase + PP_UNRDCHMAX)
                                    - aftercallchars);

  pp_nsubsts++;
  
#ifdef ENABLE_PP                /* spurious: deadcode elimination fixes */
  --pp_expand_level;

  if (debugging(DEBUG_PP))
    pp_show_buffers("e2");
#endif
}

static PP_HASHENTRY *pp_lookup(char *name, int32 hash)
{   PP_HASHENTRY *p;
    for (p = (*pp_hashtable)[hash % PP_HASHSIZE]; p != 0; p = pp_hashchain_(p))
        if (pp_hashalive_(p) && strcmp(pp_hashname_(p),name) == 0) break;
    return p;
}

/* The following routines are just used by pp_checkid:                  */
static void pp_arg_align()
{
  while ((int32)pp_abufptr & (sizeof(int32)-1)) pp_wrch(0);
  pp_abufptr += sizeof(int32);
}

static void pp_arg_link(PP_ARGENTRY *a, int32 arg)
{
  *((int32 *)(int)(pp_abufbase + arg - sizeof(int32))) = pp_argactual_(a);
  pp_argactual_(a) = arg;
}

/* AM Jan 90: These routines hopefully preserve previous PCC behaviour  */
/* but maybe ought to be reviewed as sysV PCC seems to differ.          */
static void pp_trimarg(char *abufarg)
{   while (pp_abufptr != abufarg &&
            (pp_abufptr[-1] == ' ' || pp_abufptr[-1] == PP_TOKSEP))
        pp_abufptr--;   /* trim off trailing spaces/token separators.   */
}

static void pp_spacearg(int ch)
{   /* Adds a space to the end of an argument, but keeps it in normal   */
    /* form (at most one space or PP_TOKSEP at end).                    */
    /* A space takes priority over PP_TOKSEP due to ANSI rules.         */
    /* Caller ensures pp_abufptr[-1] valid.  ch is space or PP_TOKSEP.  */
    if (pp_abufptr[-1] == PP_TOKSEP) pp_abufptr--;
    if (pp_abufptr[-1] != ' ') pp_wrch(ch);
}

static void pp_rd_args(PP_HASHENTRY *p, int32 uselinect)
{ int32 sav_abufptr = pp_abufptr - pp_abufbase;
  PP_ARGENTRY *a = pp_hasharglist_(p);         /* macro with parameters */
  int32 parcnt = 0, arglinect = 0;
  int32 abufarg;   /* now offset into pp_abufbase */
  int ch;
  pp_arg_align();
  abufarg = pp_abufptr - pp_abufbase;
  for (ch = pp_rdch();;)           /* read args */
  { switch (ch)
    { case PP_EOM: /* e.g. i(f) where i(x)=x and f=i(.                  */
                /* maybe the following line should pp_unrdch()?         */
                if (pp_scanidx > 0) pp_scanidx--;
      case EOF: cc_err(pp_err_rpar_eof, pp_hashname_(p), (long)uselinect);
                while (parcnt-- > 0) pp_wrch(')');
                goto endofargs;
      case '\n': ++arglinect;
                 if (arglinect == PP_ARGLINES_WARN_VAL)
                    cc_warn(pp_warn_many_arglines, arglinect);
                                /* drop through - treat nl in arg as space  */
      case '\t': ch = ' ';      /* drop through - treat tab in arg as space */
      case ' ': /* Ignore leading or multiple spaces within an arg.     */
                /* (Only visible via ansi stringify (#) so PCC mode ok) */
ansitoksep:     if (pp_abufptr != pp_abufbase+abufarg)
                    pp_spacearg(ch);
                break;
      default:  /* NB: we do not need to use pp_number here.            */
                if (ch == PP_TOKSEP)
                {   if (!(feature & FEATURE_PCC)) goto ansitoksep;
                    /* else for PCC ignore PP_TOKSEP in args.  Why?     */
                }
                else pp_wrch(ch);
                break;
      case '\'':
      case '"': (void)pp_copystring(ch); break;
      case '(': parcnt++; pp_wrch(ch); break;
      case ',': if (parcnt > 0) { pp_wrch(ch); break; }
                pp_trimarg(pp_abufbase+abufarg);
                pp_wrch(0);
                if (a != 0) pp_arg_link(a, abufarg),
                            pp_arg_align(),
                            abufarg = pp_abufptr - pp_abufbase,
                            a = pp_argchain_(a);
                if (a == 0)
                { cc_err(pp_err_many_args, pp_hashname_(p), (long)uselinect);
                  pp_abufptr = pp_abufbase + sav_abufptr;
                  return;
                }
                break;
      case ')': if (parcnt-- > 0) { pp_wrch(ch); break; }
      endofargs:
                pp_trimarg(pp_abufbase+abufarg);
                if (a == 0 && pp_abufptr != pp_abufbase+abufarg)
                {   /* no tokens allowed as arg in calls of #define f() ... */
                    cc_err(pp_err_many_args, pp_hashname_(p), (long)uselinect);
                    pp_abufptr = pp_abufbase + sav_abufptr;
                }
                pp_wrch(0);
                if (a != 0) pp_arg_link(a, abufarg),
                            a = pp_argchain_(a);
                if (a != 0)
                { cc_err(pp_err_few_args, pp_hashname_(p), (long)uselinect);
                    while (a != 0)
                    {   /* default missing arguments to "" */
                        pp_arg_align();
                        abufarg = pp_abufptr - pp_abufbase;
                        pp_wrch(0);
                        pp_arg_link(a, abufarg);
                        a = pp_argchain_(a);
                    }
                }
                return;
    }
    ch = pp_rdch();
  }
}

/* pp_checkid reads in a source id to see if it is a macro name, either
   outputting it unchanged or expanding it.  BEWARE - reuse of abuf.
   It notionally expands into ebuf, but there is a special case for
   speed (and to inhibit infinitely repeatedly trying to expand)
   for identifiers when the output is to abuf for pp_process().
*/
static bool pp_checkid(int pp_ch)
{ PP_HASHENTRY *p;
  int32 i = 0, hash = 0;
  int32 uselinect;
  int whitech;
  bool noexpandflag = 0;
/* @@@ currently pp_checkid() removes all PP_NOEXPAND chars, and adds   */
/* them again if we are argexpanding.  Is this what we want?            */
  if (pp_ch == PP_NOEXPAND)
    noexpandflag = 1,
    pp_ch = pp_rdch();  /* @@@ do/while below optimistic/buggy! */

  pp_abuf_ensure(PP_DEFLEN+1);
  do { if (i<PP_DEFLEN)
       { hash = HASH(hash, pp_ch);
         pp_abufptr[i++] = pp_ch;
       }
       do pp_ch = pp_rdch(); while (pp_ch == PP_NOEXPAND);
     } while pp_cidchar(pp_ch);
  pp_abufptr[i] = 0;
#ifdef ENABLE_PP                /* spurious: deadcode elimination fixes */
  if (debugging(DEBUG_PP))
    cc_msg("pp_checkid(%s%s)", pp_abufptr, noexpandflag ? "/noexpand":""),
       pp_showsleep(),
       cc_msg("\n");
#endif
  if (i == 1 && pp_widestrbeg(pp_abufptr[0], pp_ch) &&
      !(feature & FEATURE_PCC)) noexpandflag = 1;
  p = noexpandflag ? NULL : pp_lookup(pp_abufptr, hash);
  if (p == NULL)
  { if (!(pp_inhashif && strcmp("defined",pp_abufptr) == 0))
    { if (pp_scanidx < 0)
          /* The following line is notionally pp_wrbuf() but is so      */
          /* written for efficiency (main loop).                        */
          pp_abufptr += i;              /* leave chars in output buffer */
      else
      {   /* the following line looks a little odd, but currently       */
          /* we put a NOEXPAND quote around every id not expanded as    */
          /* part of a macro-arg so that IF it was suppressed then it   */
          /* will not get expanded for the body rescan of an outermore  */
          /* macro.  See comment at top of file.                        */
          pp_savch(PP_NOEXPAND);
          pp_savbuf(pp_abufptr, i);
      }
      pp_unrdch(pp_ch);
    }
    else                   /* poxy "defined id" or "defined(id)" in #if */
    { bool parens = 0;
      pp_ch = pp_skipb1(pp_ch);
      if (pp_ch == '(') { parens = 1; pp_ch = pp_skipb0(); }
      if (!pp_macstart(pp_ch)) cc_err(pp_err_if_defined);
      else
      { i = 0, hash = 0;
        pp_abuf_ensure(PP_DEFLEN+1);
        do { if (i<PP_DEFLEN)
                 hash = HASH(hash, pp_ch), pp_abufptr[i++] = pp_ch;
             pp_ch = pp_rdch();
           } while pp_cidchar(pp_ch);
        pp_abufptr[i] = 0;
        p = pp_lookup(pp_abufptr, hash);
        if (parens)
        { pp_ch = pp_skipb1(pp_ch);
          if (pp_ch == ')'); /* nothing to do */
          else cc_err(pp_err_if_defined1);
        }
        else pp_unrdch(pp_ch);
      }
      pp_expand(p != 0 ? pp_hashone : pp_hashzero, 0);
    }
    return 0;
  }
  if (pp_hashnoargs_(p))           /* macro with no parameters */
  { pp_unrdch(pp_ch);
    pp_expand(p,0);
    return 1;
  }
  uselinect = pp_linect;
/* AM, Jun 90: ensure ensure whitech sufficiently accurate so that      */
/* calls to unrdch() work when pp_argexpanding.                         */
  whitech = 0;
  if (pp_white(pp_ch)) whitech = pp_ch, pp_ch = pp_skipb0();
/* We have to be rather careful here with fn-like macros with no        */
/* following parens, especially since they may be followed by a #-line. */
/* The draft isn't especially clear, but since '(' is required to be    */
/* the next pp-token if it is present the draft seems unambiguous.      */
/* Consider examples like "f(getchar <nl>#whatever <nl> )".             */
/* Note the related "f(getchar( <nl>#whatever <nl> ))" is undefined.    */
  while (pp_ch == '\n') whitech = '\n', pp_ch = pp_skipb0();
  if (pp_ch != '(')                /* ANSI says ignore if no '(' present */
  { if (debugging(DEBUG_PP))
        cc_msg("pp_checkid(%s%c/%.2x not '(')\n", pp_abufptr, pp_ch, pp_ch);
    if (pp_scanidx < 0)
        pp_abufptr += i;                /* leave chars in output buffer */
    else
        pp_savbuf(pp_abufptr, i);
    pp_unrdch(pp_ch);                   /* hence PP_UNRDCHMAX>=2        */
    if (whitech) pp_unrdch(whitech);    /* don't lose poss. whitespace  */
    return 0;
  }
  else
  { /* @@@ later improve store efficiency by save/restore of pp_abufptr */
    pp_rd_args(p, uselinect);
    pp_expand(p, pp_linect-uselinect);
    return 1;
  }
}

static PP_HASHENTRY *pp_predefine2(char *s, int n)
{
  PP_HASHENTRY *p = (PP_HASHENTRY *) pp_new_(PP_NOARGHASHENTRY);
  int32 i = 0, hash = 0;
  int ch;


  pp_hashuses_(p) = 0;
  pp_hashalive_(p) = 1;
  pp_hashnoargs_(p) = 1;
  pp_hashismagic_(p) = n < 0;
  
/* @@@ LDS - because of #define __STDC__ 0 chaos, it may be useful to set */
/* pp_noifdef_(p) to 1 for some pre-defines. Should depend on -f?.        */
  
  pp_noifdef_(p) = 0;
  
  if (n < 0) pp_hashmagic_(p) = n; else pp_hashbody_(p) = "1";
  
  pp_hashdefchain_(p) = 0;
  pp_unchain_(p) = 0, pp_sleepleft_(p) = 0;     /* (init only to check) */
  
  while (ch = *s++, n<0 ? ch!=0 : pp_cidchar(ch))
     { if (i<PP_DEFLEN)
       { hash = HASH(hash, ch);
         pp_stuffid_(ch);
       }
     }
  
  pp_hashname_(p) = pp_closeid();
  
  switch (ch)
  {  default:  if (n >= 0)
                   cc_rerr(pp_rerr_illegal_option, pp_hashname_(p), s-1);
               /* drop through */
     case 0:   break;
     case '=': pp_hashbody_(p) = s; break;
  }
  
  pp_hashchain_(p) = (*pp_hashtable)[hash % PP_HASHSIZE];
  
  (*pp_hashtable)[hash % PP_HASHSIZE] = p;
  
  if (pp_hashfirst == 0)
    pp_hashfirst = pp_hashlast = p;
  else
    pp_hashdefchain_(pp_hashlast) = p, pp_hashlast = p;

  return p;
}

static PP_ARGENTRY *pp_addtoarglist(char *id, PP_ARGENTRY *a)
{   PP_ARGENTRY *p, *q;
    bool seen = 0;
    for ((p = a, q = 0);  p != 0;  (q = p, p = pp_argchain_(p)))
        if (strcmp(pp_argname_(p),id) == 0 && !seen)
            seen = 1, cc_rerr(pp_rerr_nonunique_formal, id);
    p = (PP_ARGENTRY *)pp_new_(sizeof(PP_ARGENTRY));
    pp_argchain_(p) = 0;
    pp_argname_(p) = id;
    pp_argactual_(p) = DUFF_OFFSET;
    return q==0 ? p : (pp_argchain_(q) = p, a);
}

static bool pp_eqarglist(PP_ARGENTRY *p, PP_ARGENTRY *q)
{   for (; p && q; p = pp_argchain_(p), q = pp_argchain_(q))
        if (strcmp(pp_argname_(p), pp_argname_(q)) != 0) return 0;
    return (p == q);
}

static PP_ARGENTRY *pp_findarg(PP_ARGENTRY *a, char *id, int32 n)
{   for (; a; a = pp_argchain_(a))
        if (pp_eqname(pp_argname_(a), id, n)) return a;
    return 0;
}

/* @@@ re-order next routine */
static void pp_skip_linetokens(int pp_ch)
{   for (;;) switch (pp_ch)
    {   case '\n': case EOF: pp_unrdch(pp_ch);
                             return;
        case '\'': case '"': pp_stuffstring(pp_ch,0,1);
                             /* drop through */
        default:             pp_ch = pp_rdch();
                             break;
    }
}

/*
 * One routine per pre-processor command...
 */

static void pp_define(int pp_ch, bool noifdef)
{ int32 i = 0, hash = 0;
  if (!pp_macstart(pp_ch))
  { cc_err(pp_err_missing_identifier); pp_skip_linetokens(pp_ch); return; }
  if (pp_skipping)
  {   /* This code is untidy, but corresponds to the ANSI rationale */
      /* that PP only needs to check for the name of a directive.   */
      /* However, improve on this behaviour one day!!!              */
      pp_skip_linetokens(pp_ch);
      return;
  }
  do { if (i<PP_DEFLEN)
       { hash = HASH(hash, pp_ch);
         pp_stuffid_(pp_ch);
         i++;
       }
       pp_ch = pp_rdch();
     } while (pp_cidchar(pp_ch));
  { char *name = pp_closeid();
    PP_HASHENTRY *p;
    PP_ARGENTRY *arglist = 0;
    enum { FIRST_TOKEN, NORMAL, SEEN_HASH, SEEN_HASHHASH } state = FIRST_TOKEN;
    int32 feature_comment = feature & FEATURE_PPCOMMENT;
    feature &= ~FEATURE_PPCOMMENT;
    if (pp_ch != '(')
      p = (PP_HASHENTRY *) pp_new_(PP_NOARGHASHENTRY), pp_hashnoargs_(p) = 1;
    else
    { int32 params = 0;
      p = (PP_HASHENTRY *) pp_new_(sizeof(PP_HASHENTRY));
      pp_hashnoargs_(p) = 0;
      pp_hasharglist_(p) = 0;
      do
       { pp_ch = pp_skipb0();
         if (!pp_macstart(pp_ch))
         { if (pp_ch == ')' && params == 0) break;
           cc_err(pp_err_missing_parameter, name);
           pp_skip_linetokens(pp_ch);
           feature |= feature_comment;
           return;
         }
         do { pp_stuffid_(pp_ch); pp_ch = pp_rdch();
            } while (pp_cidchar(pp_ch));
         pp_hasharglist_(p) = arglist =
             pp_addtoarglist(pp_closeid(), pp_hasharglist_(p));
         params++;
         pp_ch = pp_skipb1(pp_ch);
       } while (pp_ch == ',');
      if (pp_ch != ')')
      {   cc_err(pp_err_missing_comma, name);
          pp_skip_linetokens(pp_ch);
          feature |= feature_comment;
          return;
      }
      pp_ch = pp_rdch();
    }
    pp_noifdef_(p) = noifdef;
    pp_hashalive_(p) = 1;
    pp_hashismagic_(p) = 0;
    pp_hashuses_(p) = 0;
    pp_hashdefchain_(p) = 0;
    pp_unchain_(p) = 0, pp_sleepleft_(p) = 0;   /* (init only to check) */
    pp_hashname_(p) = name;
    pp_ch = pp_skipb1(pp_ch);
    for (;;)
    {
/* Turn multiple spaces (or tabs) to one space, lose trailing spaces.   */
/* Initial spaces have already been removed and comments are now space. */
/* Beware warnings for differing white space in pcc-mode (like Reiser?) */
        if ((pp_ch == ' ' || pp_ch == '\t') && !(feature & FEATURE_PCC))
        {        pp_ch = pp_skipb1(pp_ch);
                 if (!(pp_ch == '\n' || pp_ch == EOF)) pp_stuffid_(' ');
        }
	/*
	 * XXX
	 *
	 * Third test added by NC to catch
	 *
	 *    #define fred #
	 *
	 * which is legal.  This does mean that
	 *
	 *    #define fred() #
	 *
	 * won't be caught, even though it is illegal.
	 */
	
        if (state == SEEN_HASH && !pp_macstart(pp_ch))
	  {
	    state = NORMAL;

	    cc_rerr(pp_rerr_define_hash_arg);
	  }
	
        switch (pp_ch)
        {
    default:     if (pp_macstart(pp_ch))
                 {   int32 n = 0;
                     do n++, pp_stuffid_(pp_ch), pp_ch = pp_rdch();
                       while (pp_cidchar(pp_ch));
                     if (state == SEEN_HASH &&
                         !pp_findarg(arglist, pp_dbufptr-n, n))
                       cc_rerr(pp_rerr_define_hash_arg);
                     state = NORMAL;
                     continue;
                 }
    defolt:      pp_stuffid_(pp_ch);
                 break;
    case '\'':
    case '"':    pp_stuffstring(pp_ch,0,0);
                 break;

    case '#':    if (feature & FEATURE_PCC ||
		     arglist == 0)
      goto defolt;

                 pp_ch = pp_rdch();
      
                 if (pp_ch != '#')
                 { pp_stuffid_('#');
                   state = SEEN_HASH;
                 }
                 else
                 { if (state == FIRST_TOKEN)
                       cc_rerr(pp_rerr_define_hashhash);
                   pp_stuffid_('#');
                   pp_stuffid_('#');
                   pp_ch = pp_rdch();
                   state = SEEN_HASHHASH;
                 }
                 continue;
    case '\n':
    case EOF:
            if (state == SEEN_HASHHASH)
                cc_rerr(pp_rerr_define_hashhash);
            pp_hashbody_(p) = pp_closeid();
            { PP_HASHENTRY *q = pp_lookup(pp_hashname_(p), hash);
              if (q)
              { pp_hashalive_(q) = 0;   /* omit for a #define def. stack */
                if (pp_hashismagic_(q)  /* union => must be first test   */
                    || strcmp(pp_hashbody_(p), pp_hashbody_(q)) != 0
                    || pp_hashnoargs_(p) != pp_hashnoargs_(q)
                    || (!pp_hashnoargs_(p) &&
                        !pp_eqarglist(pp_hasharglist_(p),pp_hasharglist_(q))))
                  cc_pccwarn(pp_rerr_redefinition, pp_hashname_(p));
                else if (feature & (FEATURE_FUSSY|FEATURE_PREDECLARE))
                  cc_warn(pp_warn_redefinition, pp_hashname_(p));
              }
            }
            pp_hashchain_(p) = (*pp_hashtable)[hash % PP_HASHSIZE];
            (*pp_hashtable)[hash % PP_HASHSIZE] = p;
            if (pp_hashfirst == 0) pp_hashfirst = pp_hashlast = p;
            else pp_hashdefchain_(pp_hashlast) = p, pp_hashlast = p;
            pp_unrdch(pp_ch);
            feature |= feature_comment;
            return;               /* gasp */
        }
        state = NORMAL;
        pp_ch = pp_rdch();
    }
  }
}

static void pp_undef(int pp_ch)
{ int32 i = 0, hash = 0;
  if (!pp_macstart(pp_ch))
  { cc_err(pp_err_undef); pp_skip_linetokens(pp_ch); return; }
/* Assumption on ABUFSIZE implies  pp_abuf_ensure(PP_DEFLEN+1) OK.      */
  do { if (i<PP_DEFLEN)
       { hash = HASH(hash, pp_ch);
         pp_abufbase[i++] = pp_ch;
       }
       pp_ch = pp_rdch();
     } while (pp_cidchar(pp_ch));
  pp_unrdch(pp_ch);
  pp_abufbase[i++] = 0;
  if (!pp_skipping)
  { PP_HASHENTRY *p = pp_lookup(pp_abufbase, hash);
    if (p) pp_hashalive_(p) = 0;
  }
}

static void pp_addconditional(bool skipelsepart)
{ PP_IFSTACK *q = pp_freeifstack;
  if (q) pp_freeifstack = pp_ifchain_(q);
  else q = (PP_IFSTACK *) pp_new_(sizeof(PP_IFSTACK));
  pp_ifchain_(q) = pp_ifstack;
  pp_ifseenelse_(q) = 0;
  pp_ifoldskip_(q) = pp_skipping;               /* caller pp_skipping */
  pp_ifskipelse_(q) = pp_skipping || skipelsepart;     /* 'else' part */
  pp_ifstack = q;
  pp_skipping = pp_skipping || !skipelsepart;          /* 'then' part */
}

static void pp_h_ifdef(int pp_ch, bool skipelsepart)
{ PP_HASHENTRY *p;
  int32 i = 0, hash = 0;

  if (pp_macstart(pp_ch))
  {
/* Assumption on ABUFSIZE implies  pp_abuf_ensure(PP_DEFLEN+1) OK.      */
      do { if (i<PP_DEFLEN)
           { hash = HASH(hash, pp_ch);
             pp_abufbase[i++] = pp_ch;
           }
           pp_ch = pp_rdch();
         } while (pp_cidchar(pp_ch));
      pp_unrdch(pp_ch);
      pp_abufbase[i++] = 0;
      p = pp_lookup(pp_abufbase, hash);
#ifdef MACH_EXTNS
      if (p != 0 && pp_noifdef_(p))
          cc_warn(pp_warn_ifvaldef, pp_abufbase);
#endif
  }
  else
  {   p = NULL;
      if (feature & FEATURE_PCC)
          cc_warn(pp_err_ifdef);    /* @@@ is this really suppressible? */
      else
          cc_err(pp_err_ifdef);
      pp_skip_linetokens(pp_ch);
  }
  if (p != 0 && !pp_skipping) pp_hashuses_(p)++;
  pp_addconditional((p != 0) == skipelsepart);
}

static void pp_h_if(int pp_ch)
{   pp_unrdch(pp_ch);
    {   bool oskip = pp_skipping, b;
        pp_inhashif = oskip ? 2:1;  pp_skipping = 0;
        b = syn_hashif();               /* always read to check syntax  */
        pp_inhashif = 0;  pp_skipping = oskip;
        pp_addconditional(b);
        /* Assert: after syn_hashif(), at end of line or at end of file */
    }
}

static void pp_h_else(int pp_ch)
{ if (pp_ifstack == 0 || pp_ifseenelse_(pp_ifstack))
      cc_rerr(pp_rerr_spurious_else);
  else { pp_skipping = pp_ifskipelse_(pp_ifstack);
         pp_ifseenelse_(pp_ifstack) = 1;}
  pp_unrdch(pp_ch);
}

static void pp_h_elif(int pp_ch)
{   pp_unrdch(pp_ch);
    if (pp_ifstack == 0 || pp_ifseenelse_(pp_ifstack))
    {   cc_rerr(pp_rerr_spurious_elif);
        pp_skip_linetokens(pp_ch);
        return;
    }
    {   bool b;
        pp_inhashif = pp_ifskipelse_(pp_ifstack) ? 2:1;  pp_skipping = 0;
        b = syn_hashif();               /* always read to check syntax  */
        pp_inhashif = 0;
        if (b)
            pp_skipping = pp_ifskipelse_(pp_ifstack),
            pp_ifskipelse_(pp_ifstack) = 1;
        else
            pp_skipping = 1;
        pp_unrdch('\n');                  /* @@@ rather nasty */
    }
}

static void pp_h_endif(int pp_ch)
{ if (pp_ifstack == 0) cc_rerr(pp_rerr_spurious_endif);
  else { pp_skipping = pp_ifoldskip_(pp_ifstack);
         { PP_IFSTACK *q = pp_ifchain_(pp_ifstack);
           /* discard old */
           pp_ifchain_(pp_ifstack) = pp_freeifstack,
           pp_freeifstack = pp_ifstack;
           pp_ifstack = q;
         }
       }
  pp_unrdch(pp_ch);
}

static int pp_directive_expand(int ch)
{   pp_abufptr = pp_abufbase;
    while (pp_macstart(ch))
    {   pp_checkid(ch);
        if (pp_abufptr != pp_abufbase)
        {   /* This means an identifier did not expand (see pp_checkid) */
            /* undo the special treatment in pp_checkid().              */
            pp_abufptr = pp_abufbase;
            return PP_NOEXPAND;         /* sufficient for giving error. */
        }
        ch = pp_skipb0();
    }
    return ch;
}

static void pp_h_line(int pp_ch)
{ if (pp_skipping) { pp_skip_linetokens(pp_ch); return; }
  pp_ch = pp_directive_expand(pp_ch);
  if (isdigit(pp_ch))
  { int32 n = 0;
    while (isdigit(pp_ch)) n = n*10 + pp_ch-'0', pp_ch = pp_rdch();
    pp_linect = n-1;   /* we have yet to count the NL at the end */
    pp_ch = pp_skipb1(pp_ch);
    pp_ch = pp_directive_expand(pp_ch);
    if (pp_ch == '"')
    { pp_stuffstring('"',1,pp_skipping);
      pp_cisname = pp_closeid();    /* ensure done always */
      pp_ch = pp_directive_expand(pp_skipb0());
    }
    pp_unrdch(pp_ch);
  }
  else
  { cc_rerr(pp_rerr_hash_line);
    pp_skip_linetokens(pp_ch);
  }
}

void pp_push_include(char *fname, int lquote)
{
  FILE *fp;
  int rquote = (lquote == '<' ? '>' : lquote);
  char *saved_pp_cisname = pp_cisname;
      /* pp_cisname is set to the real, native file-name by push_include, */
      /* invoked from pp_inclopen() (in compiler.c). Here we save the old */
      /* value for stacking if the pp_inclopen() succeeds. Hackery. Yuck. */
/*
 * Here a null name is checked for specially lest sticking bits of a
 * search path onto it lead #include "" into getting treated as (for
 * instance) #include "./", where one could imagine the current directory
 * being opened for reading and the (binary?) junk in it leading to
 * large numbers of odd error messages. If somebody thinks of a good idea
 * for a USEFUL meaning for #include "" or #include <> here is where it
 * could be hooked in.  I probably vote against any such magic!
 */
  if (fname[0] != 0 &&
      (fp = pp_inclopen(fname,lquote=='<', &string_file_active)) != NULL)
  { if (lquote == '<' &&
        !(feature & FEATURE_PCC || suppress & D_PPNOSYSINCLUDECHECK))
    {   static char *ansiheaders[] = {
            "assert.h", "ctype.h", "errno.h", "float.h", "limits.h",
            "locale.h", "math.h", "setjmp.h", "signal.h", "stdarg.h",
            "stddef.h", "stdio.h", "stdlib.h", "string.h", "time.h" };
        bool found = 0;
        int i;
        for (i = 0; i < sizeof(ansiheaders)/sizeof(*ansiheaders); i++)
            if (strcmp(fname, ansiheaders[i]) == 0) found = 1;
        if (!found) cc_warn(pp_warn_nonansi_header, fname);
    }
  /* the following block is notionally a recursive call to pp_process()
     but that would mean a co-routine structure if used with the cc. */
    { PP_FILESTACK *fs = pp_freefilestack;
      if (fs) pp_freefilestack = pp_filchain_(fs);
      else fs = (PP_FILESTACK *) pp_new_(sizeof(PP_FILESTACK));
      pp_filchain_(fs) = pp_filestack;
/* This code screams out for a struct to save/restore not scalars! */
      pp_filstream_(fs) = pp_cis;         pp_cis = fp;
      pp_fileline_(fs)  = pp_linect + 1;  pp_linect = 1;
      pp_filename_(fs)  = saved_pp_cisname;
                               /* pp_cisname has been set by pp_inclopen */
#ifndef NO_LISTING_OUTPUT
      pp_filenumber_(fs) = pp_filenumber;
      /* NB profile_find() sets profile_ptr & pp_filenumber */
      pp_propoint_(fs) = profile_ptr; profile_find(fname);
      /* Set MSB of pp_filenumber if listing is not wanted at this level */
      if (!((lquote != '<' && (feature & FEATURE_USERINCLUDE_LISTING)) ||
          (feature & FEATURE_SYSINCLUDE_LISTING)))
      {   pp_filenumber |= (unsigned long)0x80000000;
          list_this_file = 0;
      }
      else list_this_file = 1;
#endif /* NO_LISTING_OUTPUT */
      pp_filestack = fs;
      pp_rdptr = NULL;       /* suppress ++pp_linect caused by last '\n' */
      pp_rdcnt = 0;          /* read, and ensure line buffer flushed.    */
    }
  }
  else
      cc_err(pp_err_include_file, (int)lquote,fname,(int)rquote);
}

/* Code in flux -- move typedef to top or merge with pp_filestack?      */
typedef struct file_name_list
{
    struct file_name_list *cdr;
    char fname[1];  /* typically rather longer than 1 byte */
} file_name_list;

static file_name_list *seen_before = NULL;

static void pp_include(int pp_ch)
{ /* AM: pp_include() now reads up to and including its terminating NL. */
  int lquote, rquote;
  char *fname;

  if (!pp_skipping) pp_ch = pp_directive_expand(pp_ch);

  switch (lquote = pp_ch)
  { case '"': rquote = '"'; break;
    case '<': rquote = '>'; break;
    default:  if (!pp_skipping) cc_err(pp_err_include_quote);
              pp_skip_linetokens(pp_ch);
              return;
  }
  pp_stuffstring(rquote,1,pp_skipping);
  fname = pp_closeid();    /* ensure done always */
  pp_ch = pp_directive_expand(pp_skipb0());
  if (!PP_EOLP(pp_ch) && pp_ch != EOF)
  {
    if (!pp_skipping)
      cc_err(pp_err_include_junk, (int)lquote, fname, (int)rquote);
    pp_skip_linetokens(pp_ch);
    (void)pp_rdch();
  }
  if (pp_skipping) return;

  { file_name_list *p = seen_before;
    while (p != NULL)
/* NB I treat "somename" and <somename> as equivalent here.             */
/* AM: lets open the discussion as to this.                             */
    { if (strcmp(p->fname, fname) == 0)
      {   pp_wrch('\n');    /* retain line-no synchronisation for cc -E */
          return;
      }
      else p = p->cdr;
    }
  }
  pp_push_include(fname, lquote);
}

/* Pragmas: syntax allowed (we can argue more later) is:
 * "#pragma -<letter><optional digit> Argument_List -<letter><optional digit>".
 * The effect of "#pragma -<letter>" is to set pp_pragmavec[letter-a] to -1
 * and "#pragma -<letter><digit>" to set pp_pragmavec[letter-a] to <digit>-0.
 * For the majority of these options a long-winded spelt out word can also
 * be given, as in
 * "#pragma no_warn_deprecated no_warn_implicit_fns"
 * where the particular words that are recognised and their expansions into
 * the more primitive -xn form are given in a table here.
 * (note: does anybody want to change these names? 21 March 89 is ACN's
 *  first bash at such a list and is probably in need of further refinement)
 */

typedef struct pragma_spelling
{
    char *name;
    short int code;
    short int value;
} pragma_spelling;

static pragma_spelling pragma_words[] =
{
    { "warn_implicit_fn_decls",     'a', 1 },
    { "check_memory_accesses",      'c', 1 },
    { "memory_access_checks",       'c', 1 },
    { "warn_deprecated",            'd', 1 },
    { "continue_after_hash_error",  'e', 1 },
    { "include_only_once",          'i', 1 }, /* @@@ freeze soon!        */
    { "optimise_crossjump",         'j', 1 },
    { "profile_statements",         'p', 2 },
#ifdef TARGET_IS_ARM
    { "optimise_multiple_loads",    'm', 1 },
    { "check_stack",                's', 0 },
#endif
#ifdef TARGET_IS_C40
    { "fast_fp",                    'k', 1 },    
    { "few_modules",                'l', 0 },    
    { "enable_backtrace",           'm', 1 },
    { "peepholing",                 'n', 0 },
    { "stack_checks",               's', 0 },
    { "check_stack",                's', 0 },
    { "little_data",                'w', 0 },
#endif
    { "force_top_level",            't', 1 }, /* @@@ freeze soon!        */
    { "check_printf_formats",       'v', 1 },
    { "check_scanf_formats",        'v', 2 },
    { "__compiler_msg_format_check",'v', 3 }, /* not for public use     */
    { "side_effects",               'y', 0 },
    { "optimise_cse",               'z', 1 }
};

#define NPRAGMAS (sizeof(pragma_words)/sizeof(pragma_spelling))

/* In the medium term the following routine is moving to compiler.c     */
/* Precondition: 'pragchar' is a lower case letter.                     */
static void main_pragma_set(int pragchar, int32 pragval)
{   pp_pragmavec[pragchar - 'a'] =  pragval;
#ifdef TARGET_IS_C40
    if (pragchar == 'c' && pragval == 1)
      {
	var_backtrace_enabled = 1;
	var_no_peepholing     = 1;	
      }    
#endif
    
#ifdef INMOSC
    /* Support for obsolete Norcroft C error suppression pragmas:       */
    switch (pragchar)
    {
case 'b':
        if (pragval) suppress &= ~D_IMPLICITCAST;
        else
        {   if (!(suppress & D_IMPLICITCAST))
                cc_warn("#pragma -b suppresses errors (hence non-ANSI)");
            suppress |= D_IMPLICITCAST;
        }
        break;
    }
#endif
}

static void pp_pragma(int pp_ch)
{   /* note that ANSI say it is NOT an error to fail to parse a #pragma */
    /* that does not stop us warning on syntax we fail to recognise     */
    for (;;)
    {   int pragchar; int32 pragval;
        pp_ch = pp_skipb1(pp_ch);
        switch (pp_ch)
        {
    default:
            {   char pragma_name[32];
                int p = 0, no = 0;
 /*
 * Read a word starting with whatever non-blank character happens, but
 * following on through alphanumeric characters plus _ (plus maybe $).
 * Case fold the word that is read, and truncate it to 30 characters.
 */
                do
                {   if (p<30) pragma_name[p++] = safe_tolower(pp_ch);
                    pp_ch = pp_rdch();
                } while (pp_cidchar(pp_ch));
                pragma_name[p] = 0;
                if (p > 3 && strncmp(pragma_name, "no_", 3) == 0) no = 3;
/* For the small number of available options linear search seems OK */
                for (p = 0; p < NPRAGMAS; p++)
                    if (strcmp(pragma_words[p].name, pragma_name+no) == 0)
                        break;
                if (p < NPRAGMAS)
                {   int val = pragma_words[p].value;
                    pragval = no ? !val : val;
                    pragchar = pragma_words[p].code;
                    if (!pp_skipping) main_pragma_set(pragchar, pragval);
                    continue;           /* try for more pragmas on line */
                }
                cc_warn(pp_warn_bad_pragma);
                pp_skip_linetokens(pp_ch);
                break;
            }
    case '\n':
            pp_unrdch(pp_ch);
            break;
    case '-':
            {   int32 n = 0; bool seen = 0;
                pp_ch = pp_rdch();
                if (isalpha(pp_ch)) pragchar = safe_tolower(pp_ch);
                else { cc_warn(pp_warn_bad_pragma1, (int)pp_ch);
                       pp_skip_linetokens(pp_ch);
                       break; }
                pp_ch = pp_rdch();
                while (isdigit(pp_ch))
                    seen = 1,
                    n = n*10 + (int)(pp_ch - '0'),
                    pp_ch = pp_rdch();
                pragval = seen ? n : -1;
                if (!pp_skipping) main_pragma_set(pragchar, pragval);
                continue;               /* try for more pragmas on line */
            }
        }
        break;
    }
/*
 * #pragma force_top_level
 * inserts the token ___toplevel into the input stream (and in the
 * process puts a constraint on UNRDCHMAX), which symbol is supposed
 * to tell the parser to moan if it is within any nested construction.
 * This is done this way since it seems nicer to publish that a #pragma
 * can be used to assert top-level-ness, but pp.c is not aware of
 * just where we are in a program, so must pass the checking on to syn.c.
 * The main intended use is to put #pragma force_top_level as the first
 * line of all standard header files so that
 *       junk
 *       #include <stdio.h>
 * can be handled better than was previously possible.
 */
    if (var_force_top_level > 0)
    {   int k;
/*
 * I force in a newline before the inserted __toplevel, but then need
 * to adjust pp_linect so that said newline does not mess up accounting
 * for ever more.  This is not entirely satisfactory, but since #pragma
 * has a pretty broad remit I think it is probably acceptable.
 */
        for (k=12; k>=0; k--) pp_unrdch("\n___toplevel "[k]);
        pp_linect--;
        var_force_top_level = 0;
    }
/*
 *       #pragma include_only_once
 * indicate that the current file should not be included again. The
 * name is taken as pp_cisname (which does not have quote marks or <>
 * around it).  Expected to be good for speed in some cases.
 * Note for those concerned with ANSI small print - the files names found
 * within "" and <> here are treated in an implementation defined manner
 * and that means that #pragma can change the interpretation - I deem
 * the include_only_once option to change the mapping to direct some
 * file names onto a notional empty file the second time around.
 */
    if (var_include_once > 0)
    {   file_name_list *p1 = seen_before;
        while (p1 != NULL)
        {   if (strcmp(p1->fname, pp_cisname) == 0) break;
            else p1 = p1->cdr;
        }
        if (p1 == NULL)
        {   p1 = (file_name_list *)pp_alloc(
              (int32)offsetof(file_name_list,fname) + strlen(pp_cisname) + 1);
            p1->cdr = seen_before;
            strcpy(p1->fname, pp_cisname);
            seen_before = p1;
        }
        var_include_once = 0;
    }
}

static void pp_h_error_ident(int pp_ch, bool iserror)
{   int32 n = 0;
    char msg[256];
    while (pp_ch != '\n' && pp_ch != EOF)
    {   if (n < 255) msg[n++] = pp_ch == '\t' ? ' ' : pp_ch;
        pp_ch = pp_rdch();
    }
    /* fill in terminator, and continue if last char was space */
    do msg[n] = 0; while (--n >= 0 && msg[n] == ' ');
/* the next line treats sysV #ident as a never moaning #error.         */
    if (iserror && !pp_skipping)
    {   /* Groan: the ANSI rationale recommends that #error terminates */
        /* compilation, but provides no way of fixup and continue.     */
        /* The following (hopefully) temporary fix provides #pragma -e */
        /* to continue.                                                */
        if (pp_pragmavec['e'-'a'] > 0)
            cc_rerr(pp_rerr_hash_error, msg);
        else
            cc_fatalerr(pp_fatalerr_hash_error, msg);
    }
    pp_skip_linetokens(pp_ch);          /* @@@ why? already at newline. */
}

static void pp_h_error(int pp_ch)
{   pp_h_error_ident(pp_ch,1);
}

#ifdef EXTENSION_SYSV
static void pp_h_ident(int pp_ch)
{   /* should we inhibit this when in a system include file?            */
    if (!(feature & FEATURE_PCC)) cc_rerr(pp_rerr_hash_ident);
    pp_h_error_ident(pp_ch,0);
}
#endif

static void pp_directive(void)
{
  int32 i=0;
  int pp_ch, cpp_allows_junk=0;
  char v[PP_DIRLEN+1];

  pp_ch = pp_skipb0();
  for (i=0; isalpha(pp_ch); )
  {
    if (i<PP_DIRLEN) v[i++] = pp_ch;  pp_ch = pp_rdch();
  }
  v[i] = 0;
  pp_ch = pp_skipb1(pp_ch);
  if      (strcmp(v, "include") == 0) { pp_include(pp_ch); return; }
  else if (strcmp(v, "define") == 0)  pp_define(pp_ch, 0);
#ifdef MACH_EXTNS
  else if (strcmp(v, "defineval") == 0)  pp_define(pp_ch, 1);
#endif
  else if (strcmp(v, "undef") == 0)   pp_undef(pp_ch), cpp_allows_junk=1;
  else if (strcmp(v, "if") == 0)
  { pp_h_if(pp_ch);
    /* Assert: after pp_h_if(), at end of line or at end of file */
    return;
  }
  else if (strcmp(v, "ifdef") == 0)   pp_h_ifdef(pp_ch, 1);
  else if (strcmp(v, "ifndef") == 0)  pp_h_ifdef(pp_ch, 0);
  else if (strcmp(v, "else") == 0)    pp_h_else(pp_ch), cpp_allows_junk=1;
  else if (strcmp(v, "elif") == 0)    pp_h_elif(pp_ch);
  else if (strcmp(v, "endif") == 0)   pp_h_endif(pp_ch), cpp_allows_junk=1;
  else if (strcmp(v, "line") == 0)    pp_h_line(pp_ch);
  else if (strcmp(v, "pragma") == 0)  pp_pragma(pp_ch);
  else if (strcmp(v, "error") == 0)   pp_h_error(pp_ch);
#ifdef EXTENSION_SYSV
  else if (strcmp(v, "ident") == 0)   pp_h_ident(pp_ch);    /* non-ansi */
#endif
  else if (strcmp(v, "") == 0)
  {   /* If chars left assume a #line (but only in pcc mode).           */
      /* Oughtn't we to discourage #<number> in f77?  PCC mode?         */
      if (!PP_EOLP(pp_ch)
#ifndef FORTRAN
           && (feature & FEATURE_PCC)
#endif
         )
          pp_h_line(pp_ch);
      else
          pp_unrdch(pp_ch);
  }
  else
  { /* AM: @@@ ANSI ambiguity.  Is #if 0; #wombat; #endif ok?           */
    if (!pp_skipping) cc_err(pp_err_unknown_directive, v);
    else if (feature & FEATURE_PREDECLARE)
        /* tick off the user anyway if (s)he asked for it!              */
        cc_warn(pp_err_unknown_directive, v);
    pp_skip_linetokens(pp_ch);
  }
  pp_ch = pp_skipb0();
  if (!PP_EOLP(pp_ch) && pp_ch != EOF)
  {
      if (!(cpp_allows_junk &&
              (feature & (FEATURE_PCC | FEATURE_LIMITED_PCC) ||
               suppress & D_PPALLOWJUNK)))
          cc_rerr(pp_rerr_junk_eol, v);
      pp_skip_linetokens(pp_ch);
  }
  else pp_unrdch(pp_ch);
}

#ifdef FORTRAN
/*
 * F77 INCLUDEs require *currently* sticky EOFs, or statement assembly
 * becomes screwed up.
 * The sticky condition is cleared (for one read) by pp_pop_include().
 */
static int pp_stick_at_eof;

void pp_pop_include(void)
{
    pp_stick_at_eof = 0;
}
#endif

/* pp_process() behaves like _filbuf in stdio.
   Analogously it could collect larger bits of text before returning.
   But beware the interaction via recursive calls via #if.
*/

/* pp_lastch really keeps a record of a preceding newline (or file start) */
/* for #<directive>.  Note that it is not updated on <space> or <tab>.    */
static int pp_lastch;   /* perhaps could be done via unrdch(). */

static int pp_process(void)
{
  int pp_ch;

  
  pp_abufptr = pp_abufbase;

  while (pp_abufptr == pp_abufbase) /* do {} while really. */
    {
      pp_ch = pp_rdch();

      switch (pp_ch)
        {
	case EOF:
	  if (string_file_active)
	    string_file_active = NO;
	  else
	    fclose(pp_cis);    /* see pp_include() */
	  
/* @@@ ANSI do not specify whether #if's must match in a #include file.   */
/* hence only check all #if's closed on real EOF.                         */

	  if (pp_filestack == 0)
	    {
	      if (pp_ifstack != 0) cc_err(pp_err_endif_eof);

	      return EOF;
	    }
#ifdef FORTRAN
	  if (pp_stick_at_eof) return EOF;
	  pp_stick_at_eof = 1;
#endif
	  pp_cis     = pp_filstream_(pp_filestack);
	  pp_cisname = pp_filename_(pp_filestack);
	  pp_linect  = pp_fileline_(pp_filestack);
	  
#ifndef NO_LISTING_OUTPUT
	  profile_ptr   = pp_propoint_(pp_filestack);
	  pp_filenumber  = pp_filenumber_(pp_filestack);
	  list_this_file = (pp_filenumber >= 0);
#endif /* NO_LISTING_OUTPUT */

	    {
	      PP_FILESTACK *q = pp_filchain_(pp_filestack);

	      /* discard old */
	      pp_filchain_(pp_filestack) = pp_freefilestack,
	      pp_freefilestack = pp_filestack;
	      pp_filestack = q;
	    }

	  pp_rdptr = NULL;
	  pp_rdcnt = 0;
	  pp_ch = '\n';             /* to set pp_lastch later...    */

	  /* The next line resets the (BSD-style) -I list in compiler.c           */

	  pp_inclclose();
	  break;

	case '#':
	  if (PP_EOLP(pp_lastch))
	    {
	      pp_directive(); pp_ch = '\n';
	    }
	  else if (!pp_skipping)
	    pp_wrch(pp_ch);
	  break;

	case ' ':
	case '\t':
	  if (!pp_skipping)
	    pp_wrch(pp_ch);
	  
#ifndef PASCAL_OR_FORTRAN
	  if (!(feature & FEATURE_PCC))
#endif
	    /* spaces may precede '#' only in ANSI C */
	    pp_ch = pp_lastch;
	  break;

	case '\n':
	  pp_wrch(pp_ch); break;  /* output nl even if skipping */
	  
#ifndef FORTRAN
	case '\'':
	case '"':
	  pp_ch = pp_copystring(pp_ch);
	  break;
	  
/* The following line is not needed to make pp_number work.             */
/*  case '.': if (!pp_skipping) pp_wrch(pp_ch); break;                  */
#endif
	  
	default:  if (!pp_skipping)
	  {
#ifndef FORTRAN
	    if (pp_macstart(pp_ch) || pp_ch == PP_NOEXPAND)
	      pp_checkid(pp_ch);
	    else if (isdigit(pp_ch))
	      pp_number(pp_ch);
	    else
#endif
	      if (pp_ch == PP_TOKSEP)
                {
		  if (!(feature & FEATURE_PCC)) pp_wrch(' ');
		  /* else lose it in PCC mode.                        */
                }
	      else
		pp_wrch(pp_ch);
	  }
        }
      pp_lastch = pp_ch;
    }
    
  pp_abufoptr = pp_abufbase;

  return *pp_abufoptr++;
}

#ifndef NO_LISTING_OUTPUT
static int pp_listchar(int ch)
{   /* AM: merge this code with other FEATURE_UNEXPANDED... code above? */
    if (pp_filenumber >= 0 && !(feature & FEATURE_UNEXPANDED_LISTING))
    {   if (ch != EOF)
        {   putc(ch, listingstream);
            if (ch == '\n')
            {   listing_diagnostics();
                if (profile_count) listing_nextline(pp_linect);
                fprintf(listingstream, "%6lu  ", pp_linect + 1);
            }
        } else putc('\n', listingstream);
    }
    return ch;
}
#endif

/*
 * Exported things ...
 */

int pp_nextchar(void)
{
    int ch;
/* pp_process is in a better position to remove PP_NOEXPAND!            */
    do {
      ch = ((pp_abufoptr < pp_abufptr) ? *pp_abufoptr++ : pp_process());
    } while (ch == PP_NOEXPAND);

#ifndef NO_LISTING_OUTPUT
    /* AM presumes that pp_listchar is an identity fn to help regalloc  */
    /* compile this routine into fast code?  Hmm.                       */
    if (pp_listingstream_slave) return pp_listchar(ch);
#endif /* NO_LISTING_OUTPUT */
    return ch;
}

void pp_predefine(char *s)
{
    (void)pp_predefine2(s,1);
}

void pp_preundefine(char *s)
{
  int32 i = 0, hash = 0;
/* Assumption on ABUFSIZE implies  pp_abuf_ensure(PP_DEFLEN+1) OK.      */
  for (;;)
  {   int ch = *s++;
      if (!pp_cidchar(ch)) break;
      if (i < PP_DEFLEN) hash = HASH(hash, ch), pp_abufbase[i++] = ch;
  }
  pp_abufbase[i] = 0;

  if (!pp_skipping)                            /* @@@ duh? */
  {
      PP_HASHENTRY *p = pp_lookup(pp_abufbase, hash);
      if (p) pp_hashalive_(p) = 0;
  }
}

void pp_tidyup(void)
{ PP_HASHENTRY *p;
  if (feature & FEATURE_PPNOUSE)
      for (p = pp_hashfirst; p != 0; p = pp_hashdefchain_(p))
      { if (pp_hashuses_(p) == 0)
          cc_warn(pp_warn_unused_macro, pp_hashname_(p));
      }
#ifndef NO_LISTING_OUTPUT
  /* This next line must happen after all diagnostics have been produced */
  if (listingstream != NULL)
  {   listing_diagnostics();
      if (profile_count != 0) putc('\n', listingstream);
  }
#endif
#ifdef ENABLE_PP                /* spurious: deadcode elimination fixes */
  if (debugging(DEBUG_PP))
  { int32 i, argcnt;
    PP_ARGENTRY *a;
    cc_msg("%ld substitutions\n", (long)pp_nsubsts);
    cc_msg("Hash table:\n");
    for (i=0; i<PP_HASHSIZE; i++)
      for (p = (*pp_hashtable)[i]; p != 0; p = pp_hashchain_(p))
        { cc_msg("%ld: %s", (long)i, pp_hashname_(p));
          if (!pp_hashnoargs_(p))
          { cc_msg("(");
            for (a = pp_hasharglist_(p), argcnt=0; a != 0; a=pp_argchain_(a))
              cc_msg("%s%s", (argcnt++ == 0 ? "" : ","), pp_argname_(a));
            cc_msg(")");
          }
          cc_msg(" '%s'%s uses %ld\n",
                 (pp_hashismagic_(p) ? "<magic>" : pp_hashbody_(p)),
                 (pp_hashalive_(p) ? "" : " (undef'd)"),
                 (long)pp_hashuses_(p));
        }
  }
#endif
}

void pp_init(void)
{
  time_t t0 = time(0);
  

  strncpy(pp_datetime, ctime(&t0), 26-1);   /* be cautious */

  pp_hashtable = (PP_HASHENTRY *((*)[PP_HASHSIZE])) pp_alloc(sizeof(*pp_hashtable));
  
  memclr(pp_hashtable, sizeof(*pp_hashtable));
  
  pp_hashfirst = pp_hashlast = pp_noexpand = 0;
  pp_dbufend   = pp_dbufseg  = pp_dbufptr  = 0;

  pp_ebufbase  = pp_alloc(PP_EBUFINITSIZ);
  
  pp_ebufptr   = pp_ebuftop = pp_ebufbase + PP_UNRDCHMAX;
  pp_ebufend   = pp_ebufbase + PP_EBUFINITSIZ;
  pp_abufptr   = pp_abufoptr = pp_abufbase = pp_alloc(PP_ABUFINITSIZ);
  pp_abufend   = pp_abufbase + PP_ABUFINITSIZ;
  
  pp_scanidx       = -1;
  pp_linect        = 1;
  pp_nsubsts       = 0;
  pp_instring      = 0;
  pp_inhashif      = 0;
  pp_expand_level  = 0;
  pp_ifstack       = 0;
  pp_freeifstack   = 0;
  pp_skipping      = 0;
  pp_filestack     = 0;
  pp_freefilestack = 0;
#ifdef FORTRAN
  pp_stick_at_eof  = 1;
#endif
  
  seen_before = NULL;
  
  {   int ch;
      for (ch = 0; ch <= UCHAR_MAX; ch++)
      {   int i = 0;
          if (isalpha(ch) || ch == '_') i = PP_MACSTART + PP_CIDCHAR;
          if (isalnum(ch)) i |= PP_CIDCHAR;
          pp_ctype[ch] = i;
      }
  }
  
  pp_ctype[' ']  = PP_WHITE;
  pp_ctype['\t'] = PP_WHITE;
  pp_ctype[(unsigned char)PP_TOKSEP] = PP_WHITE;
  
/* args, options... */
  
  {  int i;     /* @@@ AM migrated to mip/compiler.c, going again... */
     for (i=0; i <= 'z'-'a'; i++) pp_pragmavec[i] = -1;
  }
  
  pp_cis = stdin;  pp_cisname = "<stdin>";  string_file_active = NO;
  
  (void)pp_predefine2("__LINE__", PP__LINE);
  (void)pp_predefine2("__FILE__", PP__FILE);
  (void)pp_predefine2("__DATE__", PP__DATE);
  (void)pp_predefine2("__TIME__", PP__TIME);

  /* __STDC__ gets set up after -zi command line pre-include file!      */
  
  pp_hashone = pp_predefine2("!!ONE!!",  PP__ONE);  /* for #if defined(...) */
  pp_hashzero = pp_predefine2("!!ZERO!!", PP__ZERO);
  
  (void)pp_predefine2("__CC_NORCROFT", PP__ONE);
  
  return;
  
}

static void pp_init2(bool is_preinclude)
{ int ch;
/* The strange order here has to do with the definedness of conversions */
/* between int and signed/unsigned char. See early comment re RARE_char.*/
  for (ch = 0;  ch <= UCHAR_MAX;  ++ch)
  {   int tch;
      if (isprint(ch)             ||  /* most common case */
          (feature & FEATURE_PCC) ||  /* no more conversion */
          ch == '\t'              ||  /* allowable space ch */
          legal_non_isprint(ch))
          tch = ch;
      else if (isspace(ch))
          /* convert all other end-of-line chars to '\n' */
          tch = '\n';
      else
          tch = PP_TOKSEP;
      pp_translate[ch] = tch;
  }
#ifdef PASCAL
  pp_translate['\\'] = '\\';
#else
  pp_translate['\\'] = PP_TOKSEP;
#endif
  pp_translate['\n'] = PP_TOKSEP;
#ifndef FORTRAN         /* COMMENT_START = NOT_A_CHARACTER in FORTRAN   */
  pp_translate[COMMENT_START] = PP_TOKSEP;
#endif
#ifdef PASCAL
  pp_translate['{']  = PP_TOKSEP;
#endif
  if (feature & FEATURE_PCC)
      pp_ctype['$']  = PP_MACSTART + PP_CIDCHAR;
  else
  {   if (!is_preinclude)
      {
#ifdef PASCAL
          (void)pp_predefine2("__ISO__", PP__STDC);
#else
          (void)pp_predefine2("__STDC__", PP__STDC);
#endif
      }
#ifndef PASCAL_OR_FORTRAN
      pp_translate['?'] = PP_TOKSEP;            /* translate triglyphs */
#endif
  }

  pp_rdptr = NULL;             /* Don't fault empty files (pp_fillbuf) */
  pp_rdcnt = 0;

  pp_lastch = '\n';
  pp_rdch1nls = pp_rdch3nls = 0;
}

/* Perhaps better another external function than partial init on        */
/* notesource (which sounds innocent)                                   */
#ifdef INMOSC
void pp_notesource(char *filename)      /* preserve interface           */
{   bool is_preinclude = 0;
#else
void pp_notesource(char *filename, bool is_preinclude)
{
#endif
    pp_cisname = filename;
    pp_init2(is_preinclude);
    if (is_preinclude) return;
#ifndef NO_LISTING_OUTPUT
    pp_listingstream_slave = listingstream;
    profile_find(filename);      /* sets profile_ptr, pp_filenumber */
#endif
}

/* end of pp.c */
