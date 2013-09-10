/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright CEA/DAM/DIF  (2008)
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
 * \file    mnt_Export.c
 * \date    $Date: 2006/01/18 17:01:40 $
 * \version $Revision: 1.19 $
 * \brief   MOUNTPROC_EXPORT for Mount protocol v1 and v3.
 *
 * mnt_Null.c : MOUNTPROC_EXPORT in V1, V3.
 *
 * Exporting client hosts and networks OK.
 *
 */
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/file.h>           /* for having FNDELAY */
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>

#include "HashTable.h"
#include "log.h"
#include "nfs23.h"
#include "nfs4.h"
#include "nfs_core.h"
#include "cache_inode.h"
#include "nfs_exports.h"
#include "nfs_creds.h"
#include "nfs_tools.h"
#include "mount.h"
#include "nfs_proto_functions.h"
#include "export_mgr.h"

struct proc_state {
	exports head;
	exports tail;
	int retval;
};

static bool proc_export(struct gsh_export *exp,
			void *arg)
{
	struct proc_state *state = (struct proc_state *)arg;
	exportlist_t *export = &exp->export;
	struct exportnode *new_expnode;
	struct glist_head *glist_item;
	exportlist_client_entry_t *client;
	struct groupnode *group, *grp_tail;
	const char *grp_name;
	char addr_buf[INET6_ADDRSTRLEN + 1];

	state->retval = 0;
	new_expnode = gsh_calloc(1, sizeof(struct exportnode));
	if(new_expnode == NULL)
		goto nomem;
	new_expnode->ex_dir = gsh_strdup(export->fullpath);
	if(new_expnode->ex_dir == NULL)
		goto nomem;
	glist_for_each(glist_item, &export->clients.client_list) {
		client = glist_entry(glist_item, exportlist_client_entry_t, cle_list);
		group = gsh_calloc(1, sizeof(struct groupnode));
		if(group == NULL)
			goto nomem;
		if(new_expnode->ex_groups == NULL) {
			new_expnode->ex_groups = group;
		} else {
			grp_tail->gr_next = group;
		}
		grp_tail = group;
		switch(client->type) {
		case HOSTIF_CLIENT:
			grp_name = inet_ntop(AF_INET,
					     &client->client.hostif.clientaddr,
					     addr_buf,
					     INET6_ADDRSTRLEN);
			if(grp_name == NULL) {
				state->retval = errno;
				grp_name = "Invalid Host Address";
			}
			break;
                case NETWORK_CLIENT:
			grp_name = inet_ntop(AF_INET,
					     &client->client.network.netaddr,
					     addr_buf,
					     INET6_ADDRSTRLEN);
			if(grp_name == NULL) {
				state->retval = errno;
				grp_name = "Invalid Network Address";
			}
			break;
                case NETGROUP_CLIENT:
			grp_name = client->client.netgroup.netgroupname;
                case GSSPRINCIPAL_CLIENT:
			grp_name = client->client.gssprinc.princname;
                case MATCH_ANY_CLIENT:
			grp_name = "*";
                default:
			grp_name = "<unknown>";
		}
		group->gr_name = gsh_strdup(grp_name);
		if(group->gr_name == NULL)
			goto nomem;
	}
	if(state->head == NULL) {
		state->head = new_expnode;
	} else {
		state->tail->ex_next = new_expnode;
	}
	state->tail = new_expnode;
	return true;

nomem:
	state->retval = errno;
	return false;
}

/**
 * @brief The Mount proc EXPORT function, for all versions.
 *
 * Return a list of all exports and their allowed clients/groups/networks.
 *
 * @param[in]  parg     Ignored
 * @param[in]  pexport  The export list to be return to the client.
 * @param[in]  req_ctx  Ignored
 * @param[in]  pworker  Ignored
 * @param[in]  preq     Ignored
 * @param[out] pres     Pointer to the export list
 *
 */

int mnt_Export(nfs_arg_t *parg,
               exportlist_t *pexport,
	       struct req_op_context *req_ctx,
               nfs_worker_data_t *pworker,
               struct svc_req *preq,
               nfs_res_t * pres)
{
	struct proc_state proc_state;

	/* init everything of interest to good state. */
	memset(pres, 0, sizeof(nfs_res_t));
	memset(&proc_state, 0, sizeof(proc_state));

	(void)foreach_gsh_export(proc_export, &proc_state);
	if(proc_state.retval != 0) {
		LogCrit(COMPONENT_NFSPROTO,
			"Processing exports failed. error = \"%s\" (%d)",
			strerror(proc_state.retval),
			proc_state.retval);
	}
	pres->res_mntexport = proc_state.head;
	return NFS_REQ_OK;
}                               /* mnt_Export */

/**
 * mnt_Export_Free: Frees the result structure allocated for mnt_Export.
 * 
 * Frees the result structure allocated for mnt_Dump.
 * 
 * @param pres        [INOUT]   Pointer to the result structure.
 *
 */
void mnt_Export_Free(nfs_res_t * pres)
{
	struct exportnode *exp, *next_exp;
	struct groupnode *grp, *next_grp;

	exp = pres->res_mntexport;
	while(exp != NULL) {
		next_exp = exp->ex_next;
		grp = exp->ex_groups;
		while(grp != NULL) {
			next_grp = grp->gr_next;
			if(grp->gr_name != NULL)
				gsh_free(grp->gr_name);
			gsh_free(grp);
			grp = next_grp;
		}
		gsh_free(exp);
		exp = next_exp;
	}
}                               /* mnt_Export_Free */
