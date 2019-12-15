#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef _WIN32
# include <sys/types.h>
#endif /* _WIN32 */

#include "uv.h"
#include "fd_table.h"
#include "wasi_types.h"
#include "uv_mapping.h"
#include "uvwasi_alloc.h"


#define UVWASI__RIGHTS_ALL (UVWASI_RIGHT_FD_DATASYNC |                        \
                            UVWASI_RIGHT_FD_READ |                            \
                            UVWASI_RIGHT_FD_SEEK |                            \
                            UVWASI_RIGHT_FD_FDSTAT_SET_FLAGS |                \
                            UVWASI_RIGHT_FD_SYNC |                            \
                            UVWASI_RIGHT_FD_TELL |                            \
                            UVWASI_RIGHT_FD_WRITE |                           \
                            UVWASI_RIGHT_FD_ADVISE |                          \
                            UVWASI_RIGHT_FD_ALLOCATE |                        \
                            UVWASI_RIGHT_PATH_CREATE_DIRECTORY |              \
                            UVWASI_RIGHT_PATH_CREATE_FILE |                   \
                            UVWASI_RIGHT_PATH_LINK_SOURCE |                   \
                            UVWASI_RIGHT_PATH_LINK_TARGET |                   \
                            UVWASI_RIGHT_PATH_OPEN |                          \
                            UVWASI_RIGHT_FD_READDIR |                         \
                            UVWASI_RIGHT_PATH_READLINK |                      \
                            UVWASI_RIGHT_PATH_RENAME_SOURCE |                 \
                            UVWASI_RIGHT_PATH_RENAME_TARGET |                 \
                            UVWASI_RIGHT_PATH_FILESTAT_GET |                  \
                            UVWASI_RIGHT_PATH_FILESTAT_SET_SIZE |             \
                            UVWASI_RIGHT_PATH_FILESTAT_SET_TIMES |            \
                            UVWASI_RIGHT_FD_FILESTAT_GET |                    \
                            UVWASI_RIGHT_FD_FILESTAT_SET_TIMES |              \
                            UVWASI_RIGHT_FD_FILESTAT_SET_SIZE |               \
                            UVWASI_RIGHT_PATH_SYMLINK |                       \
                            UVWASI_RIGHT_PATH_UNLINK_FILE |                   \
                            UVWASI_RIGHT_PATH_REMOVE_DIRECTORY |              \
                            UVWASI_RIGHT_POLL_FD_READWRITE |                  \
                            UVWASI_RIGHT_SOCK_SHUTDOWN)

#define UVWASI__RIGHTS_BLOCK_DEVICE_BASE UVWASI__RIGHTS_ALL
#define UVWASI__RIGHTS_BLOCK_DEVICE_INHERITING UVWASI__RIGHTS_ALL

#define UVWASI__RIGHTS_CHARACTER_DEVICE_BASE UVWASI__RIGHTS_ALL
#define UVWASI__RIGHTS_CHARACTER_DEVICE_INHERITING UVWASI__RIGHTS_ALL

#define UVWASI__RIGHTS_REGULAR_FILE_BASE (UVWASI_RIGHT_FD_DATASYNC |          \
                                          UVWASI_RIGHT_FD_READ |              \
                                          UVWASI_RIGHT_FD_SEEK |              \
                                          UVWASI_RIGHT_FD_FDSTAT_SET_FLAGS |  \
                                          UVWASI_RIGHT_FD_SYNC |              \
                                          UVWASI_RIGHT_FD_TELL |              \
                                          UVWASI_RIGHT_FD_WRITE |             \
                                          UVWASI_RIGHT_FD_ADVISE |            \
                                          UVWASI_RIGHT_FD_ALLOCATE |          \
                                          UVWASI_RIGHT_FD_FILESTAT_GET |      \
                                          UVWASI_RIGHT_FD_FILESTAT_SET_SIZE | \
                                          UVWASI_RIGHT_FD_FILESTAT_SET_TIMES |\
                                          UVWASI_RIGHT_POLL_FD_READWRITE)
#define UVWASI__RIGHTS_REGULAR_FILE_INHERITING 0

