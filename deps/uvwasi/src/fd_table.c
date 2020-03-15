#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef _WIN32
# include <sys/types.h>
#endif /* _WIN32 */

#include "uv.h"
#include "fd_table.h"
#include "wasi_types.h"
#include "wasi_rights.h"
#include "uv_mapping.h"
#include "uvwasi_alloc.h"


static uvwasi_errno_t uvwasi__insert_stdio(uvwasi_t* uvwasi,
                                           struct uvwasi_fd_table_t* table,
                                           const uvwasi_fd_t fd,
                                           const uvwasi_fd_t expected,
                                           const char* name) {
  struct uvwasi_fd_wrap_t* wrap;
  uvwasi_filetype_t type;
  uvwasi_rights_t base;
  uvwasi_rights_t inheriting;
  uvwasi_errno_t err;

  err = uvwasi__get_filetype_by_fd(fd, &type);
  if (err != UVWASI_ESUCCESS)
    return err;

  err = uvwasi__get_rights(fd, UV_FS_O_RDWR, type, &base, &inheriting);
  if (err != UVWASI_ESUCCESS)
    return err;

  err = uvwasi_fd_table_insert(uvwasi,
                               table,
                               fd,
                               name,
                               name,
                               type,
                               base,
                               inheriting,
                               0,
                               &wrap);
  if (err != UVWASI_ESUCCESS)
    return err;

  if (wrap->id != expected)
    err = UVWASI_EBADF;

  uv_mutex_unlock(&wrap->mutex);
  return err;
}


uvwasi_errno_t uvwasi_fd_table_insert(uvwasi_t* uvwasi,
                                      struct uvwasi_fd_table_t* table,
                                      uv_file fd,
                                      const char* mapped_path,
                                      const char* real_path,
                                      uvwasi_filetype_t type,
                                      uvwasi_rights_t rights_base,
                                      uvwasi_rights_t rights_inheriting,
                                      int preopen,
                                      struct uvwasi_fd_wrap_t** wrap) {
  struct uvwasi_fd_wrap_t* entry;
  struct uvwasi_fd_wrap_t** new_fds;
  uvwasi_errno_t err;
  uint32_t new_size;
  uint32_t index;
  uint32_t i;
  int r;
  size_t mp_len;
  char* mp_copy;
  size_t rp_len;
  char* rp_copy;

  mp_len = strlen(mapped_path);
  rp_len = strlen(real_path);
  entry = (struct uvwasi_fd_wrap_t*)
    uvwasi__malloc(uvwasi, sizeof(*entry) + mp_len + rp_len + 2);
  if (entry == NULL) return UVWASI_ENOMEM;

  mp_copy = (char*)(entry + 1);
  rp_copy = mp_copy + mp_len + 1;
  memcpy(mp_copy, mapped_path, mp_len);
  mp_copy[mp_len] = '\0';
  memcpy(rp_copy, real_path, rp_len);
  rp_copy[rp_len] = '\0';

  uv_rwlock_wrlock(&table->rwlock);

  /* Check that there is room for a new item. If there isn't, grow the table. */
  if (table->used >= table->size) {
    new_size = table->size * 2;
    new_fds = uvwasi__realloc(uvwasi, table->fds, new_size * sizeof(*new_fds));
    if (new_fds == NULL) {
      uvwasi__free(uvwasi, entry);
      err = UVWASI_ENOMEM;
      goto exit;
    }

    for (i = table->size; i < new_size; ++i)
      new_fds[i] = NULL;

    index = table->size;
    table->fds = new_fds;
    table->size = new_size;
  } else {
    /* The table is big enough, so find an empty slot for the new data. */
    int valid_slot = 0;
    for (i = 0; i < table->size; ++i) {
      if (table->fds[i] == NULL) {
        valid_slot = 1;
        index = i;
        break;
      }
    }

    /* This should never happen. */
    if (valid_slot == 0) {
      uvwasi__free(uvwasi, entry);
      err = UVWASI_ENOSPC;
      goto exit;
    }
  }

  table->fds[index] = entry;

  r = uv_mutex_init(&entry->mutex);
  if (r != 0) {
    err = uvwasi__translate_uv_error(r);
    goto exit;
  }

  entry->id = index;
  entry->fd = fd;
  entry->path = mp_copy;
  entry->real_path = rp_copy;
  entry->type = type;
  entry->rights_base = rights_base;
  entry->rights_inheriting = rights_inheriting;
  entry->preopen = preopen;

  if (wrap != NULL) {
    uv_mutex_lock(&entry->mutex);
    *wrap = entry;
  }

  table->used++;
  err = UVWASI_ESUCCESS;
exit:
  uv_rwlock_wrunlock(&table->rwlock);
  return err;
}


