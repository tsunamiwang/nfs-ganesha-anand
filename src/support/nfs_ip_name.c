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
 * \file    nfs_ip_name.c
 * \date    $Date: 2006/01/20 07:39:23 $
 * \version $Revision: 1.6 $
 * \brief   The management of the IP/name cache.
 *
 * nfs_ip_name.c : The management of the IP/name cache.
 *
 */
#include "config.h"
#include "HashTable.h"
#include "log.h"
#include "nfs_core.h"
#include "nfs_exports.h"
#include "config_parsing.h"
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Hashtable used to cache the hostname, accessed by their IP addess */
hash_table_t *ht_ip_name;
unsigned int expiration_time;

/**
 * @name Compute the hash value for the entry in IP/name cache
 *
 * @param[in] hparam    Hash table parameter.
 * @param[in] buffcleff The hash key buffer
 *
 * @return the computed hash value.
 *
 * @see HashTable_Init
 *
 */
uint32_t ip_name_value_hash_func(hash_parameter_t *hparam,
				 struct gsh_buffdesc *buffclef)
{
  return hash_sockaddr(buffclef->addr, IGNORE_PORT) % hparam->index_size;
}

/**
 * @brief Compute the rbt value for the entry in IP/name cache
 *
 * @param[in] hparam   Hash table parameter
 * @param[in] buffclef Hash key buffer
 *
 * @return the computed rbt value.
 *
 * @see HashTable_Init
 *
 */
uint64_t ip_name_rbt_hash_func(hash_parameter_t *hparam,
			       struct gsh_buffdesc *buffclef)
{
  return hash_sockaddr(buffclef->addr, IGNORE_PORT);
}

/**
 *
 * compare_ip_name: compares the ip address stored in the key buffers.
 *
 * compare the ip address stored in the key buffers. This function is to be used as 'compare_key' field in 
 * the hashtable storing the nfs duplicated requests. 
 *
 * @param buff1 [IN] first key
 * @param buff2 [IN] second key
 *
 * @return 0 if keys are identifical, 1 if they are different. 
 *
 */
int compare_ip_name(struct gsh_buffdesc * buff1, struct gsh_buffdesc * buff2)
{
  return (cmp_sockaddr(buff1->addr, buff2->addr, IGNORE_PORT) != 0) ? 0 : 1;
}

/**
 * @brief Display the ip_name stored in the buffer
 *
 * @param[in]  pbuff Buffer to display
 * @param[out] str   Output string
 *
 * @return number of character written.
 */
int display_ip_name_key(struct gsh_buffdesc * pbuff, char *str)
{
  sockaddr_t *addr = (sockaddr_t *)(pbuff->addr);

  sprint_sockaddr(addr, str, HASHTABLE_DISPLAY_STRLEN);
  return strlen(str);
}

/**
 * @brief Display the ip_name stored in the buffer
 *
 * @param[in]  pbuff Buffer to display
 * @param[out] str   Output string
 *
 * @return number of character written
 */
int display_ip_name_val(struct gsh_buffdesc * pbuff, char *str)
{
  nfs_ip_name_t *nfs_ip_name = (pbuff->addr);

  return snprintf(str, HASHTABLE_DISPLAY_STRLEN, "%s", nfs_ip_name->hostname);
}

/**
 *
 * nfs_ip_name_add: adds an entry in the duplicate requests cache.
 *
 * Adds an entry in the duplicate requests cache.
 *
 * @param ipaddr           [IN]    the ipaddr to be used as key
 * @param hostname         [IN]    the hostname added (found by using gethostbyaddr)
 *
 * @return IP_NAME_SUCCESS if successfull\n.
 * @return IP_NAME_INSERT_MALLOC_ERROR if an error occured during the insertion process \n
 * @return IP_NAME_NETDB_ERROR if an error occured during the netdb query (via gethostbyaddr).
 *
 */

