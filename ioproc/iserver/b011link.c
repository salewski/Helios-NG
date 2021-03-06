
/*
 --   ---------------------------------------------------------------------------
 --
 --      ISERVER  -  INMOS standard file server
 --
 --      b011link.c
 --
 --      Link module for B011 boards sans device driver
 --
 --      Copyright (c) INMOS Ltd., 1988.
 --      All Rights Reserved.
 --
 --   ---------------------------------------------------------------------------
*/



#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <signal.h>
#include <pwd.h>

#include "inmos.h"
#include "iserver.h"

EXTERN BYTE *rindex();
EXTERN caddr_t mmap();

#define LOCK_FILE_ROOT "/tmp/B011@"

#define VME_DEVICE "/dev/vme24d32"

#define DEFAULT_BASE_ADDRESS 0x800000                                      /* address set on the B011 */


#define PAGESIZE 8192                                                      /* as returned by pagesize(1) */

PRIVATE struct B011Layout
   {
      BYTE d0[ PAGESIZE - 32 ];                                            /*  Space                   */
      BYTE d1[3];
      BYTE ResetAndError;                                                  /*  Reset and error         */
      BYTE d2[3];
      BYTE Analyse;                                                        /*  Analyse                 */
      BYTE d3[17];
      BYTE Idr;                                                            /*  Input data register     */
      BYTE d4;
      BYTE Odr;                                                            /*  Output data register    */
      BYTE d5;
      BYTE Isr;                                                            /*  Input status register   */
      BYTE d6;
      BYTE Osr;                                                            /*  Output status register  */
      BYTE d7;  
   } *B;


#define NULL_LINK -1
#define LINK_TIMEOUT 100000


PRIVATE int BaseAddress;

PRIVATE LockFileName[32];

PRIVATE LINK ActiveLink = NULL_LINK;

EXTERN LINK TheLink;




/*
 *   Write a lock file in /tmp
 */
 
PRIVATE BOOL LockBoard(Address)
     int Address;
{
   BYTE String[128], *c;
   FILE *LockFd;
   long Seconds;
   struct passwd *pw;
   
   sprintf( LockFileName, "%s%X", LOCK_FILE_ROOT, Address );

   if ( ( LockFd = fopen( LockFileName, "r" ) ) != NULL )
      {
	 if ( fgets( String, 127, LockFd ) == NULL )
	    strcpy( String, "???");
	 else 
	    if ( ( c = rindex( String, '\n') ) != NULL )
	       *c = '\0';
	 fprintf( stderr, "B011 at 0x%X is locked by %s", Address, String );

	 if ( fgets( String, 99, LockFd ) == NULL )
	    strcpy( String, "?");
	 else 
	    if ( ( c = rindex( String, '\n') ) != NULL )
	       *c = '\0';
	 fprintf( stderr, " since %s.\n", String );

	 (void)fclose( LockFd );
	 return( FALSE );
      }

   if ( ( LockFd = fopen( LockFileName, "w" ) ) == NULL )
      {
	  fprintf(stderr,"B011 : cannot create lock file \"%s\"\n", LockFileName );
	  return(FALSE);
      }
   else
      {
	  pw = getpwnam(cuserid(NULL));
	  (void) time( &Seconds );
	  fprintf( LockFd, "%s (%s)\n%s\n", cuserid(NULL), pw->pw_gecos, ctime(&Seconds) );
	  (void) fclose( LockFd );
	  chown( LockFileName, getuid(), getgid() );
      }
   return(TRUE);
}




/*
 *   Delete the lock file
 */
 
PRIVATE BOOL UnlockBoard()
{
   FILE *LockFd;

   if ( ( LockFd = fopen( LockFileName, "r" ) ) == NULL )
      {
	  fprintf(stderr,"B011 : cannot find the lock file \"%s\"\n", LockFileName );
	  return(FALSE);
      }
   else
      {
	 (void) fclose( LockFd );
	 if ( unlink( LockFileName ) )
	    {
	       fprintf(stderr, "B011 : failed to delete the lock file %s\n", LockFileName);
	       return(FALSE);
	    }
	 else
	    return(TRUE);
      }
}




/*
 *   Signal handler to catch segmentation violations
 */
 
PRIVATE VOID BusErrorHandler ()
{

   fprintf( stderr, "B011 : no hardware at address %X\n", BaseAddress );
   UnlockBoard();
   ABORT(MISC_EXIT, (SE, "unable to access transputer hardware"));
}


/*
 *   Routine to map the B011 structure onto real VME addresses
 */
 
PRIVATE BOOL MapBoard(Offset)
     int Offset;
{
   int VmeFd;
   BYTE HaveNoVolatileConstants;
   
   if ( ( VmeFd = open( VME_DEVICE , O_RDWR) ) == -1 )
      {
	 fprintf(stderr,"B011 : Cannot open VME device %s\n", VME_DEVICE );
	 return(FALSE);
      }

   if ( ( B = (struct B011Layout *)valloc(sizeof(*B)) ) == NULL )
      {
	 fprintf(stderr, "B011 : Cannot valloc memory\n");
	 return(FALSE);
      }

   if ( (int)( B = (struct B011Layout *)mmap( B, PAGESIZE, PROT_READ | PROT_WRITE, MAP_SHARED, VmeFd, Offset ) ) == -1 )
      {
	 fprintf(stderr, "B011 : Cannot mmap board addresses\n" );
	 (void) close( VmeFd );
	 return(FALSE);
      }

   signal( SIGBUS, BusErrorHandler );
   HaveNoVolatileConstants = B->ResetAndError;
   signal( SIGBUS, SIG_IGN );

   return(TRUE);
}




/*
 *   Open Link
 */

