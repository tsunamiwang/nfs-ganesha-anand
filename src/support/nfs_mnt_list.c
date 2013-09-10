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
 * \file    nfs_mnt_list.c
 * \author  $Author: deniel $
 * \date    $Date: 2005/11/28 17:03:03 $
 * \version $Revision: 1.7 $
 * \brief   routines for managing the mount list.
 *
 * nfs_mnt_list.c : routines for managing the mount list.
 *
 * $Header: /cea/home/cvs/cvs/SHERPA/BaseCvs/GANESHA/src/support/nfs_mnt_list.c,v 1.7 2005/11/28 17:03:03 deniel Exp $
 *
 * $Log: nfs_mnt_list.c,v $
 *
 * Revision 1.6  2005/11/24 13:44:29  deniel
 * ajout du remove dans ipname
 * track list in nfs_mnt_listc.
 *
 * Revision 1.5  2005/10/07 09:33:22  leibovic
 * Correcting bug in nfs_Add_MountList_Entry function
 * ( pnew_mnt_list_entry->ml_next not initialized ).
 *
 * Revision 1.4  2005/10/05 14:03:31  deniel
 * DEBUG ifdef are now much cleaner
 *
 * Revision 1.3  2005/08/11 12:37:28  deniel
 * Added statistics management
 *
 * Revision 1.2  2005/08/03 13:13:59  deniel
 * memset to zero before building the filehandles
 *
 * Revision 1.1  2005/08/03 06:57:54  deniel
 * Added a libsupport for miscellaneous service functions
 *
 * Revision 1.1  2005/07/20 11:49:47  deniel
 * Mount protocol V1/V3 support almost complete
 *
 *
 */
#include "config.h"
#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>              /* for having isalnum */
#include <stdlib.h>             /* for having atoi */
#include <dirent.h>             /* for having MAXNAMLEN */
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/file.h>           /* for having FNDELAY */
#include <pwd.h>
#include <grp.h>
#include "log.h"
#include "ganesha_rpc.h"
#include "nfs_core.h"
#include "nfs23.h"
#include "nfs4.h"
#include "fsal.h"
#include "nfs_tools.h"
#include "nfs_exports.h"
#include "nfs_file_handle.h"

/* The server's mount list (we use the native mount v3 structure) */
mountlist MNT_List_head = NULL;
mountlist MNT_List_tail = NULL;

/**
 *
 * nfs_Add_MountList_Entry: Adds a client to the mount list.
 *
 * Adds a client to the mount list.
 *
 * @param hostname [IN] the hostname for the client
 * @param dirpath [IN] the mounted path 
 *
 * @return 1 if successful, 0 otherwise
 *
 */
int nfs_Add_MountList_Entry(char *hostname, char *dirpath)
{
#ifndef _NO_MOUNT_LIST
  mountlist pnew_mnt_list_entry;
#endif

  /* Sanity check */
  if(hostname == NULL || dirpath == NULL)
    return 0;

#ifndef _NO_MOUNT_LIST

  /* Allocate the new entry */
  if((pnew_mnt_list_entry = gsh_calloc(1, sizeof(struct mountbody))) == NULL)
    return 0;

  if((pnew_mnt_list_entry->ml_hostname
      = gsh_calloc(1, MAXHOSTNAMELEN + 1)) == NULL)
    {
      gsh_free(pnew_mnt_list_entry);
      return 0;
    }

  if((pnew_mnt_list_entry->ml_directory
      = gsh_calloc(1, MAXPATHLEN + 1)) == NULL)
    {
      gsh_free(pnew_mnt_list_entry->ml_hostname);
      gsh_free(pnew_mnt_list_entry);
      return 0;
    }
  /* Copy the data */
  if (strmaxcpy(pnew_mnt_list_entry->ml_hostname, hostname,
		MAXHOSTNAMELEN) == -1)
    {
      gsh_free(pnew_mnt_list_entry->ml_directory);
      gsh_free(pnew_mnt_list_entry->ml_hostname);
      gsh_free(pnew_mnt_list_entry);
      return 0;
    }
  if (strmaxcpy(pnew_mnt_list_entry->ml_directory, dirpath,
		MAXPATHLEN) == -1)
    {
      gsh_free(pnew_mnt_list_entry->ml_directory);
      gsh_free(pnew_mnt_list_entry->ml_hostname);
      gsh_free(pnew_mnt_list_entry);
      return 0;
    }

  /* initialize next pointer */
  pnew_mnt_list_entry->ml_next = NULL;

  /* This should occur only for the first mount */
  if(MNT_List_head == NULL)
    {
      MNT_List_head = pnew_mnt_list_entry;
    }

  /* Append to the tail of the list */
  if(MNT_List_tail == NULL)
    MNT_List_tail = pnew_mnt_list_entry;
  else
    {
      MNT_List_tail->ml_next = pnew_mnt_list_entry;
      MNT_List_tail = pnew_mnt_list_entry;
    }

  if(isFullDebug(COMPONENT_NFSPROTO))
    nfs_Print_MountList();

#endif

  return 1;
}