uvwasi_errno_t uvwasi_fd_table_init(uvwasi_t* uvwasi,
                                    uvwasi_options_t* options) {
  struct uvwasi_fd_table_t* table;
  uvwasi_errno_t err;
  int r;

  /* Require an initial size of at least three to store the stdio FDs. */
  if (uvwasi == NULL || options == NULL || options->fd_table_size < 3)
    return UVWASI_EINVAL;

  table = &uvwasi->fds;
  table->fds = NULL;
  table->used = 0;
  table->size = options->fd_table_size;
  table->fds = uvwasi__calloc(uvwasi,
                              options->fd_table_size,
                              sizeof(struct uvwasi_fd_wrap_t*));

  if (table->fds == NULL)
    return UVWASI_ENOMEM;

  r = uv_rwlock_init(&table->rwlock);
  if (r != 0) {
    err = uvwasi__translate_uv_error(r);
    /* Free table->fds and set it to NULL here. This is done explicitly instead
       of jumping to error_exit because uvwasi_fd_table_free() relies on fds
       being NULL to know whether or not to destroy the rwlock.
    */
    uvwasi__free(uvwasi, table->fds);
    table->fds = NULL;
    return err;
  }

  /* Create the stdio FDs. */
  err = uvwasi__insert_stdio(uvwasi, table, options->in, 0, "<stdin>");
  if (err != UVWASI_ESUCCESS)
    goto error_exit;

  err = uvwasi__insert_stdio(uvwasi, table, options->out, 1, "<stdout>");
  if (err != UVWASI_ESUCCESS)
    goto error_exit;

  err = uvwasi__insert_stdio(uvwasi, table, options->err, 2, "<stderr>");
  if (err != UVWASI_ESUCCESS)
    goto error_exit;

  return UVWASI_ESUCCESS;
error_exit:
  uvwasi_fd_table_free(uvwasi, table);
  return err;
}


void uvwasi_fd_table_free(uvwasi_t* uvwasi, struct uvwasi_fd_table_t* table) {
  struct uvwasi_fd_wrap_t* entry;
  uint32_t i;

  if (table == NULL)
    return;

  for (i = 0; i < table->size; i++) {
    entry = table->fds[i];
    if (entry == NULL) continue;

    uv_mutex_destroy(&entry->mutex);
    uvwasi__free(uvwasi, entry);
  }

  if (table->fds != NULL) {
    uvwasi__free(uvwasi, table->fds);
    table->fds = NULL;
    table->size = 0;
    table->used = 0;
    uv_rwlock_destroy(&table->rwlock);
  }
}


uvwasi_errno_t uvwasi_fd_table_insert_preopen(uvwasi_t* uvwasi,
                                              struct uvwasi_fd_table_t* table,
                                              const uv_file fd,
                                              const char* path,
                                              const char* real_path) {
  uvwasi_filetype_t type;
  uvwasi_rights_t base;
  uvwasi_rights_t inheriting;
  uvwasi_errno_t err;

  if (table == NULL || path == NULL || real_path == NULL)
    return UVWASI_EINVAL;

  err = uvwasi__get_filetype_by_fd(fd, &type);
  if (err != UVWASI_ESUCCESS)
    return err;

  if (type != UVWASI_FILETYPE_DIRECTORY)
    return UVWASI_ENOTDIR;

  err = uvwasi__get_rights(fd, 0, type, &base, &inheriting);
  if (err != UVWASI_ESUCCESS)
    return err;

  return uvwasi_fd_table_insert(uvwasi,
                                table,
                                fd,
                                path,
                                real_path,
                                UVWASI_FILETYPE_DIRECTORY,
                                UVWASI__RIGHTS_DIRECTORY_BASE,
                                UVWASI__RIGHTS_DIRECTORY_INHERITING,
                                1,
                                NULL);
}


uvwasi_errno_t uvwasi_fd_table_get(struct uvwasi_fd_table_t* table,
                                   const uvwasi_fd_t id,
                                   struct uvwasi_fd_wrap_t** wrap,
                                   uvwasi_rights_t rights_base,
                                   uvwasi_rights_t rights_inheriting) {
  uvwasi_errno_t err;

  if (table == NULL)
    return UVWASI_EINVAL;

  uv_rwlock_wrlock(&table->rwlock);
  err = uvwasi_fd_table_get_nolock(table,
                                   id,
                                   wrap,
                                   rights_base,
                                   rights_inheriting);
  uv_rwlock_wrunlock(&table->rwlock);
  return err;
}


