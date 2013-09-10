/*
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
 * @addtogroup idmapper
 * @{
 */

/**
 * @file    uid_grplist_cache.c
 * @brief   Uid->Group List mapping cache functions
 */
#include "config.h"
#include "log.h"
#include "config_parsing.h"
#include <string.h>
#include <pwd.h>
#include <grp.h>
#include "gsh_intrinsic.h"
#include "ganesha_types.h"
#include "common_utils.h"
#include "avltree.h"
#include "uid2grp.h"
#include "abstract_atomic.h"

/**
 * @brief User entry in the IDMapper cache
 */


struct cache_info {
	uid_t uid; /*< Corresponding UID */
        struct gsh_buffdesc uname;
	struct group_data group_data ;
	struct avltree_node uname_node; /*< Node in the name tree */
	struct avltree_node uid_node; /*< Node in the UID tree */
};

/**
 * @brief Number of entires in the UID cache, should be prime.
 */

#define id_cache_size 1009

/**
 * @brief UID cache, may only be accessed with uid2grp_user_lock
 * held.  If uid2grp_user_lock is held for read, it must be accessed
 * atomically.  (For a write, normal fetch/store is sufficient since
 * others are kept out.)
 */

static struct avltree_node *uid_grplist_cache[id_cache_size];


/**
 * @brief Lock that protects the idmapper user cache
 */

pthread_rwlock_t uid2grp_user_lock = PTHREAD_RWLOCK_INITIALIZER;

/**
 * @brief Tree of users, by name
 */

static struct avltree uname_tree;

/**
 * @brief Tree of users, by ID
 */

static struct avltree uid_tree;

/**
 * @brief Compare two buffers
 *
 * Handle the case where one buffer is a left sub-buffer of another
 * buffer by counting the longer one as larger.
 *
 * @param[in] buff1 A buffer
 * @param[in] buffa Another buffer
 *
 * @retval -1 if buff1 is less than buffa
 * @retval 0 if buff1 and buffa are equal
 * @retval 1 if buff1 is greater than buffa
 */

static inline int buffdesc_comparator(const struct gsh_buffdesc *buffa,
				      const struct gsh_buffdesc *buff1)
{
	int mr = memcmp(buff1->addr, buffa->addr, MIN(buff1->len,
						      buffa->len));
	if (unlikely(mr == 0)) {
		if (buff1->len < buffa->len) {
			return -1;
		} else if (buff1->len > buffa->len) {
			return 1;
		} else {
			return 0;
		}
	} else {
		return mr;
	}
}

/**
 * @brief Comparison for user names
 *
 * @param[in] node1 A node
 * @param[in] nodea Another node
 *
 * @retval -1 if node1 is less than nodea
 * @retval 0 if node1 and nodea are equal
 * @retval 1 if node1 is greater than nodea
 */

static int uname_comparator(const struct avltree_node *node1,
			    const struct avltree_node *nodea)
{
	struct cache_info *user1
		= avltree_container_of(node1, struct cache_info,
				       uname_node);
	struct cache_info *usera
		= avltree_container_of(nodea, struct cache_info,
				       uname_node);

	return buffdesc_comparator(&user1->uname, &usera->uname);
}

/**
 * @brief Comparison for UIDs
 *
 * @param[in] node1 A node
 * @param[in] nodea Another node
 *
 * @retval -1 if node1 is less than nodea
 * @retval 0 if node1 and nodea are equal
 * @retval 1 if node1 is greater than nodea
 */

static int uid_comparator(const struct avltree_node *node1,
			  const struct avltree_node *nodea)
{
	struct cache_info *user1
		= avltree_container_of(node1, struct cache_info,
				       uid_node);
	struct cache_info *usera
		= avltree_container_of(nodea, struct cache_info,
				       uid_node);

	if (user1->uid < usera->uid) {
		return -1;
	} else if (user1->uid > usera->uid) {
		return 1;
	} else {
		return 0;
	}
}

/**
 * @brief Initialize the IDMapper cache
 */

void uid2grp_cache_init(void)
{
	avltree_init(&uname_tree, uname_comparator, 0);
	avltree_init(&uid_tree, uid_comparator, 0);
	memset(uid_grplist_cache, 0,
	       id_cache_size * sizeof(struct avltree_node*));
}

/**
 * @brief Add a user entry to the cache
 *
 * @note The caller must hold uid2grp_user_lock for write.
 *
 * @param[in] name The user name
 * @param[in] uid  The user ID
 * @param[in] gid  Optional.  Set to NULL if no gid is known.
 *
 * @retval true on success.
 * @retval false if our reach exceeds our grasp.
 */

bool uid2grp_add_user(const struct gsh_buffdesc *name,
		       uid_t uid,
                       struct group_data * pgdata)
{
	struct avltree_node *found_name = NULL;
	struct avltree_node *found_id = NULL;
	struct cache_info *new = gsh_malloc(sizeof(struct cache_info) +
					    name->len);

	if (new == NULL) {
		LogMajor(COMPONENT_IDMAPPER,
			 "Unable to allocate memory for new node. "
			 "This is not wonderful.");
		return false;
	}
	new->uname.addr = (char *)new + sizeof(struct cache_info);
	new->uname.len = name->len;
	new->uid = uid;
	memcpy(new->uname.addr, name->addr, name->len);
        memcpy(&new->group_data, pgdata, sizeof( struct group_data ) ) ;

	found_name = avltree_insert(&new->uname_node, &uname_tree);
	found_id = avltree_insert(&new->uid_node, &uid_tree);

