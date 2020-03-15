#ifndef __UVWASI_FD_TABLE_H__
#define __UVWASI_FD_TABLE_H__

#include <stdint.h>
#include "uv.h"
#include "wasi_types.h"

struct uvwasi_s;
struct uvwasi_options_s;

struct uvwasi_fd_wrap_t {
  uvwasi_fd_t id;
  uv_file fd;
  char* path;
  char* real_path;
  uvwasi_filetype_t type;
  uvwasi_rights_t rights_base;
  uvwasi_rights_t rights_inheriting;
  int preopen;
  uv_mutex_t mutex;
};

struct uvwasi_fd_table_t {
  struct uvwasi_fd_wrap_t** fds;
  uint32_t size;
  uint32_t used;
  uv_rwlock_t rwlock;
};

uvwasi_errno_t uvwasi_fd_table_init(struct uvwasi_s* uvwasi,
                                    struct uvwasi_options_s* options);
void uvwasi_fd_table_free(struct uvwasi_s* uvwasi,
                          struct uvwasi_fd_table_t* table);
uvwasi_errno_t uvwasi_fd_table_insert(struct uvwasi_s* uvwasi,
                                      struct uvwasi_fd_table_t* table,
                                      uv_file fd,
                                      const char* mapped_path,
                                      const char* real_path,
                                      uvwasi_filetype_t type,
                                      uvwasi_rights_t rights_base,
                                      uvwasi_rights_t rights_inheriting,
                                      int preopen,
                                      struct uvwasi_fd_wrap_t** wrap);
uvwasi_errno_t uvwasi_fd_table_insert_preopen(struct uvwasi_s* uvwasi,
                                              struct uvwasi_fd_table_t* table,
                                              const uv_file fd,
                                              const char* path,
                                              const char* real_path);
uvwasi_errno_t uvwasi_fd_table_get(struct uvwasi_fd_table_t* table,
                                   const uvwasi_fd_t id,
                                   struct uvwasi_fd_wrap_t** wrap,
                                   uvwasi_rights_t rights_base,
                                   uvwasi_rights_t rights_inheriting);
uvwasi_errno_t uvwasi_fd_table_get_nolock(struct uvwasi_fd_table_t* table,
                                          const uvwasi_fd_t id,
                                          struct uvwasi_fd_wrap_t** wrap,
                                          uvwasi_rights_t rights_base,
                                          uvwasi_rights_t rights_inheriting);
uvwasi_errno_t uvwasi_fd_table_remove_nolock(struct uvwasi_s* uvwasi,
                                             struct uvwasi_fd_table_t* table,
                                             const uvwasi_fd_t id);
uvwasi_errno_t uvwasi_fd_table_renumber(struct uvwasi_s* uvwasi,
                                        struct uvwasi_fd_table_t* table,
                                        const uvwasi_fd_t dst,
                                        const uvwasi_fd_t src);
uvwasi_errno_t uvwasi_fd_table_lock(struct uvwasi_fd_table_t* table);
uvwasi_errno_t uvwasi_fd_table_unlock(struct uvwasi_fd_table_t* table);

#endif /* __UVWASI_FD_TABLE_H__ */
