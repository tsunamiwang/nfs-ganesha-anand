/*
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * Copyright (C) Red Hat  Inc., 2013
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

/**
 * @file  export.c
 * @author Shyamsundar R <srangana@redhat.com>
 * @author Anand Subramanian <anands@redhat.com>
 *
 * @brief GLUSTERFS FSAL export object
 */

#include "config.h"
#include <fcntl.h>
#include "fsal.h"
#include <libgen.h>             /* used for 'dirname' */
#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include "FSAL/fsal_commonlib.h"
#include "FSAL/fsal_config.h"
#include "gluster_internal.h"

/* export_release
 * default case is to throw a fault error.
 * creating an export is not supported so getting here is bad
 */

static fsal_status_t export_release(struct fsal_export *exp_hdl)
{
	fsal_status_t            status = {ERR_FSAL_NO_ERROR, 0};
	struct glusterfs_export *glfs_export = 
		container_of(exp_hdl, struct glusterfs_export, export);

	pthread_mutex_lock(&glfs_export->export.lock);
	if((glfs_export->export.refs > 0) ||
		 (!glist_empty(&glfs_export->export.handles))) {
		pthread_mutex_unlock(&glfs_export->export.lock);
		status.major = ERR_FSAL_INVAL;
		return status;
	}

	fsal_detach_export(glfs_export->export.fsal, &glfs_export->export.exports);
	free_export_ops(&glfs_export->export);
	glfs_export->export.ops = NULL;
	pthread_mutex_unlock(&glfs_export->export.lock);

	glfs_fini(glfs_export->gl_fs);
	glfs_export->gl_fs = NULL;
	free(glfs_export->export_path);
	glfs_export->export_path = NULL;
	pthread_mutex_destroy(&glfs_export->export.lock);
	gsh_free(glfs_export);
	glfs_export = NULL;

	return status;
}

/* lookup_path
 * default case is not supported
 */

static fsal_status_t lookup_path(struct fsal_export *export_pub,
				 const struct req_op_context *opctx,
				 const char *path,
				 struct fsal_obj_handle **pub_handle)
{
	int                      rc = 0;
	fsal_status_t            status = {ERR_FSAL_NO_ERROR, 0};
	const char              *realpath;
	struct stat              sb;
	struct glfs_object      *glhandle = NULL;
	struct glfs_gfid        *gfid = NULL;
	struct glusterfs_handle *objhandle = NULL;
	struct glusterfs_export *glfs_export = 
		container_of(export_pub, struct glusterfs_export, export);

	LogFullDebug(COMPONENT_FSAL, "In args: path = %s", path);

	*pub_handle = NULL;
	realpath = "/";

	glhandle = glfs_h_lookupat(glfs_export->gl_fs, NULL, realpath, &sb);
	if (glhandle == NULL) {
		status = gluster2fsal_error(errno);
		goto out;
	}

	gfid = glfs_h_extract_gfid(glhandle);
	if (gfid == NULL) {
		status = gluster2fsal_error(errno);
		goto out;
	}

	rc = construct_handle(glfs_export, &sb, glhandle, gfid, &objhandle);
	if (rc != 0) {
		status = gluster2fsal_error(rc);
		goto out;
	}

	*pub_handle = &objhandle->handle;

	return status;
out:
	gluster_cleanup_vars(glhandle, gfid);

	return status;
}

static fsal_status_t lookup_junction(struct fsal_export *exp_hdl,
				     struct fsal_obj_handle *junction,
				     struct fsal_obj_handle **handle)
{
	latency_dump();
	return fsalstat(ERR_FSAL_NO_ERROR, 0);	
}

/* extract_handle
 * default case is not supported
 */

static fsal_status_t extract_handle(struct fsal_export *exp_hdl,
				    fsal_digesttype_t in_type,
				    struct gsh_buffdesc *fh_desc)
{
	size_t fh_size;
	struct timespec          s_time, e_time;

	now(&s_time);

	/* sanity checks */
	if( !fh_desc || !fh_desc->addr)
		return fsalstat(ERR_FSAL_FAULT, 0);