#define UVWASI__RIGHTS_DIRECTORY_BASE (UVWASI_RIGHT_FD_FDSTAT_SET_FLAGS |     \
                                       UVWASI_RIGHT_FD_SYNC |                 \
                                       UVWASI_RIGHT_FD_ADVISE |               \
                                       UVWASI_RIGHT_PATH_CREATE_DIRECTORY |   \
                                       UVWASI_RIGHT_PATH_CREATE_FILE |        \
                                       UVWASI_RIGHT_PATH_LINK_SOURCE |        \
                                       UVWASI_RIGHT_PATH_LINK_TARGET |        \
                                       UVWASI_RIGHT_PATH_OPEN |               \
                                       UVWASI_RIGHT_FD_READDIR |              \
                                       UVWASI_RIGHT_PATH_READLINK |           \
                                       UVWASI_RIGHT_PATH_RENAME_SOURCE |      \
                                       UVWASI_RIGHT_PATH_RENAME_TARGET |      \
                                       UVWASI_RIGHT_PATH_FILESTAT_GET |       \
                                       UVWASI_RIGHT_PATH_FILESTAT_SET_SIZE |  \
                                       UVWASI_RIGHT_PATH_FILESTAT_SET_TIMES | \
                                       UVWASI_RIGHT_FD_FILESTAT_GET |         \
                                       UVWASI_RIGHT_FD_FILESTAT_SET_TIMES |   \
                                       UVWASI_RIGHT_PATH_SYMLINK |            \
                                       UVWASI_RIGHT_PATH_UNLINK_FILE |        \
                                       UVWASI_RIGHT_PATH_REMOVE_DIRECTORY |   \
                                       UVWASI_RIGHT_POLL_FD_READWRITE)
#define UVWASI__RIGHTS_DIRECTORY_INHERITING (UVWASI__RIGHTS_DIRECTORY_BASE |  \
                                             UVWASI__RIGHTS_REGULAR_FILE_BASE)

#define UVWASI__RIGHTS_SOCKET_BASE (UVWASI_RIGHT_FD_READ |                    \
                                    UVWASI_RIGHT_FD_FDSTAT_SET_FLAGS |        \
                                    UVWASI_RIGHT_FD_WRITE |                   \
                                    UVWASI_RIGHT_FD_FILESTAT_GET |            \
                                    UVWASI_RIGHT_POLL_FD_READWRITE |          \
                                    UVWASI_RIGHT_SOCK_SHUTDOWN)
#define UVWASI__RIGHTS_SOCKET_INHERITING UVWASI__RIGHTS_ALL;

#define UVWASI__RIGHTS_TTY_BASE (UVWASI_RIGHT_FD_READ |                       \
                                 UVWASI_RIGHT_FD_FDSTAT_SET_FLAGS |           \
                                 UVWASI_RIGHT_FD_WRITE |                      \
                                 UVWASI_RIGHT_FD_FILESTAT_GET |               \
                                 UVWASI_RIGHT_POLL_FD_READWRITE)
#define UVWASI__RIGHTS_TTY_INHERITING 0

