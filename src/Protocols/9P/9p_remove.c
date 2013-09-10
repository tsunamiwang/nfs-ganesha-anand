/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright CEA/DAM/DIF  (2011)
 * contributeur : Philippe DENIEL   philippe.deniel@cea.fr
 *                Thomas LEIBOVICI  thomas.leibovici@cea.fr
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * ---------------------------------------
 */

/**
 * \file    9p_remove.c
 * \brief   9P version
 *
 * 9p_remove.c : _9P_interpretor, request ATTACH
 *
 *
 */

#include "config.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "nfs_core.h"
#include "log.h"
#include "cache_inode.h"
#include "fsal.h"
#include "9p.h"


#define FREE_FID(pfid, fid, preq9p) do {        \
/* Tell the cache the fid that used this        \
 * entry is not used by this set of messages */ \
  cache_inode_put( pfid->pentry ) ;             \
                                                \
  /* Free the fid */                            \
  gsh_free( pfid ) ;                            \
  preq9p->pconn->fids[*fid] = NULL ;            \
} while( 0 )


int _9p_remove( _9p_request_data_t * preq9p, 
                void  * pworker_data,
                u32 * plenout, 
                char * preply)
{
  char * cursor = preq9p->_9pmsg + _9P_HDR_SIZE + _9P_TYPE_SIZE ;

  u16 * msgtag = NULL ;
  u32 * fid    = NULL ;
  _9p_fid_t * pfid = NULL ;
  cache_inode_status_t  cache_status ;


  if ( !preq9p || !pworker_data || !plenout || !preply )
   return -1 ;

  /* Get data */
  _9p_getptr( cursor, msgtag, u16 ) ; 
  _9p_getptr( cursor, fid,    u32 ) ; 

  LogDebug( COMPONENT_9P, "TREMOVE: tag=%u fid=%u", (u32)*msgtag, *fid ) ;

  if( *fid >= _9P_FID_PER_CONN )
    return  _9p_rerror( preq9p, pworker_data,  msgtag, ERANGE, plenout, preply ) ;

  pfid = preq9p->pconn->fids[*fid] ;

  /* Check that it is a valid fid */
  if (pfid == NULL || pfid->pentry == NULL) 
  {
    LogDebug( COMPONENT_9P, "request on invalid fid=%u", *fid ) ;
    return  _9p_rerror( preq9p, pworker_data,  msgtag, EIO, plenout, preply ) ;
  }

  cache_status = cache_inode_remove(pfid->ppentry,
				    pfid->name,
				    &pfid->op_context);
  if(cache_status != CACHE_INODE_SUCCESS)
    return  _9p_rerror( preq9p, pworker_data,  msgtag, _9p_tools_errno( cache_status ), plenout, preply ) ;

  /* If object is an opened file, close it */
  if( ( pfid->pentry->type == REGULAR_FILE ) &&
      is_open( pfid->pentry ) )
   {
     if( pfid->opens )
      {
        cache_inode_dec_pin_ref(pfid->pentry, FALSE);
        pfid->opens = 0; /* dead */

        /* Under this flag, pin ref is still checked */
        cache_status = cache_inode_close(pfid->pentry,
				      CACHE_INODE_FLAG_REALLYCLOSE);
        if(cache_status != CACHE_INODE_SUCCESS)
        {
           FREE_FID(pfid, fid, preq9p);
           return  _9p_rerror( preq9p, pworker_data,  msgtag, _9p_tools_errno( cache_status ), plenout, preply ) ;
        }
      }
   }

  /* Clean the fid */
  FREE_FID(pfid, fid, preq9p);

  /* Build the reply */
  _9p_setinitptr( cursor, preply, _9P_RREMOVE ) ;
  _9p_setptr( cursor, msgtag, u16 ) ;

  _9p_setendptr( cursor, preply ) ;
  _9p_checkbound( cursor, preply, plenout ) ;

  LogDebug( COMPONENT_9P, "TREMOVE: tag=%u fid=%u",
            (u32)*msgtag, *fid ) ;

  // _9p_stat_update( *pmsgtype, TRUE, &pwkrdata->stats._9p_stat_req ) ;
  return 1 ;

}