	fh_size = GLUSTER_GFID_SIZE;
	if(in_type == FSAL_DIGEST_NFSV2) {
		if(fh_desc->len < fh_size) {
			LogMajor(COMPONENT_FSAL,
			"V2 size too small for handle.  should be %lu, got %lu",
			fh_size, fh_desc->len);
			return fsalstat(ERR_FSAL_SERVERFAULT, 0);
		}
	} else if(in_type != FSAL_DIGEST_SIZEOF && fh_desc->len != fh_size) {
		LogMajor(COMPONENT_FSAL,
			 "Size mismatch for handle.  should be %lu, got %lu",
			 fh_size, fh_desc->len);
		return fsalstat(ERR_FSAL_SERVERFAULT, 0);
	}

	fh_desc->len = fh_size;  /* pass back the actual size */

	now(&e_time);
	latency_update(&s_time, &e_time, lat_extract_handle);
	return fsalstat(ERR_FSAL_NO_ERROR, 0);
}


/* create_handle
 * default case is not supported
 */

static fsal_status_t create_handle(struct fsal_export *export_pub,
				   const struct req_op_context *opctx,
				   struct gsh_buffdesc *fh_desc,
				   struct fsal_obj_handle **pub_handle)
{
	int                      rc = 0;
	fsal_status_t            status = {ERR_FSAL_NO_ERROR, 0};
	struct stat              sb;
	struct glfs_object      *glhandle = NULL;
	struct glfs_gfid        *gfid = NULL;
	struct glusterfs_handle *objhandle = NULL;
	struct glusterfs_export *glfs_export = 
		container_of(export_pub, struct glusterfs_export, export);
	struct timespec          s_time, e_time;

	now(&s_time);

	//LogCrit(COMPONENT_FSAL, "called");

	*pub_handle = NULL;

	if (fh_desc->len != GLUSTER_GFID_SIZE) {
		status.major = ERR_FSAL_INVAL;
		goto out;
	}

	gfid = calloc(1, sizeof (struct glfs_gfid));
	if (gfid == NULL) {
		status.major = ERR_FSAL_NOMEM;
		goto out;
	}

	gfid->len = GLUSTER_GFID_SIZE;
	gfid->id = calloc(gfid->len, sizeof(unsigned char));
	if (gfid->id == NULL) {
		status.major = ERR_FSAL_NOMEM;
		goto out;
	}

	memcpy(gfid->id, fh_desc->addr, 16);

	glhandle = glfs_h_create_from_gfid(glfs_export->gl_fs, gfid, &sb);
	if (glhandle == NULL) {
		status = gluster2fsal_error(errno);
		goto out;
	}

	rc = construct_handle(glfs_export, &sb, glhandle, gfid, &objhandle);
	if (rc != 0) {
		status = gluster2fsal_error(rc);
		goto out;
	}

	*pub_handle = &objhandle->handle;

	now(&e_time);
	latency_update(&s_time, &e_time, lat_create_handle);
	return status;
out:
	gluster_cleanup_vars(glhandle, gfid);

	now(&e_time);
	latency_update(&s_time, &e_time, lat_create_handle);
	return status;
}

/**
 * @brief Fail to create a FSAL data server handle from a wire handle
 *
 * @param[in]  exp_hdl  The export in which to create the handle
 * @param[out] handle   FSAL object handle
 *
 * @retval NFS4ERR_BADHANDLE.
 */
/*
static nfsstat4
create_ds_handle(struct fsal_export *const exp_hdl,
		 const struct gsh_buffdesc *const hdl_desc,
		 struct fsal_ds_handle **const handle)
{
	return NFS4ERR_BADHANDLE;
}
*/
/* get_dynamic_info
 * default case is not supported
 */

static fsal_status_t get_dynamic_info(struct fsal_export *exp_hdl,
				      const struct req_op_context *opctx,
				      fsal_dynamicfsinfo_t *infop)
{
	int                      rc = 0;
	fsal_status_t            status = {ERR_FSAL_NO_ERROR, 0};
	struct statvfs           vfssb;
	struct glusterfs_export *glfs_export = 
		container_of(exp_hdl, struct glusterfs_export, export);

	rc = glfs_statvfs(glfs_export->gl_fs, glfs_export->export_path, 
			  &vfssb);
	if (rc != 0) {
		return gluster2fsal_error(rc);
	}

	memset(infop, 0, sizeof(fsal_dynamicfsinfo_t));
	infop->total_bytes = vfssb.f_frsize * vfssb.f_blocks;
	infop->free_bytes = vfssb.f_frsize * vfssb.f_bfree;
	infop->avail_bytes = vfssb.f_frsize * vfssb.f_bavail;
	infop->total_files = vfssb.f_files;
	infop->free_files = vfssb.f_ffree;
	infop->avail_files = vfssb.f_favail;
	infop->time_delta.tv_sec = 1;
	infop->time_delta.tv_nsec = 0;

	return status;
}