	if (unlikely(found_name || found_id)) {
		/* Obnoxious complexity.  And we can't even reuse the
		   code because the types are different.  Foo. */
		struct cache_info *coll_name
			= (found_name ?
			   avltree_container_of(found_name,
						struct cache_info,
						uname_node) :
			   NULL);
		struct cache_info *coll_id
			= (found_id ?
			   avltree_container_of(found_id,
						struct cache_info,
						uid_node) :
			   NULL);

		LogWarn(COMPONENT_IDMAPPER,
			"Collision found in user cache.  This likely "
			"indicates a bug in ganesha or a screwy host "
			"configuration.");

		if (coll_name) {
			avltree_remove(found_name, &uname_tree);
			if (coll_name != coll_id) {
				uid_grplist_cache[coll_name->uid % id_cache_size]
					= NULL;
				avltree_remove(&coll_name->uid_node,
					       &uid_tree);
			}
		}
		if (coll_id) {
			avltree_remove(found_id, &uid_tree);
			if (coll_id != coll_name) {
				avltree_remove(&coll_id->uname_node,
					       &uname_tree);
			}
		}
		if (coll_id == coll_name) {
			gsh_free(coll_id);
		} else if (!coll_name) {
			gsh_free(coll_id);
		} else if (!coll_id) {
			gsh_free(coll_name);
		} else {
			gsh_free(coll_id);
			gsh_free(coll_name);
		}
		avltree_insert(&new->uname_node, &uname_tree);
		avltree_insert(&new->uid_node, &uid_tree);
	}
	uid_grplist_cache[uid % id_cache_size] = &new->uid_node;

	return true;
}

/**
 * @brief Look up a user by name
 *
 * @note The caller must hold uid2grp_user_lock for read.
 *
 * @param[in]  name The user name to look up.
 * @param[out] uid  The user ID found.  May be NULL if the caller
 *                  isn't interested in the UID.  (This seems
 *                  unlikely.)
 * @param[out] gid  The GID for the user, or NULL if there is
 *                  none. The caller may specify NULL if it isn't
 *                  interested.
 *
 * @retval true on success.
 * @retval false if we need to try, try again.
 */

bool uid2grp_lookup_by_uname(const struct gsh_buffdesc *name,
			      uid_t *uid,
                              struct group_data **pgdata )
{
	struct cache_info prototype = {
		.uname = *name
	};
	struct avltree_node *found_node
		= avltree_lookup(&prototype.uname_node,
				 &uname_tree);
	struct cache_info *found_user;
	struct avltree_node **cache_entry;

	if (unlikely(!found_node)) {
		return false;
	}
	found_user = avltree_container_of(found_node,
					  struct cache_info,
					  uname_node);

	/* I assume that if someone likes this user enough to look it
	   up by name, they'll like it enough to look it up by ID
	   later. */

	cache_entry = uid_grplist_cache + (found_user->uid % id_cache_size);
	atomic_store_uint64_t((uint64_t *)cache_entry,
			       (uint64_t) &found_user->uid_node);

	*uid = found_user->uid;
	*pgdata = &found_user->group_data;
	

	return true;
}

/**
 * @brief Look up a user by ID
 *
 * @note The caller must hold uid2grp_user_lock for read.
 *
 * @param[in]  uid  The user ID to look up.
 * @param[out] name The user name to look up. (May be NULL if the user
 *                  doesn't care about the name.)
 * @param[out] gid  The GID for the user, or NULL if there is
 *                  none. The caller may specify NULL if it isn't
 *                  interested.
 *
 * @retval true on success.
 * @retval false if we weren't so successful.
 */

bool uid2grp_lookup_by_uid(const uid_t uid,
			   struct gsh_buffdesc **name,
			   struct group_data **pgdata )
{
	struct cache_info prototype = {
		.uid = uid
	};
	struct avltree_node **cache_entry = uid_grplist_cache +
		(prototype.uid % id_cache_size);
	struct avltree_node *found_node
		= ((struct avltree_node*)
		   atomic_fetch_uint64_t((uint64_t *)cache_entry));
	struct cache_info *found_user;

	if (likely(found_node)) {
		found_user = avltree_container_of(found_node,
						  struct cache_info,
						  uid_node);
	} else {
		found_node = avltree_lookup(&prototype.uid_node,
					    &uid_tree);
		if (unlikely(!found_node)) {
			return false;
		}
		atomic_store_uint64_t((uintptr_t *)cache_entry,
				      (uintptr_t) found_node);
		found_user = avltree_container_of(found_node,
						  struct cache_info,
						  uid_node);
	}

	*name = &found_user->uname;
	*pgdata = &found_user->group_data ;

	return true;
}


/**
 * @brief Wipe out the idmapper cache
 */

void uid2grp_clear_cache(void)
{
	struct avltree_node *node;

	pthread_rwlock_wrlock(&uid2grp_user_lock);

	memset(uid_grplist_cache, 0,
	       id_cache_size * sizeof(struct avltree_node*));

	while ((node = avltree_first(&uname_tree))) {
		struct cache_info *user
			= avltree_container_of(node,
					       struct cache_info,
					       uname_node);
		avltree_remove(&user->uname_node, &uname_tree);
		avltree_remove(&user->uid_node, &uid_tree);
		gsh_free(user);
	}

	assert(avltree_first(&uid_tree) == NULL);

	pthread_rwlock_unlock(&uid2grp_user_lock);
}

/** @} */
