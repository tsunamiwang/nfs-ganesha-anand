/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) Red Hat  Inc., 2011
 * Author: Anand Subramanian anands@redhat.com
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
 * ------------- 
 */

/* main.c
 * Module core functions
 */

#include <sys/fsuid.h>
#include "gluster_internal.h"
#include "fsal_convert.h"
#include "FSAL/fsal_commonlib.h"

/**
 * @brief FSAL status mapping from GlusterFS errors
 *
 * This function returns a fsal_status_t with the FSAL error as the
 * major, and the posix error as minor.	
 *
 * @param[in] gluster_errorcode Gluster error
 *
 * @return FSAL status.
 */

fsal_status_t gluster2fsal_error(const int gluster_errorcode)
{
	fsal_status_t status;
	status.minor = gluster_errorcode;

	switch (gluster_errorcode) {

		case 0:
			status.major = ERR_FSAL_NO_ERROR;
			break;

		case EPERM:
			status.major = ERR_FSAL_PERM;
			break;

		case ENOENT:
			status.major = ERR_FSAL_NOENT;
			break;

		case ECONNREFUSED:
		case ECONNABORTED:
		case ECONNRESET:
		case EIO:
		case ENFILE:
		case EMFILE:
		case EPIPE:
			status.major = ERR_FSAL_IO;
			break;

		case ENODEV:
		case ENXIO:
			status.major = ERR_FSAL_NXIO;
			break;

		case EBADF:
			/**
			 * @todo: The EBADF error also happens when file is
			 *	  opened for reading, and we try writting in
			 *	  it.  In this case, we return
			 *	  ERR_FSAL_NOT_OPENED, but it doesn't seems to
			 *	  be a correct error translation.
			 */
			status.major = ERR_FSAL_NOT_OPENED;
			break;

		case ENOMEM:
			status.major = ERR_FSAL_NOMEM;
			break;

		case EACCES:
			status.major = ERR_FSAL_ACCESS;
			break;

		case EFAULT:
			status.major = ERR_FSAL_FAULT;
			break;

		case EEXIST:
			status.major = ERR_FSAL_EXIST;
			break;

		case EXDEV:
			status.major = ERR_FSAL_XDEV;
			break;

		case ENOTDIR:
			status.major = ERR_FSAL_NOTDIR;
			break;

		case EISDIR:
			status.major = ERR_FSAL_ISDIR;
			break;

		case EINVAL:
			status.major = ERR_FSAL_INVAL;
			break;

		case EFBIG:
			status.major = ERR_FSAL_FBIG;
			break;

		case ENOSPC:
			status.major = ERR_FSAL_NOSPC;
			break;

		case EMLINK:
			status.major = ERR_FSAL_MLINK;
			break;

		case EDQUOT:
			status.major = ERR_FSAL_DQUOT;
			break;

		case ENAMETOOLONG:
			status.major = ERR_FSAL_NAMETOOLONG;
			break;

		case ENOTEMPTY:
			status.major = ERR_FSAL_NOTEMPTY;
			break;

		case ESTALE:
			status.major = ERR_FSAL_STALE;
			break;

		case EAGAIN:
		case EBUSY:
			status.major = ERR_FSAL_DELAY;
			break;

		default:
			status.major = ERR_FSAL_SERVERFAULT;
			break;
	}

	return status;
}

/**
 * @brief Convert a struct stat from Gluster to a struct attrlist
 *
 * This function writes the content of the supplied struct stat to the
 * struct fsalsattr.
 *
 * @param[in]  buffstat Stat structure
 * @param[out] fsalattr FSAL attributes
 */

void stat2fsal_attributes(const struct stat *buffstat,
			  struct attrlist *fsalattr)
{
	FSAL_CLEAR_MASK(fsalattr->mask);

	/* Fills the output struct */
	fsalattr->type = posix2fsal_type(buffstat->st_mode);
	FSAL_SET_MASK(fsalattr->mask, ATTR_TYPE);

	fsalattr->filesize = buffstat->st_size;
	FSAL_SET_MASK(fsalattr->mask, ATTR_SIZE);

	fsalattr->fsid = posix2fsal_fsid(buffstat->st_dev);
	FSAL_SET_MASK(fsalattr->mask, ATTR_FSID);

	fsalattr->fileid = buffstat->st_ino;
	FSAL_SET_MASK(fsalattr->mask, ATTR_FILEID);