/* TODO: We have gone POSIX way for the APIs below, can consider the CEPH way 
 * in case all are constants across all volumes etc. */
static bool fs_supports (struct fsal_export *exp_hdl, fsal_fsinfo_options_t option)
{
	struct fsal_staticfsinfo_t *info;
	
	info = gluster_staticinfo (exp_hdl->fsal);
	return fsal_supports (info, option);
}

static uint64_t fs_maxfilesize (struct fsal_export *exp_hdl)
{
	struct fsal_staticfsinfo_t *info;
	
	info = gluster_staticinfo (exp_hdl->fsal);
	return fsal_maxfilesize (info);
}

static uint32_t fs_maxread (struct fsal_export *exp_hdl)
{
	struct fsal_staticfsinfo_t *info;
	
	info = gluster_staticinfo (exp_hdl->fsal);
	return fsal_maxread (info);
}

static uint32_t fs_maxwrite (struct fsal_export *exp_hdl)
{
	struct fsal_staticfsinfo_t *info;
	
	info = gluster_staticinfo (exp_hdl->fsal);
	return fsal_maxwrite (info);
}

static uint32_t fs_maxlink (struct fsal_export *exp_hdl)
{
	struct fsal_staticfsinfo_t *info;
	
	info = gluster_staticinfo (exp_hdl->fsal);
	return fsal_maxlink (info);
}

static uint32_t fs_maxnamelen (struct fsal_export *exp_hdl)
{
	struct fsal_staticfsinfo_t *info;
	
	info = gluster_staticinfo (exp_hdl->fsal);
	return fsal_maxnamelen (info);
}

static uint32_t fs_maxpathlen (struct fsal_export *exp_hdl)
{
	struct fsal_staticfsinfo_t *info;
	
	info = gluster_staticinfo (exp_hdl->fsal);
	return fsal_maxpathlen (info);
}

static struct timespec fs_lease_time (struct fsal_export *exp_hdl)
{
	struct fsal_staticfsinfo_t *info;
	
	info = gluster_staticinfo (exp_hdl->fsal);
	return fsal_lease_time (info);
}

static fsal_aclsupp_t fs_acl_support (struct fsal_export *exp_hdl)
{
	struct fsal_staticfsinfo_t *info;
	
	info = gluster_staticinfo (exp_hdl->fsal);
	return fsal_acl_support (info);
}

static attrmask_t fs_supported_attrs (struct fsal_export *exp_hdl)
{
	struct fsal_staticfsinfo_t *info;
	
	info = gluster_staticinfo (exp_hdl->fsal);
	return fsal_supported_attrs (info);
}

static uint32_t fs_umask (struct fsal_export *exp_hdl)
{
	struct fsal_staticfsinfo_t *info;
	
	info = gluster_staticinfo (exp_hdl->fsal);
	return fsal_umask (info);
}

static uint32_t fs_xattr_access_rights (struct fsal_export *exp_hdl)
{
	struct fsal_staticfsinfo_t *info;
	
	info = gluster_staticinfo (exp_hdl->fsal);
	return fsal_xattr_access_rights (info);
}

/* check_quota
 * return happiness for now.
 */
/*
static fsal_status_t check_quota(struct fsal_export *exp_hdl,
				 const char * filepath,
				 int quota_type,
				 struct req_op_context *req_ctx)
{
	return fsalstat(ERR_FSAL_NO_ERROR, 0) ;
}
*/
/* get_quota
 * default case not supported
 */
/*
static fsal_status_t get_quota(struct fsal_export *exp_hdl,
			       const char * filepath,
			       int quota_type,
			       struct req_op_context *req_ctx,
			       fsal_quota_t *pquota)
{
	return fsalstat(ERR_FSAL_NOTSUPP, 0) ;
}
*/
/* set_quota
 * default case not supported
 */