/**
 *
 * @brief Remove a client from the mount list
 *
 * @param[in] hostname The hostname for the client
 * @param[in] dirpath  The mounted path
 *
 * @return true if successful, false otherwise
 */
bool nfs_Remove_MountList_Entry(char *hostname, char *dirpath)
{
#ifndef _NO_MOUNT_LIST
  mountlist piter_mnt_list_entry;
  mountlist piter_mnt_list_entry_prev;
  int found = 0;
#endif

  if(hostname == NULL)
    return 0;

#ifndef _NO_MOUNT_LIST

  piter_mnt_list_entry_prev = NULL;

  for(piter_mnt_list_entry = MNT_List_head;
      piter_mnt_list_entry != NULL; piter_mnt_list_entry = piter_mnt_list_entry->ml_next)
    {
      /* BUGAZOMEU: pas de verif sur le path */
      if(!strncmp(piter_mnt_list_entry->ml_hostname, hostname, MAXHOSTNAMELEN)
         /*  && !strncmp( piter_mnt_list_entry->ml_directory, dirpath, MAXPATHLEN ) */ )
        {
          found = 1;
          break;
        }

      piter_mnt_list_entry_prev = piter_mnt_list_entry;

    }

  if(found)
    {
      /* remove head item ? */
      if(piter_mnt_list_entry_prev == NULL)
        MNT_List_head = MNT_List_head->ml_next;
      else
        piter_mnt_list_entry_prev->ml_next = piter_mnt_list_entry->ml_next;

      /* remove tail item ? */
      if(MNT_List_tail == piter_mnt_list_entry)
        MNT_List_tail = piter_mnt_list_entry_prev;

      gsh_free(piter_mnt_list_entry->ml_hostname);
      gsh_free(piter_mnt_list_entry->ml_directory);
      gsh_free(piter_mnt_list_entry);

    }

  if(isFullDebug(COMPONENT_NFSPROTO))
    nfs_Print_MountList();

#endif

  return 1;
}                               /* nfs_Remove_MountList_Entry */

/**
 * @brief Purges the whole mount list
 *
 * @return true if successful, false otherwise
 */
bool nfs_Purge_MountList(void)
{
  mountlist piter_mnt_list_entry __attribute__((unused)),
    piter_mnt_list_entry_next __attribute__((unused));

  piter_mnt_list_entry = MNT_List_head;
  piter_mnt_list_entry_next = MNT_List_head;

#ifndef _NO_MOUNT_LIST

  while(piter_mnt_list_entry_next != NULL)
    {
      piter_mnt_list_entry_next = piter_mnt_list_entry->ml_next;
      gsh_free(piter_mnt_list_entry->ml_hostname);
      gsh_free(piter_mnt_list_entry->ml_directory);
      gsh_free(piter_mnt_list_entry);
      piter_mnt_list_entry = piter_mnt_list_entry_next;
    }

  MNT_List_head = NULL;
  MNT_List_tail = NULL;

  if(isFullDebug(COMPONENT_NFSPROTO))
    nfs_Print_MountList();

#endif

  return 1;
}                               /* nfs_Purge_MountList */

/**
 * @brief Initializes the mount list
 *
 * @return 1 if successful, 0 otherwise
 *
 */
int nfs_Init_MountList(void)
{
  MNT_List_head = NULL;
  MNT_List_tail = NULL;

  if(isFullDebug(COMPONENT_NFSPROTO))
    nfs_Print_MountList();

  return 1;
}                               /* nfs_Init_MountList */

/**
 * @brief Returns the mount list
 *
 * @return The mount list
 */
mountlist nfs_Get_MountList(void)
{
  if(isFullDebug(COMPONENT_NFSPROTO))
    nfs_Print_MountList();

  return MNT_List_head;
}

/**
 * @brief Print the mount list
 */

void nfs_Print_MountList(void)
{
  mountlist piter_mnt_list_entry = NULL;

  if(MNT_List_head == NULL)
    LogFullDebug(COMPONENT_NFSPROTO, "Mount List Entry is empty");

  for(piter_mnt_list_entry = MNT_List_head;
      piter_mnt_list_entry != NULL; piter_mnt_list_entry = piter_mnt_list_entry->ml_next)
    LogFullDebug(COMPONENT_NFSPROTO,
                 "Mount List Entry : ml_hostname=%s   ml_directory=%s",
                 piter_mnt_list_entry->ml_hostname,
                 piter_mnt_list_entry->ml_directory);

  return;
}                               /* nfs_Print_MountList */