	fsalattr->mode = unix2fsal_mode(buffstat->st_mode);
	FSAL_SET_MASK(fsalattr->mask, ATTR_MODE);

	fsalattr->numlinks = buffstat->st_nlink;
	FSAL_SET_MASK(fsalattr->mask, ATTR_NUMLINKS);

	fsalattr->owner = buffstat->st_uid;
	FSAL_SET_MASK(fsalattr->mask, ATTR_OWNER);

	fsalattr->group = buffstat->st_gid;
	FSAL_SET_MASK(fsalattr->mask, ATTR_GROUP);

	fsalattr->atime = posix2fsal_time(buffstat->st_atime, 0);
	FSAL_SET_MASK(fsalattr->mask, ATTR_ATIME);

	fsalattr->ctime = posix2fsal_time(buffstat->st_ctime, 0);
	FSAL_SET_MASK(fsalattr->mask, ATTR_CTIME);

	fsalattr->mtime = posix2fsal_time(buffstat->st_mtime, 0);
	FSAL_SET_MASK(fsalattr->mask, ATTR_MTIME);

	fsalattr->chgtime
		= posix2fsal_time(MAX_2(buffstat->st_mtime,
					buffstat->st_ctime), 0);
	fsalattr->change = fsalattr->chgtime.tv_sec;
	FSAL_SET_MASK(fsalattr->mask, ATTR_CHGTIME);

	fsalattr->spaceused = buffstat->st_blocks * S_BLKSIZE;
	FSAL_SET_MASK(fsalattr->mask, ATTR_SPACEUSED);

	fsalattr->rawdev = posix2fsal_devt(buffstat->st_rdev);
	FSAL_SET_MASK(fsalattr->mask, ATTR_RAWDEV);
}

struct fsal_staticfsinfo_t *gluster_staticinfo (struct fsal_module *hdl)
{
	struct glusterfs_fsal_module *glfsal_module;

	glfsal_module = container_of(hdl, struct glusterfs_fsal_module, fsal);
	return &glfsal_module->fs_info;
}

/**
 * @brief Construct a new filehandle
 *
 * This function constructs a new Gluster FSAL object handle and attaches
 * it to the export.  After this call the attributes have been filled
 * in and the handdle is up-to-date and usable.
 *
 * @param[in]  st     Stat data for the file
 * @param[in]  export Export on which the object lives
 * @param[out] obj    Object created
 *
 * @return 0 on success, negative error codes on failure.
 */

int construct_handle(struct glusterfs_export *glexport, const struct stat *sb, 
		     struct glfs_object *glhandle, struct glfs_gfid *gfid, 
		     struct glusterfs_handle **obj)
{
	int                      rc = 0;
	struct glusterfs_handle *constructing = NULL;

	*obj = NULL;

	constructing = gsh_calloc(1, sizeof(struct glusterfs_handle));
	if (constructing == NULL) {
		errno = ENOMEM;
		return -1;
	}

	stat2fsal_attributes(sb, &constructing->handle.attributes);
	constructing->glhandle = glhandle;
	constructing->gfid = gfid;
	constructing->glfd = NULL;

	rc = (fsal_obj_handle_init(&constructing->handle,
				    &glexport->export,
				    constructing->handle.attributes.type));
	if (rc != 0) {
		gsh_free(constructing);
		return rc;
	}

	*obj = constructing;

	return 0;
}

void gluster_cleanup_vars(struct glfs_object *glhandle, struct glfs_gfid *gfid)
{
	if (gfid) {
		if (gfid->id)
			free(gfid->id);
		free(gfid);
	}

	if (glhandle) {
		/* Error ignored, this is a cleanup operation, can't do much.
		 * TODO: Useful point for logging? */
		glfs_h_close(glhandle);
	}

	return;
}

/* fs_specific_has() parses the fs_specific string for a particular key, 
 *  returns true if found, and optionally returns a val if the string is
 *  of the form key=val.
 *
 * The fs_specific string is a comma (,) separated options where each option
 * can be of the form key=value or just key. Example:
 *	FS_specific = "foo=baz,enable_A";
 */
