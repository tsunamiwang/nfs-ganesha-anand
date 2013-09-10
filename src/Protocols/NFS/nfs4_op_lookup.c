/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright CEA/DAM/DIF  (2008)
 * contributeur : Philippe DENIEL   philippe.deniel@cea.fr
 *                Thomas LEIBOVICI  thomas.leibovici@cea.fr
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 * ---------------------------------------
 */

/**
 * @file    nfs4_op_lookup.c
 * @brief   Routines used for managing the NFS4 COMPOUND functions.
 *
 * Routines used for managing the NFS4 COMPOUND functions.
 *
 *
 */
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include "HashTable.h"
#include "log.h"
#include "ganesha_rpc.h"
#include "nfs4.h"
#include "nfs_core.h"
#include "cache_inode.h"
#include "nfs_exports.h"
#include "nfs_creds.h"
#include "nfs_proto_functions.h"
#include "nfs_tools.h"
#include "nfs_proto_tools.h"

/**
 * @brief NFS4_OP_LOOKUP
 *
 * This function implments the NFS4_OP_LOOKUP operation, which looks
 * a filename up in the FSAL.
 *
 * @param[in]     op   Arguments for nfs4_op
 * @param[in,out] data Compound request's data
 * @param[out]    resp Results for nfs4_op
 *
 * @return per RFC5661, pp. 368-9
 *
 */

int
nfs4_op_lookup(struct nfs_argop4 *op,
               compound_data_t *data,
               struct nfs_resop4 *resp)
{
        /* Convenient alias for the arguments */
        LOOKUP4args    *const  arg_LOOKUP4 = &op->nfs_argop4_u.oplookup;
        /* Convenient alias for the response  */
        LOOKUP4res     *const  res_LOOKUP4 = &resp->nfs_resop4_u.oplookup;
        /* The name to look up */
        char                 * name = NULL;
        /* The directory in which to look up the name */
        cache_entry_t        * dir_entry = NULL;
        /* The name found */
        cache_entry_t        * file_entry = NULL;
        /* Status code from Cache inode */
        cache_inode_status_t   cache_status = CACHE_INODE_SUCCESS;

        resp->resop = NFS4_OP_LOOKUP;
        res_LOOKUP4->status = NFS4_OK;

        /* Do basic checks on a filehandle */
        res_LOOKUP4->status = nfs4_sanity_check_FH(data, DIRECTORY,
                                                   false);
        if (res_LOOKUP4->status != NFS4_OK) {
		/* for some reason lookup is picky.  Just not being
		 * dir is not enough.  We want to know it is a symlink
		 */
		if(res_LOOKUP4->status == NFS4ERR_NOTDIR
		   && data->current_filetype == SYMBOLIC_LINK)
			res_LOOKUP4->status = NFS4ERR_SYMLINK;
                goto out;
        }

        /* Validate and convert the UFT8 objname to a regular string */
        res_LOOKUP4->status = nfs4_utf8string2dynamic(&arg_LOOKUP4->objname,
						      UTF8_SCAN_ALL,
						      &name);
	if (res_LOOKUP4->status  != NFS4_OK) {
		goto out;
	}

        /* If Filehandle points to a pseudo fs entry, manage it via pseudofs specific functions */
        if (nfs4_Is_Fh_Pseudo(&(data->currentFH))) {
                res_LOOKUP4->status = nfs4_op_lookup_pseudo(op, data, resp);
                goto out;
        }

        /* Do the lookup in the FSAL */
        file_entry = NULL;
        dir_entry = data->current_entry;

        /* Sanity check: dir_entry should be ACTUALLY a directory */

        cache_status = cache_inode_lookup(dir_entry,
					  name,
					  data->req_ctx,
					  &file_entry);
        if (cache_status != CACHE_INODE_SUCCESS) {
                res_LOOKUP4->status = nfs4_Errno(cache_status);
                goto out;
        }

        /* Convert it to a file handle */
        if (!nfs4_FSALToFhandle(&data->currentFH,
                                file_entry->obj_handle)) {
                res_LOOKUP4->status = NFS4ERR_SERVERFAULT;
                cache_inode_put(file_entry);
                goto out;
        }

        /* Release dir_entry, as it is not reachable from anywhere in
           compound after this function returns.  Count on later
           operations or nfs4_Compound to clean up current_entry. */

        if (dir_entry) {
                cache_inode_put(dir_entry);
        }

        /* Keep the pointer within the compound data */
        data->current_entry = file_entry;
        data->current_filetype = file_entry->type;

        /* Return successfully */
        res_LOOKUP4->status = NFS4_OK;

out:
        if (name) {
                gsh_free(name);
                name = NULL;
        }
        return res_LOOKUP4->status;
}                               /* nfs4_op_lookup */

/**
 * @brief Free memory allocated for LOOKUP result
 *
 * This function frees any memory allocated for the result of the
 * NFS4_OP_LOOKUP operation.
 *
 * @param[in,out] resp nfs4_op results
 */
void nfs4_op_lookup_Free(nfs_resop4 *resp)
{
  /* Nothing to be done */
        return;
} /* nfs4_op_lookup_Free */
