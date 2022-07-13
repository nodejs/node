#ifndef __UVWASI_WASI_RIGHTS_H__
#define __UVWASI_WASI_RIGHTS_H__

#include "wasi_types.h"

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


uvwasi_errno_t uvwasi__get_rights(uv_file fd,
                                  int flags,
                                  uvwasi_filetype_t type,
                                  uvwasi_rights_t* rights_base,
                                  uvwasi_rights_t* rights_inheriting);


#endif /* __UVWASI_WASI_RIGHTS_H__ */
