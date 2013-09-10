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
 * @file    nfs4_cb_getattr.c
 * @brief   Routines used for managing the NFS4_CB_GETATTR operation
 *
 * Routines used for managing the NFS4_CB_GETATTR operation
 *
 *
 */
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/file.h>           /* for having FNDELAY */
#include "HashTable.h"
#include "log.h"
#include "ganesha_rpc.h"
#include "nfs4.h"
#include "nfs_core.h"
#include "cache_inode.h"
#include "nfs_exports.h"
#include "nfs_creds.h"
#include "nfs_proto_functions.h"
#include "nfs_proto_tools.h"
#include "nfs_tools.h"
#include "nfs_file_handle.h"

/**
 * @brief NFS4_OP_CB_getattr, nfsv4 call back to get files attributes
 *
 * This function implements the NFS4_OP_CB_GETATTR operation that
 * calls back to get files attributes.
 *
 * @param[in]     op   Arguments for nfs4_cb
 * @param[in,out] data Compound request's data
 * @param[out]    resp Results for nfs4_cb
 *
 * @return NFS4_OK if successfull, other values show an error.
 *
 */

int nfs4_cb_getattr(struct nfs_cb_argop4 *op,
                    compound_data_t *data,
                    struct nfs_cb_resop4 *resp)
{
  return NFS4_OK;
} /* nfs4_cb_getattr */
