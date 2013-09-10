/*
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
 * @addtogroup fsal_up
 * @{
 */

/**
 * @file fsal_up_utils.c
 * @author Adam C. Emerson <aemerson@linuxbox.com>
 * @brief Utilities for use by FSAL UP-call handlers and callers
 */

#include "fsal_up.h"
#include "cache_inode_hash.h"
#include "cache_inode_lru.h"

/**
 * @brief Look up a cache entry by a key
 *
 * This function retrieves a cache entry from a key generated by the
 * FSAL.  It does no attribute refresh, since this is for upcalls.  It
 * does, however, get a reference on the cache_entry that must be
 * returned with up_put.  It will not load an entry into cache if it
 * isn't found.
 *
 * @param[in]  key   The key identifying the inode
 * @param[out] entry The entry looked up.
 *
 * @retval CACHE_INODE_SUCCESS on success.
 * @retval CACHE_INODE_NOT_FOUND if the inode could not be found or
 *         referenced.
 */

cache_inode_status_t up_get(const struct gsh_buffdesc *key, cache_entry_t **entry)
{
	cih_latch_t latch;

	if ((&cih_fhcache)->partition == NULL)
		return CACHE_INODE_NOT_FOUND;
	*entry = cih_get_by_fh_latched(key, &latch,
				       CIH_GET_RLOCK|CIH_GET_UNLOCK_ON_MISS,
				       __func__, __LINE__);
	if (*entry == NULL) {
		return CACHE_INODE_NOT_FOUND;
	}

	/* Found entry, ref it */
	cache_inode_lru_ref(*entry, LRU_REQ_INITIAL);
	cih_latch_rele(&latch);

	return CACHE_INODE_SUCCESS;
}

/** @} */
