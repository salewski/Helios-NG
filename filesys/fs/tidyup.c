/* 16.08.90 - Some basic cuts to build the "integrated" version
 *
 */

/*************************************************************************
**                                                                      **
**          H E L I O S   F I L E S Y S T E M   C H E C K E R           **
**          -------------------------------------------------           **
**									**
**                  Copyright (C) 1989 Parsytec GmbH                    **
**                         All Rights Reserved.                         **
**                                                                      **
** tidyup.c								**
**									**
**      - Do everything needed to finish consistence checking,		**
**	  especially rebuilding the summary informations		**
**                                                                      **
**************************************************************************
** HISTORY   :                                                          **
**------------       							**
** Author    :   18/04/89 : H.J.Ermen 					**
**		 24/01/90 : H.J.Ermen, modified for stand-alone usage	**
*************************************************************************/

#include "check.h"

#define   DEBUG		0


/**************************************************************************
 * TIDY UP OPERATIONS AND UPDATING OF INFO DATA STRUCTURES
 *
 * - Various operations take place here :
 *   > The hash-table for symbolic links is inspected and all still remaining
 *     entries (...which are "hanging" links...) are removed.
 *   > A last pass over the ref-map and the original maps is performed and
 *     the different values are compared.
 *   > The summary-informations for each cg and for the whole file-system
 *     are corrected to the actual values. 
 *
 * Parameter  : - nothing -
 * Return     : TRUE  = no error at all
 *		FALSE = occurrence of an error
 *
 *************************************************************************/
