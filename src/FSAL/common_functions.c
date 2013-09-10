/**
 * @defgroup FSAL File-System Abstraction Layer
 * @{
 */

/**
 * @file common_functions.c
 * @brief Internal and misc functions used by all/most FSALs
 */

#include "config.h"

#define fsal_increment_nbcall( _f_,_struct_status_ )

#include <sys/types.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <os/quota.h>
#include "log.h"
#include "fsal.h"
#include "fsal_types.h"


/**
 * @brief Dump and fsal_staticfsinfo_t to a log
 *
 * This is used for debugging
 *
 * @param[in] info The info to dump
 */
void display_fsinfo(struct fsal_staticfsinfo_t *info) {
	LogDebug(COMPONENT_FSAL, "FileSystem info: {");
	LogDebug(COMPONENT_FSAL, "  maxfilesize  = %"PRIX64"    ",
		 (uint64_t) info->maxfilesize);
        LogDebug(COMPONENT_FSAL, "  maxlink  = %"PRIu32,
		 info->maxlink);
	LogDebug(COMPONENT_FSAL, "  maxnamelen  = %"PRIu32,
		 info->maxnamelen);
	LogDebug(COMPONENT_FSAL, "  maxpathlen  = %"PRIu32,
		 info->maxpathlen);
	LogDebug(COMPONENT_FSAL, "  no_trunc  = %d ",
		 info->no_trunc);
	LogDebug(COMPONENT_FSAL, "  chown_restricted  = %d ",
		 info->chown_restricted);
	LogDebug(COMPONENT_FSAL, "  case_insensitive  = %d ",
		 info->case_insensitive);
	LogDebug(COMPONENT_FSAL, "  case_preserving  = %d ",
		 info->case_preserving);
	LogDebug(COMPONENT_FSAL, "  link_support  = %d  ",
		 info->link_support);
	LogDebug(COMPONENT_FSAL, "  symlink_support  = %d  ",
		 info->symlink_support);
	LogDebug(COMPONENT_FSAL, "  lock_support  = %d  ",
		 info->lock_support);
	LogDebug(COMPONENT_FSAL, "  lock_support_owner  = %d  ",
		 info->lock_support_owner);
	LogDebug(COMPONENT_FSAL, "  lock_support_async_block  = %d  ",
		 info->lock_support_async_block);
	LogDebug(COMPONENT_FSAL, "  named_attr  = %d  ",
		 info->named_attr);
	LogDebug(COMPONENT_FSAL, "  unique_handles  = %d  ",
		 info->unique_handles);
	LogDebug(COMPONENT_FSAL, "  acl_support  = %hu  ",
		 info->acl_support);
	LogDebug(COMPONENT_FSAL, "  cansettime  = %d  ",
		 info->cansettime);
	LogDebug(COMPONENT_FSAL, "  homogenous  = %d  ",
		 info->homogenous);
	LogDebug(COMPONENT_FSAL, "  supported_attrs  = %"PRIX64,
		 info->supported_attrs);
	LogDebug(COMPONENT_FSAL, "  maxread  = %"PRIu32,
		 info->maxread);
	LogDebug(COMPONENT_FSAL, "  maxwrite  = %"PRIu32,
		 info->maxwrite);
	LogDebug(COMPONENT_FSAL, "  umask  = %X ", info->umask);
	LogDebug(COMPONENT_FSAL, "  auth_exportpath_xdev  = %d  ",
		 info->auth_exportpath_xdev);
	LogDebug(COMPONENT_FSAL, "  xattr_access_rights = %#o ",
		 info->xattr_access_rights);
	LogDebug(COMPONENT_FSAL, "  accesscheck_support  = %d  ",
		 info->accesscheck_support);
	LogDebug(COMPONENT_FSAL, "  share_support  = %d  ",
		 info->share_support);
	LogDebug(COMPONENT_FSAL, "  share_support_owner  = %d  ",
		 info->share_support_owner);
	LogDebug(COMPONENT_FSAL, "}");
}
/** @} */