static uvwasi_errno_t uvwasi__get_type_and_rights(uv_file fd,
                                           int flags,
                                           uvwasi_filetype_t* type,
                                           uvwasi_rights_t* rights_base,
                                           uvwasi_rights_t* rights_inheriting) {
  uv_fs_t req;
  uvwasi_filetype_t filetype;
  int read_or_write_only;
  int r;

  r = uv_fs_fstat(NULL, &req, fd, NULL);
  filetype = uvwasi__stat_to_filetype(&req.statbuf);
  uv_fs_req_cleanup(&req);
  if (r != 0)
    return uvwasi__translate_uv_error(r);

  *type = filetype;
  switch (filetype) {
    case UVWASI_FILETYPE_REGULAR_FILE:
      *rights_base = UVWASI__RIGHTS_REGULAR_FILE_BASE;
      *rights_inheriting = UVWASI__RIGHTS_REGULAR_FILE_INHERITING;
      break;

    case UVWASI_FILETYPE_DIRECTORY:
      *rights_base = UVWASI__RIGHTS_DIRECTORY_BASE;
      *rights_inheriting = UVWASI__RIGHTS_DIRECTORY_INHERITING;
      break;

    /* uvwasi__stat_to_filetype() cannot differentiate socket types. It only
       returns UVWASI_FILETYPE_SOCKET_STREAM. */
    case UVWASI_FILETYPE_SOCKET_STREAM:
      if (uv_guess_handle(fd) == UV_UDP)
        *type = UVWASI_FILETYPE_SOCKET_DGRAM;

      *rights_base = UVWASI__RIGHTS_SOCKET_BASE;
      *rights_inheriting = UVWASI__RIGHTS_SOCKET_INHERITING;
      break;

    case UVWASI_FILETYPE_CHARACTER_DEVICE:
      if (uv_guess_handle(fd) == UV_TTY) {
        *rights_base = UVWASI__RIGHTS_TTY_BASE;
        *rights_inheriting = UVWASI__RIGHTS_TTY_INHERITING;
      } else {
        *rights_base = UVWASI__RIGHTS_CHARACTER_DEVICE_BASE;
        *rights_inheriting = UVWASI__RIGHTS_CHARACTER_DEVICE_INHERITING;
      }
      break;

    case UVWASI_FILETYPE_BLOCK_DEVICE:
      *rights_base = UVWASI__RIGHTS_BLOCK_DEVICE_BASE;
      *rights_inheriting = UVWASI__RIGHTS_BLOCK_DEVICE_INHERITING;
      break;

    default:
      *rights_base = 0;
      *rights_inheriting = 0;
  }

  if (*type == UVWASI_FILETYPE_UNKNOWN)
    return UVWASI_EINVAL;

  /* Disable read/write bits depending on access mode. */
  read_or_write_only = flags & (UV_FS_O_RDONLY | UV_FS_O_WRONLY | UV_FS_O_RDWR);

  if (read_or_write_only == UV_FS_O_RDONLY)
    *rights_base &= ~UVWASI_RIGHT_FD_WRITE;
  else if (read_or_write_only == UV_FS_O_WRONLY)
    *rights_base &= ~UVWASI_RIGHT_FD_READ;

  return UVWASI_ESUCCESS;
}