/*
static fsal_status_t set_quota(struct fsal_export *exp_hdl,
			       const char *filepath,
			       int quota_type,
			       struct req_op_context *req_ctx,
			       fsal_quota_t * pquota,
			       fsal_quota_t * presquota)
{
	return fsalstat(ERR_FSAL_NOTSUPP, 0) ;
}
*/
/**
 * @brief Be uninformative about a device
 */
/*
static nfsstat4
getdeviceinfo(struct fsal_export *exp_hdl,
	      XDR *da_addr_body,
	      const layouttype4 type,
	      const struct pnfs_deviceid *deviceid)
{
	return NFS4ERR_NOTSUPP;
}
*/
/**
 * @brief Be uninformative about all devices
 */
/*
static nfsstat4
getdevicelist(struct fsal_export *exp_hdl,
	      layouttype4 type,
	      void *opaque,
	      bool (*cb)(void *opaque,
			 const uint64_t id),
	      struct fsal_getdevicelist_res *res)
{
	return NFS4ERR_NOTSUPP;
}
*/
/**
 * @brief Support no layout types
 */
/*
static void
fs_layouttypes(struct fsal_export *exp_hdl,
	       size_t *count,
	       const layouttype4 **types)
{
	*count = 0;
	*types = NULL;
}
*/
/**
 * @brief Read no bytes through layouts
 */
/*
static uint32_t
fs_layout_blocksize(struct fsal_export *exp_hdl)
{
	return 0;
}
*/
/**
 * @brief No segments
 */
/*
static uint32_t
fs_maximum_segments(struct fsal_export *exp_hdl)
{
	return 0;
}
*/
/**
 * @brief No loc_body
 */
/*
static size_t
fs_loc_body_size(struct fsal_export *exp_hdl)
{
	return 0;
}
*/
/**
 * No da_addr.
 */
/*
static size_t
fs_da_addr_size(struct fsal_export *exp_hdl)
{
	return 0;
}
*/
/**
 * @brief Get write verifier
 *
 * This function is called by write and commit to match the commit verifier
 * with the one returned on  write.
 *
 * @param[in/out] verf_desc Address and length of verifier
 *
 * @return No errors
 */
/*static void
global_verifier(struct gsh_buffdesc *verf_desc)
{
	memcpy(verf_desc->addr, &NFS4_write_verifier, verf_desc->len);
}*/

/**
 * @brief Set operations for exports
 *
 * This function overrides operations that we've implemented, leaving
 * the rest for the default.
 *
 * @param[in,out] ops Operations vector
 */

void export_ops_init(struct export_ops *ops)
{
	ops->release = export_release;
	ops->lookup_path = lookup_path;
	ops->lookup_junction = lookup_junction;
	ops->extract_handle = extract_handle;
	ops->create_handle = create_handle;
	//ops->create_ds_handle = create_ds_handle;
	ops->get_fs_dynamic_info = get_dynamic_info;
	ops->fs_supports = fs_supports;
	ops->fs_maxfilesize = fs_maxfilesize;
	ops->fs_maxread = fs_maxread;
	ops->fs_maxwrite = fs_maxwrite;
	ops->fs_maxlink = fs_maxlink;
	ops->fs_maxnamelen = fs_maxnamelen;
	ops->fs_maxpathlen = fs_maxpathlen;
	ops->fs_lease_time = fs_lease_time;
	ops->fs_acl_support = fs_acl_support;
	ops->fs_supported_attrs = fs_supported_attrs;
	ops->fs_umask = fs_umask;
	ops->fs_xattr_access_rights = fs_xattr_access_rights;
	//export_ops_pnfs(ops);
}

void handle_ops_init(struct fsal_obj_ops *ops);

/**
 * @brief  create_export
 *         Create an export point and return a handle to it to be kept
 *         in the export list.
 *         First lookup the fsal, then create the export and then put
 *         the fsal back. Returns the export with one reference taken.
 */
fsal_status_t glusterfs_create_export(struct fsal_module *fsal_hdl,
                                const char *export_path,
                                const char *fs_options,
                                struct exportlist *exp_entry,
                                struct fsal_module *next_fsal,
                                const struct fsal_up_vector *up_ops,
                                struct fsal_export **pub_export)
{
	/* TODO: Work with fs_options to get volume and other details */
	int                      rc;
	fsal_status_t            status = {ERR_FSAL_NO_ERROR, 0};
	struct glusterfs_export *glfsexport = NULL;
	glfs_t                  *fs = NULL;
	/* FIXME: do this better */
	char                     glvolname[512], glhostname[512];