bool fs_specific_has(const char *fs_specific, const char* key,
			    char *val, int max_val_bytes)
{
	char *next_comma, *option;
	bool ret;
	char *fso_dup = NULL;

	if (!fs_specific || !fs_specific[0])
		return false;

	fso_dup = strdup(fs_specific);
	if (!fso_dup) {
		LogCrit(COMPONENT_FSAL,"strdup(%s) failed", fs_specific);
		return false;
	}

	for (option = strtok_r(fso_dup, ",", &next_comma);
	     option; option=strtok_r(NULL, ",", &next_comma)) {
		char *k = option;
		char *v = k;

		strsep(&v, "=");
		if (0 == strcmp(k, key)) {
			if(val)
				strncpy(val, v, max_val_bytes);
			ret = true;
			goto cleanup;
		}
	}

	ret = false;
cleanup:
	free(fso_dup);
	return ret;
}

int setids(uid_t nuid, uid_t ngid, uid_t *suid, uid_t *sgid)
{
	int rc = 0;
	uid_t euid, egid;

	/* TODO: Speed up same ID checking, store the euid/gid 
	 * than get it each time? */
	euid = geteuid();
	if (euid != nuid) {
		if (euid != setfsuid(nuid)) {
			rc = -1;
			errno = EPERM;
			goto out;
		}
	}

	egid = getegid();
	if (egid != ngid) {
		if (egid != setfsgid(ngid)) {
			rc = -1;
			errno = EPERM;
			/* revert the fsuid on errors */
			setfsuid(euid);
			goto out;
		}
	}

	if (suid != NULL)
		*suid = euid;

	if (sgid != NULL)
		*sgid = egid;

out:
	return rc;
}

void latency_update(struct timespec *s_time, struct timespec *e_time, int opnum)
{
	atomic_add_uint64_t(&glfsal_latencies[opnum].overall_time, 
			      timespec_diff(s_time, e_time));
	atomic_add_uint64_t(&glfsal_latencies[opnum].count, 1);
}

void latency_dump(void)
{
	int i = 0;
	
	for (; i < LATENCY_SLOTS; i++) {
		LogCrit(COMPONENT_FSAL, "Op:%d:Count:%llu:nsecs:%llu", i, 
			(long long unsigned int)glfsal_latencies[i].count, 
			(long long unsigned int)glfsal_latencies[i].overall_time);
	}
}


/**
 * @brief pthread set/getspecific functions for uid/gid key based ops
 * 
 * The following set of functions setup the uid/gid pthread key structs
 * for use by gluster apis to set uid/gid for different fops.
 * Since these are thread-specific, they don't need special locking
 * primitives right now.
 *
 * Following function inits the uid key
 *
 * @param[in] none
 * @param[out] int 
 */
int
glfs_uid_keyinit( void )
{
	int rc = -1;

	if ( uid_key == NULL ) {
		uid_key = calloc( 1, sizeof( pthread_key_t ));
	}	
	rc = pthread_key_create( uid_key, NULL );

	return rc;
}

/**
 * @brief initialize the gid pthread_key struct for use by gluster api
 *
 * @param[in] none
 * @param[out] int
 */
int
glfs_gid_keyinit( void )
{
	int rc = -1;
	
	if ( gid_key == NULL ) {
		gid_key = calloc( 1, sizeof( pthread_key_t ));
	}	
	rc = pthread_key_create( gid_key, NULL );

	return rc;
}

/**
 * @brief return the value associated with the uid pthread_key_t struct
 *
 * @param[in] void
 * @param[out] void * ptr to the key value
 */
void *glfs_uid_get( void )
{
	uid_t *uid = NULL;

	uid = pthread_getspecific( *uid_key );

	return uid;  
}

/**
 * @brief set a value wrt either the uid or gid pthread_key_t object
 *
 * @param[in] pthread_key_t * ptr to the key to be used
 * @param[in] const void *    ptr to the value to be stored in the key
 * 
 * @param[out] int a negative return value indicates the set function failed
 */
int glfs_uidgid_set( pthread_key_t *key, const void *val )
{
	int rc = -1;

	if ( (NULL == val) || (NULL == key) ) {
		return rc;
	}

	rc = pthread_setspecific( *key, val );

	return rc;

}

/**
 * @brief return the value associated with the gid pthread_key_t struct
 *
 * @param[in] void
 * @param[out] void * ptr to the key value
 */
void *glfs_gid_get( void )
{
	gid_t *gid = NULL;

	gid = pthread_getspecific( *gid_key );

	return gid;  
}