PUBLIC LINK OpenLink ( Name )
   BYTE *Name;
{
   FILE *LockFd;
   BYTE String[100];
   BYTE *c;

   if ( ActiveLink != NULL_LINK )
      return( ER_LINK_CANT );

   if ( *Name == '\0' || Name == NULL )
      BaseAddress = DEFAULT_BASE_ADDRESS;
   else if ( sscanf( Name, "#%x", &BaseAddress ) != 1 &&
	     sscanf( Name, "%x", &BaseAddress ) != 1)
	 return(ER_LINK_BAD);

   if ( ActiveLink != NULL_LINK )
      return( ER_LINK_CANT );

   if ( LockBoard(BaseAddress) == FALSE )
      return( ER_LINK_CANT );

   if ( MapBoard(BaseAddress + 0x200000 - PAGESIZE) == FALSE )
      {
	 UnlockBoard();
	 return( ER_LINK_CANT );
      }

   setreuid(-1, getuid());
   setregid(-1, getgid());

   ActiveLink = 1;
   return(ActiveLink);
}




/*
 *   Close Link
 */

PUBLIC int CloseLink ( LinkId )
   LINK LinkId;
{
   if ( LinkId != ActiveLink )
      return( -1 );
   if ( UnlockBoard() == FALSE )
      return( -2 );
   ActiveLink = NULL_LINK;
   return(TRUE);
}




/*
 *   Read Link
 */
 
PUBLIC int ReadLink ( LinkId, Buffer, Count, Timeout )
   LINK LinkId;
   char *Buffer;
   unsigned int Count;
   int Timeout;
{
   register int n, l ;

   if ( LinkId != ActiveLink )
      return( -1 );

   if ( Count < 1 )
      return( -2 );
   if ( Timeout )
      {
	 for( n=0 ; Count-- ; n++ )
	    {
	       for( l=0 ; l< LINK_TIMEOUT ; l++ )
		  if ( B->Isr & 1 )
		     break;
	       if ( l == LINK_TIMEOUT )
		  break;
#ifdef FROSTED_FLAKES
	       {
		  *Buffer = B->Idr & 0xff;
		  printf( "<%X", *Buffer++ );
	       }
#else
	       *Buffer++ = B->Idr & 0xff;
#endif
	    }
      }
   else
      {
	 for( n=0 ; Count-- ; n++ )
	    {
	       while( ( B->Isr & 1 ) == 0)
		  ;
	       *Buffer++ = B->Idr & 0xff;
	    }
      }
   return( n );
}




/*
 *   Write Link
 */
 
PUBLIC int WriteLink ( LinkId, Buffer, Count, Timeout )
   LINK LinkId;
   char *Buffer;
   unsigned int Count;
   int Timeout;
{
   register int l, n, c ;

   if ( LinkId != ActiveLink )
      return( -1 );

   if ( Count < 1 )
      return( -2 );
   c = Count;
   if ( Timeout )
      {
	 for( n=0 ; c-- ; n++ )
	    {
	       for( l=0 ; l< LINK_TIMEOUT ; l++ )
		  if ( B->Osr & 1  )
		     break;
	       if ( l == LINK_TIMEOUT )
		  break;
#ifdef FROSTED_FLAKES
	       {
		  B->Odr = *Buffer & 0xff;
		  printf( ">%X", *Buffer++ &0xFF );
	       }
#else
	       B->Odr = *Buffer++ & 0xff;
#endif
	    }
      }
   else
      {
	 for( n=0 ; Count-- ; n++ )
	    {
	       while( ( B->Osr & 1 ) == 0)
		  ;
	       B->Odr = *Buffer++ & 0xff;
	    }
      }
   return( n );
}




/*
 *   Reset Link
 */
 
PUBLIC int ResetLink ( LinkId )
   LINK LinkId;
{
   if ( LinkId != ActiveLink )
      return( -1 );

   B->Analyse = 0;                                                         /*  deassert analyse  */
   usleep(1);                                                              /*  wait a while      */
   B->ResetAndError = 0;                                                   /*  deassert reset    */
   usleep(1);               
   B->ResetAndError = 1;                                                   /*  assert reset      */
   usleep(1);               
   B->ResetAndError = 0;                                                   /* deassert reset     */
   usleep(1);               
   return( 1 );
}




/*
 *   Analyse Link
 */
 
PUBLIC int AnalyseLink ( LinkId )
   LINK LinkId;
{
   if ( LinkId != ActiveLink )
      return( -1 );
   B->Analyse = 1;                                                         /*  assert analyse    */
   usleep(1);
   B->ResetAndError = 1;                                                   /*  assert reset      */
   usleep(1);               
   B->ResetAndError = 0;                                                   /*  deassert reset    */
   usleep(1);
   B->Analyse = 0;                                                         /*  deassert analyse  */
   usleep(1);               
   return( 1 );
}




/*
 *   Test Error
 */
 
PUBLIC int TestError ( LinkId )
   LINK LinkId;
{
   if ( LinkId != ActiveLink )
      return( -1 );
   return ( (B->ResetAndError & 1) ? 1 : 0 ) ;
}




/*
 *   Test Read
 */
 
PUBLIC int TestRead ( LinkId )
   LINK LinkId;
{
   if ( LinkId != ActiveLink )
      return( -1 );
   return ( ( B->Isr & 1) ? 1 : 0 ) ;
}




/*
 *   Test Write
 */
 
PUBLIC int TestWrite ( LinkId )
   LINK LinkId;
{
   if ( LinkId != ActiveLink )
      return( -1 );
   return ( ( B->Osr & 1 ) ? 1 : 0 ) ;
}



/*
 *   Eof
 */