static uvwasi_errno_t uvwasi__fd_table_insert(uvwasi_t* uvwasi,
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
  int index;
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
    index = -1;
    for (i = 0; i < table->size; ++i) {
      if (table->fds[i] == NULL) {
        index = i;
        break;
      }
    }

    /* index should never be -1. */
    if (index == -1) {
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
  table->used++;

  if (wrap != NULL)
    *wrap = entry;

  err = UVWASI_ESUCCESS;
exit:
  uv_rwlock_wrunlock(&table->rwlock);
  return err;
}


uvwasi_errno_t uvwasi_fd_table_init(uvwasi_t* uvwasi,
                                    struct uvwasi_fd_table_t* table,
                                    uint32_t init_size) {
  struct uvwasi_fd_wrap_t* wrap;
  uvwasi_filetype_t type;
  uvwasi_rights_t base;
  uvwasi_rights_t inheriting;
  uvwasi_errno_t err;
  uvwasi_fd_t i;
  int r;

  /* Require an initial size of at least three to store the stdio FDs. */
  if (table == NULL || init_size < 3)
    return UVWASI_EINVAL;

  table->fds = NULL;
  r = uv_rwlock_init(&table->rwlock);
  if (r != 0)
    return uvwasi__translate_uv_error(r);

  table->used = 0;
  table->size = init_size;
  table->fds = uvwasi__calloc(uvwasi,
                              init_size,
                              sizeof(struct uvwasi_fd_wrap_t*));

  if (table->fds == NULL) {
    err = UVWASI_ENOMEM;
    goto error_exit;
  }

  /* Create the stdio FDs. */
  for (i = 0; i < 3; ++i) {
    err = uvwasi__get_type_and_rights(i,
                                      UV_FS_O_RDWR,
                                      &type,
                                      &base,
                                      &inheriting);
    if (err != UVWASI_ESUCCESS)
      goto error_exit;

    err = uvwasi__fd_table_insert(uvwasi,
                                  table,
                                  i,
                                  "",
                                  "",
                                  type,
                                  base,
                                  inheriting,
                                  0,
                                  &wrap);
    if (err != UVWASI_ESUCCESS)
      goto error_exit;

    if (wrap->id != i || wrap->id != (uvwasi_fd_t) wrap->fd) {
      err = UVWASI_EBADF;
      goto error_exit;
    }
  }

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

  uvwasi__free(uvwasi, table->fds);
  table->fds = NULL;
  table->size = 0;
  table->used = 0;
  uv_rwlock_destroy(&table->rwlock);
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

  err = uvwasi__get_type_and_rights(fd, 0, &type, &base, &inheriting);
  if (err != UVWASI_ESUCCESS)
    return err;

  if (type != UVWASI_FILETYPE_DIRECTORY)
    return UVWASI_ENOTDIR;

  err = uvwasi__fd_table_insert(uvwasi,
                                table,
                                fd,
                                path,
                                real_path,
                                UVWASI_FILETYPE_DIRECTORY,
                                UVWASI__RIGHTS_DIRECTORY_BASE,
                                UVWASI__RIGHTS_DIRECTORY_INHERITING,
                                1,
                                NULL);
  if (err != UVWASI_ESUCCESS)
    return err;

  return UVWASI_ESUCCESS;
}


uvwasi_errno_t uvwasi_fd_table_insert_fd(uvwasi_t* uvwasi,
                                         struct uvwasi_fd_table_t* table,
                                         const uv_file fd,
                                         const int flags,
                                         const char* path,
                                         uvwasi_rights_t rights_base,
                                         uvwasi_rights_t rights_inheriting,
                                         struct uvwasi_fd_wrap_t* wrap) {
  struct uvwasi_fd_wrap_t* fd_wrap;
  uvwasi_filetype_t type;
  uvwasi_rights_t max_base;
  uvwasi_rights_t max_inheriting;
  uvwasi_errno_t r;

  if (table == NULL || path == NULL || wrap == NULL)
    return UVWASI_EINVAL;

  r = uvwasi__get_type_and_rights(fd, flags, &type, &max_base, &max_inheriting);
  if (r != UVWASI_ESUCCESS)
    return r;

  r = uvwasi__fd_table_insert(uvwasi,
                              table,
                              fd,
                              path,
                              path,
                              type,
                              rights_base & max_base,
                              rights_inheriting & max_inheriting,
                              0,
                              &fd_wrap);
  if (r != UVWASI_ESUCCESS)
    return r;

  *wrap = *fd_wrap;
  return UVWASI_ESUCCESS;
}


uvwasi_errno_t uvwasi_fd_table_get(const struct uvwasi_fd_table_t* table,
                                   const uvwasi_fd_t id,
                                   struct uvwasi_fd_wrap_t** wrap,
                                   uvwasi_rights_t rights_base,
                                   uvwasi_rights_t rights_inheriting) {
  struct uvwasi_fd_wrap_t* entry;
  uvwasi_errno_t err;

  if (table == NULL || wrap == NULL)
    return UVWASI_EINVAL;

  uv_rwlock_rdlock((uv_rwlock_t *)&table->rwlock);

  if (id >= table->size) {
    err = UVWASI_EBADF;
    goto exit;
  }

  entry = table->fds[id];

  if (entry == NULL || entry->id != id) {
    err = UVWASI_EBADF;
    goto exit;
  }

  /* Validate that the fd has the necessary rights. */
  if ((~entry->rights_base & rights_base) != 0 ||
      (~entry->rights_inheriting & rights_inheriting) != 0) {
    err = UVWASI_ENOTCAPABLE;
    goto exit;
  }

  uv_mutex_lock(&entry->mutex);
  *wrap = entry;
  err = UVWASI_ESUCCESS;
exit:
  uv_rwlock_rdunlock((uv_rwlock_t *)&table->rwlock);
  return err;
}


uvwasi_errno_t uvwasi_fd_table_remove(uvwasi_t* uvwasi,
                                      struct uvwasi_fd_table_t* table,
                                      const uvwasi_fd_t id) {
  struct uvwasi_fd_wrap_t* entry;
  uvwasi_errno_t err;

  if (table == NULL)
    return UVWASI_EINVAL;

  uv_rwlock_wrlock(&table->rwlock);

  if (id >= table->size) {
    err = UVWASI_EBADF;
    goto exit;
  }

  entry = table->fds[id];

  if (entry == NULL || entry->id != id) {
    err = UVWASI_EBADF;
    goto exit;
  }

  uv_mutex_destroy(&entry->mutex);
  uvwasi__free(uvwasi, entry);
  table->fds[id] = NULL;
  table->used--;
  err = UVWASI_ESUCCESS;
exit:
  uv_rwlock_wrunlock(&table->rwlock);
  return err;
}
