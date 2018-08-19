#include <uv.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h> /* PATH_MAX */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include <uthash.h>

#include <jni.h>
#include <android/asset_manager_jni.h>

int uv__getiovmax(void);
void* uv__malloc(size_t size);

ssize_t uv__fs_copyfile(uv_fs_t* req);
ssize_t uv__fs_fdatasync(uv_fs_t* req);
int uv__fs_fstat(int fd, uv_stat_t *buf);
ssize_t uv__fs_fsync(uv_fs_t* req);
ssize_t uv__fs_futime(uv_fs_t* req);
int uv__fs_lstat(const char *path, uv_stat_t *buf);
ssize_t uv__fs_mkdtemp(uv_fs_t* req);
ssize_t uv__fs_open(uv_fs_t* req);
ssize_t uv__fs_read(uv_fs_t* req);
ssize_t uv__fs_scandir(uv_fs_t* req);
ssize_t uv__fs_readlink(uv_fs_t* req);
ssize_t uv__fs_pathmax_size(const char* path);
ssize_t uv__fs_realpath(uv_fs_t* req);
ssize_t uv__fs_sendfile(uv_fs_t* req);
int uv__fs_stat(const char *path, uv_stat_t *buf);
ssize_t uv__fs_utime(uv_fs_t* req);
ssize_t uv__fs_write_all(uv_fs_t* req);

#if defined(__APPLE__) && !defined(MAC_OS_X_VERSION_10_8)
#define UV_CONST_DIRENT uv__dirent_t
#else
#define UV_CONST_DIRENT const uv__dirent_t
#endif
int uv__fs_scandir_filter(UV_CONST_DIRENT* dent);
int uv__fs_scandir_sort(UV_CONST_DIRENT** a, UV_CONST_DIRENT** b);

// extern AAssetManager *android_asset_manager;
struct AssetStatStruct;
void initAssetManager(AAssetManager *am);

int android_access(const char *pathname, int mode);
int android_chmod(const char *pathname, mode_t mode);
int android_chown(const char *pathname, uid_t owner, gid_t group);
int android_close(int fd);
ssize_t android_copyfile(uv_fs_t* req);
int android_fchmod(int fd, mode_t mode);
int android_fchown(int fd, uid_t owner, gid_t group);
int android_lchown(const char *path, uid_t owner, gid_t group);
ssize_t android_fdatasync(uv_fs_t* req);
int android_fstat(int fd, uv_stat_t *buf);
ssize_t android_fsync(uv_fs_t* req);
int android_ftruncate(int fd, off_t length);
ssize_t android_futime(uv_fs_t* req);
int android_lstat(const char *path, uv_stat_t *buf);
int android_link(const char *oldpath, const char *newpath);
int android_mkdir(const char *pathname, mode_t mode);
ssize_t android_mkdtemp(uv_fs_t* req);
ssize_t android_open(uv_fs_t* req);
ssize_t android_read(uv_fs_t* req);
ssize_t android_scandir(uv_fs_t* req);
ssize_t android_readlink(uv_fs_t* req);
ssize_t android_realpath(uv_fs_t* req);
int android_rename(const char *oldpath, const char *newpath);
int android_rmdir(const char *path);
ssize_t android_sendfile(uv_fs_t* req);
int android_stat(const char *path, uv_stat_t *buf);
int android_symlink(const char *target, const char *linkpath);
int android_unlink(const char *pathname);
ssize_t android_utime(uv_fs_t* req);
ssize_t android_write_all(uv_fs_t* req);