word
tidy_update ( void )
{
 word cgnr, bcnt, i, changes, sum_changes;
 word nbfree, fs_nbfree, fs_ndir;
 struct buf *bp, *bsum;
 struct de_link *lpt;
 
#if DEBUG
IOdebug ("	tidy_update :	Make some final corrections and adjustments");
#endif

 /*---------------------  Remove "hanging" links  ----------------------*/
 
					/* If we have any links found,	*/
 if ( found_links && 			/* we also have to look for	*/
      ! ignore_links )			/* those which are 'hanging':	*/
 {
 	 IOdebug ("%s***    Step 4.1 : Delete all 'hanging' links found", 
 	 	   S_INFO);

	 for ( i = 0 ; i < LINKHASHSZ ; i++ )
 	 {				/* Start with the first element */
 	 				/* in a row			*/
 		lpt = link_hash_tab[i].lnnxt;	
 	
 					/* Follow the row up to it's end*/
 		while ( lpt != (struct de_link *) NULL )
 		{
 					/* Delete all link-entries	*/
 					/* which are found on the way.	*/
 			IOdebug ("%sDeleting 'hanging' link which refers to :\n[%s]",
 				    S_INFO, lpt->path );
#if 0
 			    		/* Read the dir-block which	*/
 			    		/* holds the link - inode.	*/
 			bp = bread ( 0, lpt->i_bnr, 1, SAVEA );

 					/* ... and delete the entry	*/
#if DEBUG
IOdebug ("	tidy_update :	Update dir-block %d at offset position %d",
	 lpt->i_bnr, lpt->i_off );
#endif
 			memset ( &bp->b_un.b_dir[lpt->i_off], 0, sizeof (struct dir_elem) );
 					/* Write the modified dir-block */
 					/* back to disk.		*/
 			test_bwrite ( bp );	
 		
 					/* Read the parent-dir inode	*/
 			bp = bread ( 0, lpt->pi_bnr, 1, SAVEA );

 					/* Correct the number of entries*/
 			bp->b_un.b_dir[lpt->pi_off].de_inode.i_spare--;
 					/* ... and the total size	*/
 			bp->b_un.b_dir[lpt->pi_off].de_inode.i_size -= sizeof (struct dir_elem);
 		
#if DEBUG
IOdebug ("	tidy_update :	Correct the parent-inode: block %d and slot %d",
	 lpt->pi_bnr, lpt->pi_off );
#endif
					/* One more hanging link which	*/
					/* was deleted.			*/
			fst.hanging_links++;	
					
					/* Write the modified parent -	*/
					/* inode back to disk.		*/
 			test_bwrite ( bp );
 					
 					/* Get the next element in the	*/
 					/* linked list.			*/
#endif 
			lpt = lpt->lnnxt;
 		}
 	}
 }

 /*----  A final step for comparison ref-map and original bit-maps  ----*/
 

 IOdebug ("%s***    Step 4.2 : Final comparison of ref-maps and bit-maps",
 	   S_INFO);

 					/* Work on each cg		*/
 for ( cgnr = 0 ; cgnr < i_fs.fs_ncg ; cgnr++ )
 {					/* Get the cg - info-block	*/
	bp = bread ( 0, info_blocks[cgnr], 1, SAVEA ); 	

					/* Work on all blocks kept in 	*/
					/* the cylinder-group		*/
	for ( bcnt = 0, changes = 0 ; bcnt < i_fs.fs_cgsize ; bcnt++ )
	{
					/* Remaining 'lost' blocks are	*/
					/* freed			*/
		if ( ( bit_maps[cgnr][bcnt] & MASK_REFCNT ) == 0  &&
		       bp->b_un.b_info->cgx.cg_free[bcnt] != 0 )
		{
			bp->b_un.b_info->cgx.cg_free[bcnt] = 0;
			bp->b_un.b_info->cgx.cg_s.s_nbfree++;
			changes++;
		} 
					/* New allocated blocks have to */
					/* be denoted.			*/
		if ( ( bit_maps[cgnr][bcnt] & MASK_REFCNT ) > 0  &&
		       bp->b_un.b_info->cgx.cg_free[bcnt] == 0 )
		{
			bp->b_un.b_info->cgx.cg_free[bcnt] = 0xff;
			bp->b_un.b_info->cgx.cg_s.s_nbfree--;
			changes++;
		} 
	} /* end <for (bcnt)> */
	
	if ( changes )			/* Were there any differences	*/
	{				/* found and corrected ?	*/
#if DEBUG
IOdebug ("	tidy_update :	cg = %d is updated (different # of free blocks found)",
	 cgnr );
#endif
		test_bwrite ( bp );	
	}
	else
		brelse ( bp->b_tbp, TAIL );
		
 } /* end <for (cgnr)> */

 /*---------------  Update the cg summary informations  ----------------*/

 IOdebug ("%s***    Step 4.3 : Update the summary informations for each cg",
 	   S_INFO);
 
 fs_nbfree = 0;				/* Total num of free blocks in	*/
 fs_ndir   = 0;				/* the file-system		*/
 					
 					/* Get the summary - block	*/
 bsum = bread ( 0, 1, 1, SAVEA );
					/* The summary informations of	*/
					/* each cylinder-group		*/
 for ( sum_changes = 0 , cgnr = 0 ; cgnr < i_fs.fs_ncg ; cgnr++ )
 {					/* At first : no changes at all.*/
 	changes = 0;
 					/* Get the info-block		*/
 	bp = bread ( 0, info_blocks[cgnr], 1, SAVEA );

	/* correct illegal cylindergroup numbers in info block */
	if ( bp->b_un.b_info->cgx.cg_cgx != cgnr ) {
	IOdebug ("%sCgnr in info block %d is corrected to %d!", S_WARNING, cgnr, cgnr);
		bp->b_un.b_info->cgx.cg_cgx = cgnr;
		changes++;
	}

 					/* Work on the free-map		*/
 	for ( bcnt = 0 , nbfree = 0 ;
 	      bcnt < i_fs.fs_cgsize ; bcnt++ )
 	{				/* Count the # of free blocks	*/
 		if ( bp->b_un.b_info->cgx.cg_free[bcnt] == 0 )
 			nbfree++;
 	}
 	
#if DEBUG
IOdebug ("	tidy_update :	cg = %d , Free blocks  counted = %d | found = %d",
	 cgnr, nbfree, bp->b_un.b_info->cgx.cg_s.s_nbfree );
#endif
					/* A different number of free	*/
					/* blocks counted and found ?	*/
	if ( nbfree != bp->b_un.b_info->cgx.cg_s.s_nbfree )
	{
		bp->b_un.b_info->cgx.cg_s.s_nbfree = nbfree;
		fst.summary_errors++;
		changes++;
	}

	if ( nbfree != bsum->b_un.b_sum->cg_sum[cgnr].s_nbfree )
	{				/* Update information in the	*/
					/* sum-block 			*/
		bsum->b_un.b_sum->cg_sum[cgnr].s_nbfree = nbfree;
		sum_changes++;
		fst.summary_errors++;
	}
	
#if DEBUG
IOdebug ("	tidy_update :	cg = %d , Directories  counted = %d | found = %d",
	 cgnr, n_dirs[cgnr], bp->b_un.b_info->cgx.cg_s.s_ndir );
#endif
					/* A different total number of	*/
					/* directories found ?		*/
	if ( n_dirs[cgnr] != bp->b_un.b_info->cgx.cg_s.s_ndir )
	{
		bp->b_un.b_info->cgx.cg_s.s_ndir = n_dirs[cgnr];
		changes++;
		fst.summary_errors++;
	}
	
	if ( n_dirs[cgnr] != bsum->b_un.b_sum->cg_sum[cgnr].s_ndir )
	{
		bsum->b_un.b_sum->cg_sum[cgnr].s_ndir = n_dirs[cgnr];
		sum_changes++;
		fst.summary_errors++;
	}
	
	if ( changes )			/* Any changes made ?		*/
	{
#if DEBUG
IOdebug ("	tidy_update : Write modified info-block for cg = %d to disk",
	 cgnr );
#endif
		test_bwrite ( bp );
	}
	else				/* No differences found ...	*/
		brelse ( bp->b_tbp, TAIL );
		
	fs_nbfree += nbfree;		/* Increment tot. # of free	*/
					/* blocks available.		*/
	fs_ndir += n_dirs[cgnr];	/* .. and the number of dirs	*/
 } /* end <for (cgnr)> */
 
 if ( sum_changes || bsum->b_un.b_sum->sum_same != TRUE)  /* Any modification done to the	*/
 {							  /* summary block ?		*/
	bsum->b_un.b_sum->sum_same = TRUE;
  	test_bwrite ( bsum );		/* Update summary-block on disk */
 }
 else
	brelse ( bsum->b_tbp, TAIL );
 
 /*------------  Update the file-system summary informations  ----------*/
 
#if DEBUG
IOdebug ("	tidy_update :	Update the total file-system summary information");
#endif

 bsum = bread ( 0, 1, 1, SAVEA );	/* Get the summary-block	*/
 
#if DEBUG
IOdebug ("	tidy_update :	FS-summary : Free blocks  counted = %d | found = %d",
	 fs_nbfree, bsum->b_un.b_sum->fs_sum.s_nbfree );
#endif
 					/* A different total number of	*/
 					/* blocks found for the file-sys*/
 if ( fs_nbfree != bsum->b_un.b_sum->fs_sum.s_nbfree ||
      fs_ndir   != bsum->b_un.b_sum->fs_sum.s_ndir ) 
 {					/* Save the total number of free*/
 					/* blocks in the file-system	*/
 	bsum->b_un.b_sum->fs_sum.s_nbfree = fs_nbfree;
 					/* Correct also the incore-copy */
 					/* Save the total number of di-	*/
 					/* rectories in the file-system */
 	bsum->b_un.b_sum->fs_sum.s_ndir  = fs_ndir;
 					/* ...and correct the incore-cp */
 				
 	IOdebug ( "%sHave to update summary block on disk.", S_INFO );
 	test_bwrite ( bsum );		/* Write summary-block back	*/
 	fst.summary_errors++;
 }
 else					/* We can simply release the	*/
 	brelse ( bsum->b_tbp, TAIL );	/* block !			*/

 return TRUE;				/* All operations done with 	*/
 					/* success			*/
}