int nfs_ip_name_add(sockaddr_t *ipaddr, char *hostname, size_t size)
{
  struct gsh_buffdesc buffkey;
  struct gsh_buffdesc buffdata;
  nfs_ip_name_t *nfs_ip_name = NULL;
  sockaddr_t *pipaddr = NULL;
  struct timeval tv0, tv1, dur;
  int rc;
  char ipstring[SOCK_NAME_MAX + 1];

  nfs_ip_name = gsh_malloc(sizeof(nfs_ip_name_t));

  if(nfs_ip_name == NULL)
    return IP_NAME_INSERT_MALLOC_ERROR;

  pipaddr = gsh_malloc(sizeof(sockaddr_t));
  if(pipaddr == NULL)
    {
      gsh_free(nfs_ip_name);
      return IP_NAME_INSERT_MALLOC_ERROR;
    }

  /* I have to keep an integer as key, I wil use the pointer buffkey->addr for this, 
   * this also means that buffkey->len will be 0 */
  memcpy(pipaddr, ipaddr, sizeof(sockaddr_t));

  buffkey.addr = (caddr_t) pipaddr;
  buffkey.len = sizeof(sockaddr_t);

  gettimeofday(&tv0, NULL) ;
  rc = getnameinfo((struct sockaddr *)pipaddr, sizeof(sockaddr_t),
                   nfs_ip_name->hostname, sizeof(nfs_ip_name->hostname),
                   NULL, 0, 0);
  gettimeofday(&tv1, NULL) ;
  timersub(&tv1, &tv0, &dur) ;


  sprint_sockaddr(pipaddr, ipstring, sizeof(ipstring));

  /* display warning if DNS resolution took more that 1.0s */
  if (dur.tv_sec >= 1)
  {
       LogEvent(COMPONENT_DISPATCH,
                "Warning: long DNS query for %s: %u.%06u sec", ipstring,
                (unsigned int)dur.tv_sec, (unsigned int)dur.tv_usec );
  }


  /* Ask for the name to be cached */
  if(rc != 0)
    {
       LogEvent(COMPONENT_DISPATCH,
                "Cannot resolve address %s, error %s",
                ipstring, gai_strerror(rc));

       gsh_free(nfs_ip_name);
       gsh_free(pipaddr);
       return IP_NAME_NETDB_ERROR;
    }

  LogDebug(COMPONENT_DISPATCH,
           "Inserting %s->%s to addr cache",
           ipstring, nfs_ip_name->hostname);

  /* I build the data with the request pointer that should be in state 'IN USE' */
  nfs_ip_name->timestamp = time(NULL);

  buffdata.addr = (caddr_t) nfs_ip_name;
  buffdata.len = sizeof(nfs_ip_name_t);

  if(HashTable_Set(ht_ip_name, &buffkey, &buffdata) != HASHTABLE_SUCCESS)
    return IP_NAME_INSERT_MALLOC_ERROR;

  /* Copy the value for the caller */
  strmaxcpy(hostname, nfs_ip_name->hostname, size);

  return IP_NAME_SUCCESS;
}                               /* nfs_ip_name_add */

/**
 *
 * nfs_ip_name_get: Tries to get an entry for ip_name cache.
 *
 * Tries to get an entry for ip_name cache.
 * 
 * @param ipaddr   [IN]  the ip address requested
 * @param hostname [OUT] the hostname
 *
 * @return the result previously set if *pstatus == IP_NAME_SUCCESS
 *
 */
int nfs_ip_name_get(sockaddr_t *ipaddr, char *hostname, size_t size)
{
  struct gsh_buffdesc buffkey;
  struct gsh_buffdesc buffval;
  nfs_ip_name_t *nfs_ip_name;
  char ipstring[SOCK_NAME_MAX + 1];

  sprint_sockaddr(ipaddr, ipstring, sizeof(ipstring));

  buffkey.addr = (caddr_t) ipaddr;
  buffkey.len = sizeof(sockaddr_t);

  if(HashTable_Get(ht_ip_name, &buffkey, &buffval) == HASHTABLE_SUCCESS)
    {
      nfs_ip_name = buffval.addr;
      strmaxcpy(hostname, nfs_ip_name->hostname, size);

      LogFullDebug(COMPONENT_DISPATCH,
                   "Cache get hit for %s->%s",
                   ipstring, nfs_ip_name->hostname);

      return IP_NAME_SUCCESS;
    }

  LogFullDebug(COMPONENT_DISPATCH,
               "Cache get miss for %s",
               ipstring);

  return IP_NAME_NOT_FOUND;
}                               /* nfs_ip_name_get */

/**
 *
 * nfs_ip_name_remove: Tries to remove an entry for ip_name cache
 *
 * Tries to remove an entry for ip_name cache.
 * 
 * @param ipaddr           [IN]    the ip address to be uncached.
 *
 * @return the result previously set if *pstatus == IP_NAME_SUCCESS
 *
 */
int nfs_ip_name_remove(sockaddr_t *ipaddr)
{
  struct gsh_buffdesc buffkey, old_value;
  nfs_ip_name_t *nfs_ip_name = NULL;
  char ipstring[SOCK_NAME_MAX + 1];

  sprint_sockaddr(ipaddr, ipstring, sizeof(ipstring));

  buffkey.addr = (caddr_t) ipaddr;
  buffkey.len = sizeof(sockaddr_t);

  if(HashTable_Del(ht_ip_name, &buffkey, NULL, &old_value) == HASHTABLE_SUCCESS)
    {
      nfs_ip_name = (nfs_ip_name_t *) old_value.addr;

      LogFullDebug(COMPONENT_DISPATCH,
                   "Cache remove hit for %s->%s",
                   ipstring, nfs_ip_name->hostname);

      gsh_free(nfs_ip_name);
      return IP_NAME_SUCCESS;
    }

  LogFullDebug(COMPONENT_DISPATCH,
               "Cache remove miss for %s",
               ipstring);

  return IP_NAME_NOT_FOUND;
}                               /* nfs_ip_name_remove */