/* uvwasi_fd_table_get_nolock() retrieves a file descriptor and locks its mutex,
   but does not lock the file descriptor table like uvwasi_fd_table_get() does.
*/
uvwasi_errno_t uvwasi_fd_table_get_nolock(struct uvwasi_fd_table_t* table,
                                          const uvwasi_fd_t id,
                                          struct uvwasi_fd_wrap_t** wrap,
                                          uvwasi_rights_t rights_base,
                                          uvwasi_rights_t rights_inheriting) {
  struct uvwasi_fd_wrap_t* entry;

  if (table == NULL || wrap == NULL)
    return UVWASI_EINVAL;

  if (id >= table->size)
    return UVWASI_EBADF;

  entry = table->fds[id];

  if (entry == NULL || entry->id != id)
    return UVWASI_EBADF;

  /* Validate that the fd has the necessary rights. */
  if ((~entry->rights_base & rights_base) != 0 ||
      (~entry->rights_inheriting & rights_inheriting) != 0) {
    return UVWASI_ENOTCAPABLE;
  }

  uv_mutex_lock(&entry->mutex);
  *wrap = entry;
  return UVWASI_ESUCCESS;
}


uvwasi_errno_t uvwasi_fd_table_remove_nolock(uvwasi_t* uvwasi,
                                             struct uvwasi_fd_table_t* table,
                                             const uvwasi_fd_t id) {
  struct uvwasi_fd_wrap_t* entry;

  if (table == NULL)
    return UVWASI_EINVAL;

  if (id >= table->size)
    return UVWASI_EBADF;

  entry = table->fds[id];

  if (entry == NULL || entry->id != id)
    return UVWASI_EBADF;

  uv_mutex_destroy(&entry->mutex);
  uvwasi__free(uvwasi, entry);
  table->fds[id] = NULL;
  table->used--;
  return UVWASI_ESUCCESS;
}


uvwasi_errno_t uvwasi_fd_table_renumber(struct uvwasi_s* uvwasi,
                                        struct uvwasi_fd_table_t* table,
                                        const uvwasi_fd_t dst,
                                        const uvwasi_fd_t src) {
  struct uvwasi_fd_wrap_t* dst_entry;
  struct uvwasi_fd_wrap_t* src_entry;
  uv_fs_t req;
  uvwasi_errno_t err;
  int r;

  if (uvwasi == NULL || table == NULL)
    return UVWASI_EINVAL;

  if (dst == src)
    return UVWASI_ESUCCESS;

  uv_rwlock_wrlock(&table->rwlock);

  if (dst >= table->size || src >= table->size) {
    err = UVWASI_EBADF;
    goto exit;
  }

  dst_entry = table->fds[dst];
  src_entry = table->fds[src];

  if (dst_entry == NULL || dst_entry->id != dst ||
      src_entry == NULL || src_entry->id != src) {
    err = UVWASI_EBADF;
    goto exit;
  }

  uv_mutex_lock(&dst_entry->mutex);
  uv_mutex_lock(&src_entry->mutex);

  /* Close the existing destination descriptor. */
  r = uv_fs_close(NULL, &req, dst_entry->fd, NULL);
  uv_fs_req_cleanup(&req);
  if (r != 0) {
    uv_mutex_unlock(&src_entry->mutex);
    uv_mutex_unlock(&dst_entry->mutex);
    err = uvwasi__translate_uv_error(r);
    goto exit;
  }

  /* Move the source entry to the destination slot in the table. */
  table->fds[dst] = table->fds[src];
  table->fds[dst]->id = dst;
  uv_mutex_unlock(&table->fds[dst]->mutex);
  table->fds[src] = NULL;
  table->used--;

  /* Clean up what's left of the old destination entry. */
  uv_mutex_unlock(&dst_entry->mutex);
  uv_mutex_destroy(&dst_entry->mutex);
  uvwasi__free(uvwasi, dst_entry);

  err = UVWASI_ESUCCESS;
exit:
  uv_rwlock_wrunlock(&table->rwlock);
  return err;
}


uvwasi_errno_t uvwasi_fd_table_lock(struct uvwasi_fd_table_t* table) {
  if (table == NULL)
    return UVWASI_EINVAL;

  uv_rwlock_wrlock(&table->rwlock);
  return UVWASI_ESUCCESS;
}


uvwasi_errno_t uvwasi_fd_table_unlock(struct uvwasi_fd_table_t* table) {
  if (table == NULL)
    return UVWASI_EINVAL;

  uv_rwlock_wrunlock(&table->rwlock);
  return UVWASI_ESUCCESS;
}