/*************************************************************************
 * UPDATE THE "INODE-TOTALS" (# blocks, spare, size) OF A CORRECTED
 * INODE
 *
 * - This procedure is called to guarantee consistence of an inode if
 *   block references are set to zero (f.e. invalid block numbers).
 * - It is called after the following checker operations:
 *	o Deletion of references to multiple allocated blocks
 * - The number of allocated blocks is counted for every entry
 * - If the entry is of Type_Directory, the number of entries (spare) and
 *   the size of the entry (spare * sizeof(struct dir_elem)) are also 
 *   examined and corrected, if there are any differences found.
 * - We don't need to validate block-numbers or the content of blocks,
 *   because all inodes handled by update_inode_totals() were validated
 *   before!
 * - Note : Because of the fact that the file-server is actually not
 *   able to seek over non allocated blocks, it is assumed that there
 *   are NO gaps in the allocation schema. Therefore this procedure
 *   makes additional efforts to copy block references, if there were
 *   any gaps detected. Such a gap is created for example, if we delete
 *   a reference of a multiple allocated block.
 *
 * Parameter : ip	= Pointer to the inode to be inspected
 * Return    : TRUE     : No differences are found and the inode is
 *			  left unchanged.
 *	       FALSE    : If we have made any corrections to
 *			  i_blocks, i_spare or i_size
 *
 *************************************************************************/