/**
 *
 * nfs_Init_ip_name: Init the hashtable for IP/name cache.
 *
 * Perform all the required initialization for hashtable IP/name cache
 * 
 * @param param [IN] parameter used to init the ip name cache
 *
 * @return 0 if successful, -1 otherwise
 *
 */
int nfs_Init_ip_name(nfs_ip_name_parameter_t param)
{
  if((ht_ip_name = HashTable_Init(&param.hash_param)) == NULL)
    {
      LogCrit(COMPONENT_INIT, "NFS IP_NAME: Cannot init IP/name cache");
      return -1;
    }

  /* Set the expiration time */
  expiration_time = param.expiration_time;

  return IP_NAME_SUCCESS;
}                               /* nfs_Init_ip_name */

int nfs_ip_name_populate(char *path)
{
  config_file_t config_file;
  config_item_t block;
  int var_max;
  int var_index;
  int err;
  char *key_name;
  char *key_value;
  char label[MAXNAMLEN + 1];
  sockaddr_t ipaddr;
  nfs_ip_name_t *nfs_ip_name;
  sockaddr_t *pipaddr;
  struct gsh_buffdesc buffkey;
  struct gsh_buffdesc buffdata;

  config_file = config_ParseFile(path);

  if(!config_file)
    {
      LogCrit(COMPONENT_CONFIG, "Can't open file %s", path);

      return IP_NAME_NOT_FOUND;
    }

  /* Get the config BLOCK */
  if((block = config_FindItemByName(config_file, CONF_LABEL_IP_NAME_HOSTS)) == NULL)
    {
      LogCrit(COMPONENT_CONFIG,
              "Can't get label %s in file %s",
              CONF_LABEL_IP_NAME_HOSTS, path);
      return IP_NAME_NOT_FOUND;
    }
  else if(config_ItemType(block) != CONFIG_ITEM_BLOCK)
    {
      /* Expected to be a block */
      return IP_NAME_NOT_FOUND;
    }

  var_max = config_GetNbItems(block);

  for(var_index = 0; var_index < var_max; var_index++)
    {
      config_item_t item;

      item = config_GetItemByIndex(block, var_index);

      /* Get key's name */
      if((err = config_GetKeyValue(item, &key_name, &key_value)) != 0)
        {
          LogCrit(COMPONENT_CONFIG,
                  "Error reading key[%d] from section \"%s\" of configuration file.",
                  var_index, label);
          return IP_NAME_NOT_FOUND;
        }

      err = ipstring_to_sockaddr(key_value, &ipaddr);
      if(err != 0)
        {
          LogCrit(COMPONENT_CONFIG,
                  "Error converting %s to an ipaddress %s",
                  key_value, gai_strerror(err));
          return IP_NAME_NOT_FOUND;
        }

      /* Entry to be cached */
      nfs_ip_name = gsh_malloc(sizeof(nfs_ip_name_t));
      if(nfs_ip_name == NULL)
        return IP_NAME_INSERT_MALLOC_ERROR;

      pipaddr = gsh_malloc(sizeof(sockaddr_t));
      if(pipaddr == NULL)
        {
          gsh_free(nfs_ip_name);
          return IP_NAME_INSERT_MALLOC_ERROR;
        }

      strmaxcpy(nfs_ip_name->hostname, key_name, sizeof(nfs_ip_name->hostname));
      nfs_ip_name->timestamp = time(NULL);
      memcpy(pipaddr, &ipaddr, sizeof(sockaddr_t));

      buffdata.addr = (caddr_t) nfs_ip_name;
      buffdata.len = sizeof(nfs_ip_name_t);

      buffkey.addr = (caddr_t) pipaddr;
      buffkey.len = sizeof(sockaddr_t);

      if(HashTable_Set(ht_ip_name, &buffkey, &buffdata) != HASHTABLE_SUCCESS)
        {
          gsh_free(nfs_ip_name);
          gsh_free(pipaddr);
          return IP_NAME_INSERT_MALLOC_ERROR;
        }
    }

  if(isFullDebug(COMPONENT_CONFIG))
    HashTable_Log(COMPONENT_CONFIG, ht_ip_name);

  return IP_NAME_SUCCESS;
}                               /* nfs_ip_name_populate */