	LogDebug(COMPONENT_FSAL, "In args: export path = %s, fs options = %s", 
		 export_path, fs_options);

	if (( NULL == export_path ) || (strlen(export_path) == 0)) {
		status.major = ERR_FSAL_INVAL;
		LogCrit(COMPONENT_FSAL, "No path to export.");
		goto error;
	}

	if ((NULL == fs_options) || (strlen(fs_options) == 0)) {
		status.major = ERR_FSAL_INVAL;
		LogCrit(COMPONENT_FSAL, 
		"Missing FS specific information. Export: %s", export_path);
		goto error;
	}

	if (next_fsal != NULL) {
		status.major = ERR_FSAL_INVAL;
		LogCrit(COMPONENT_FSAL, 
			"Stacked FSALs unsupported. Export: %s", export_path);
		goto error;
	}

	if (!(fs_specific_has(fs_options, GLUSTER_VOLNAME_KEY, glvolname, 
		 sizeof(glvolname)))) {
		status.major = ERR_FSAL_INVAL;
		LogCrit(COMPONENT_FSAL, 
			"FS specific missing gluster volume name. Export: %s", 
			export_path);
		goto error;
	}

	if (!(fs_specific_has(fs_options, GLUSTER_HOSTNAME_KEY, glhostname, 
		 sizeof(glhostname)))) {
		status.major = ERR_FSAL_INVAL;
		LogCrit(COMPONENT_FSAL, 
			"FS specific missing gluster hostname or IP address. Export: %s", export_path);
		goto error;
	}

	glfsexport = gsh_calloc(1, sizeof(struct glusterfs_export));
	if (glfsexport == NULL) {
		status.major = ERR_FSAL_NOMEM;
		LogCrit(COMPONENT_FSAL, 
			"Unable to allocate export object.  Export: %s", 
			export_path);
		goto error;
	}

	if (fsal_export_init(&glfsexport->export, exp_entry) != 0) {
		status.major = ERR_FSAL_NOMEM;
		LogCrit(COMPONENT_FSAL, 
			"Unable to allocate export ops vectors.  Export: %s", 
			export_path);
		goto error;
	}

	export_ops_init(glfsexport->export.ops);
	handle_ops_init(glfsexport->export.obj_ops);
	glfsexport->export.up_ops = up_ops;

	/* TODO: Consider using gluster2fsal_error instead of setting error */
	fs = glfs_new (glvolname);
	if (!fs) {
		status.major = ERR_FSAL_SERVERFAULT;
		LogCrit(COMPONENT_FSAL, 
			"Unable to create new glfs. Export: %s", export_path);
		goto error;
	}

	rc = glfs_set_volfile_server (fs, "tcp", glhostname, 24007);
	if (rc != 0) {
		status.major = ERR_FSAL_SERVERFAULT;
		LogCrit(COMPONENT_FSAL, 
			"Unable to set volume file. Export: %s", export_path);
		goto error;
	}

	rc = glfs_set_logging (fs, "/tmp/gfapi.log", 7);
	if (rc != 0) {
		status.major = ERR_FSAL_SERVERFAULT;
		LogCrit(COMPONENT_FSAL, 
			"Unable to set logging. Export: %s", export_path);
		goto error;
	}

	rc = glfs_init (fs);
	if (rc != 0) {
		status.major = ERR_FSAL_SERVERFAULT;
		LogCrit(COMPONENT_FSAL, 
			"Unable to initialize volume. Export: %s", export_path);
		goto error;
	}

	if ((rc = fsal_attach_export(fsal_hdl, 
		 &glfsexport->export.exports)) != 0) {
		status.major = ERR_FSAL_SERVERFAULT;
		LogCrit(COMPONENT_FSAL, 
			"Unable to attach export. Export: %s", export_path);
		goto error;
	}

	glfsexport->export_path = strdup(export_path);
	if (glfsexport->export_path == NULL) {
		status.major = ERR_FSAL_NOMEM;
		LogCrit(COMPONENT_FSAL, 
			"Unable to dupe export path. Export: %s", export_path);
		goto error;
	}

	glfsexport->gl_fs = fs;

	glfsexport->export.fsal = fsal_hdl;

	*pub_export = &glfsexport->export;

	return status;

error:
	/* FIXME: Cleanup required on error */
	return status;
}