word
update_inode_totals ( struct inode *ip )
{
 word bcnt, ibcnt1, ibcnt2;
 word icnt, changes;
 word free_1, free_2;
 struct buf *bp, *bip1, *bip2;
 word blocks, spare;
 
 blocks = 0;				/* First time initialization	*/
 spare  = 0;				/* (only for "Type_Directory")	*/
 changes= 0;
 
/*------------------------  The direct blocks  -------------------------*/

#if DEBUG
IOdebug ("	update_inode_totals :	Correct inode totals for direct blocks");
#endif

 for ( bcnt = 0 , free_1 = -1 ; bcnt < i_fs.fs_ndaddr ; bcnt++ )
 {
 	if ( ! ip->i_db[bcnt] )		/* A zeroed reference has to be */
 	{				/* left out.			*/
 		if ( free_1 == -1 )	/* Note the first free slot, we	*/
 			free_1 = bcnt;	/* have found.			*/
 		continue;
	}
 		
 	blocks++;			/* Another valid reference	*/
 					/* Find out the number of clai-	*/
 					/* med slots for this inode:	*/
 	if ( ip->i_mode == Type_Directory )
 	{				/* Read the direct block into   */
 					/* memory.			*/
 		bp = bread ( 0, ip->i_db[bcnt], 1, SAVEA );		

 					/* Step through the inodes	*/
 		for ( icnt = 0 ; icnt < i_fs.fs_maxdpb ; icnt++ )
			if ( bp->b_un.b_dir[icnt].de_inode.i_mode !=
			     FREE )
			     	spare++;
			     		/* Release the directory-block	*/
		brelse ( bp->b_tbp, TAIL );
 	}
 					/* Is there any free slot avai-	*/
 	if ( free_1 != -1 )		/* lable ?			*/
 	{				/* Then we have to copy the 	*/
 					/* actual reference to this slot*/
 		ip->i_db[free_1] = ip->i_db[bcnt];
 		ip->i_db[bcnt] = 0;
 		free_1++;
 	}
 }
 
/*------------------  The single indirect blocks  ----------------------*/

 if ( ip->i_ib[0] )
 {
#if DEBUG
IOdebug ("	update_inode_totals :	Correct inode totals for single indirect blocks");
#endif
 	blocks++;			/* One more claimed block	*/
 					/* Read the indirect block	*/
 	bip1 = bread ( 0, ip->i_ib[0], 1, SAVEA );

 					/* Handle all references	*/
 	for ( ibcnt1 = 0 , free_1 = -1 ; ibcnt1 < i_fs.fs_maxcontig ; ibcnt1++ )
 	{				/* Zeored entries are skipped	*/
 		if ( ! bip1->b_un.b_daddr[ibcnt1] )
 		{			/* Note the first empty slot!	*/
 			if ( free_1 == -1 )
 				free_1 = ibcnt1;
 					/* ... and go on directly	*/
 			continue;
 		}
 			
 		blocks++;		/* One more block to be noted	*/
 					/* Find out the number of clai-	*/
 					/* med slots for this inode:	*/
	 	if ( ip->i_mode == Type_Directory )
 		{			/* Read the direct block into   */
 					/* memory.			*/
 			bp = bread ( 0, bip1->b_un.b_daddr[ibcnt1], 1, SAVEA );

 					/* Step through the inodes	*/
 			for ( icnt = 0 ; icnt < i_fs.fs_maxdpb ; icnt++ )
				if ( bp->b_un.b_dir[icnt].de_inode.i_mode !=
				     FREE )
			     		spare++;
			     		/* Release the directory-block	*/
			brelse ( bp->b_tbp, TAIL );
 		}
 					/* Is there an empty slot to	*/
 					/* which the actual reference	*/
 					/* should be copied ?		*/
 		if ( free_1 != -1 )
 		{			/* Copy the actual reference !	*/
 			bip1->b_un.b_daddr[free_1] = bip1->b_un.b_daddr[ibcnt1];
 			bip1->b_un.b_daddr[ibcnt1] = 0;
 					/* The next free slot available */
 			free_1++;
 		}
 	}
 	
 	brelse ( bip1->b_tbp, TAIL );
 }

/*------------------  The double indirect blocks  ----------------------*/

 if ( ip->i_ib[1] )
 {
#if DEBUG
IOdebug ("	update_inode_totals :	Correct inode totals for double indirect blocks");
#endif
 	blocks++;			/* One more claimed block	*/
 					/* Read the indirect block	*/
 	bip2 = bread ( 0, ip->i_ib[1], 1, SAVEA );

					/* Scan the block numbers re -	*/
					/* ferred in the indirect block.*/ 	
 	for ( ibcnt2 = 0 , free_2 = -1 ; ibcnt2 < i_fs.fs_maxcontig ; ibcnt2++ )
 	{				/* Skip over zeroed directory -	*/
 					/* entries			*/
 		if ( ! bip2->b_un.b_daddr[ibcnt2] )
 		{
 			if ( free_2 == -1 )
 				free_2 = ibcnt2;
 			continue;
 		}
 
		blocks++;		/* Again one more block to be 	*/
					/* noted.			*/
					/* Read the indirect block	*/
 		bip1 = bread ( 0, bip2->b_un.b_daddr[ibcnt2], 1, SAVEA );

 					/* Handle all references	*/
 		for ( ibcnt1 = 0 , free_1 = -1 ; ibcnt1 < i_fs.fs_maxcontig ; 
 		      ibcnt1++ )
 		{			/* A zeroed entry ?		*/
 			if ( ! bip1->b_un.b_daddr[ibcnt1] )
 			{
 				if ( free_1 == -1 )
 					free_1 = ibcnt1;
 				continue;
 			}
 			
 			blocks++;	/* One more block to be noted	*/
 					/* Find out the number of clai-	*/
 					/* med slots for this inode:	*/
	 		if ( ip->i_mode == Type_Directory )
 			{		/* Read the direct block into   */
 					/* memory.			*/
 				bp = bread ( 0, bip1->b_un.b_daddr[ibcnt1], 1, SAVEA );

 					/* Step through the inodes	*/
 				for ( icnt = 0 ; icnt < i_fs.fs_maxdpb ; icnt++ )
					if ( bp->b_un.b_dir[icnt].de_inode.i_mode !=
					     FREE )
				     		spare++;
			     		/* Release the directory-block	*/
				brelse ( bp->b_tbp, TAIL );
 			}
 					
 			if ( free_1 != -1 )
 			{		/* Copy actual reference to the	*/
 					/* free slot.			*/
 				bip1->b_un.b_daddr[free_1] = bip1->b_un.b_daddr[ibcnt1];
 				bip1->b_un.b_daddr[ibcnt1] = 0;
 				free_1++;
 			}
 		} /* end <for (ibcnt1)> */ 
 					/* Release the indirect block	*/
 		brelse ( bip1->b_tbp, TAIL );
 		
 		if ( free_2 != -1 )
 		{			/* Copy actual reference to the	*/
 					/* available free slot.		*/
 			bip2->b_un.b_daddr[free_2] = bip2->b_un.b_daddr[ibcnt2];
 			bip2->b_un.b_daddr[ibcnt2] = 0;
 			free_2++;
 		}
	} /* end <for (ibcnt2)> */
	brelse ( bip2->b_tbp, TAIL );
 }

/*------------------  The results of our inspection  -------------------*/

 if ( ip->i_blocks != blocks )		/* Different number of blocks	*/
 {					/* found:			*/
#if DEBUG
IOdebug ("	update_inode_totals :	# of allocated blocks found= %d | counted= %d",
	 ip->i_blocks, blocks );
#endif
 	changes++;
 	ip->i_blocks = blocks;
 }
					/* Further checks, if the entry	*/
					/* is of Type_Directory		*/
 if ( ip->i_mode == Type_Directory )
 {					/* Different number of directo-	*/
 	if ( ip->i_spare != spare )	/* ry entries ?			*/
 	{
#if DEBUG
IOdebug ("	update_inode_totals :	# of directory-entries found= %d | counted= %d",
	 ip->i_spare, spare );
#endif
 		changes++;
 		ip->i_spare = spare;
 	}
 					/* Wrong size measured in the	*/
 					/* inode ?			*/
 	if ( ip->i_size != spare * sizeof (struct dir_elem) )
 	{
#if DEBUG
IOdebug ("	update_inode_totals :	Size of the dir-entry found= %d | calculated= %d",
	 ip->i_size, spare * sizeof (struct dir_elem) );
#endif
 		changes++;
 		ip->i_size = spare * sizeof (struct dir_elem);
 	}
 } 
 
 if ( ! changes )			/* Any differences found ?	*/
 	return TRUE;			/* No changes made !!		*/
 else
 	return FALSE;			/* We have made corrections	*/
}

/*-----------------------------------------------------------------------*/

/* end of tidyup.c */
