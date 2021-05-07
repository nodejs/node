/* Copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "uv.h"
#include "task.h"

#include <errno.h>
#include <string.h> /* memset */
#include <fcntl.h>
#include <sys/stat.h>
#include <limits.h> /* INT_MAX, PATH_MAX, IOV_MAX */

#ifndef _WIN32
# include <unistd.h> /* unlink, rmdir, etc. */
#else
# include <winioctl.h>
# include <direct.h>
# include <io.h>
# ifndef ERROR_SYMLINK_NOT_SUPPORTED
#  define ERROR_SYMLINK_NOT_SUPPORTED 1464
# endif
# define unlink _unlink
# define rmdir _rmdir
# define open _open
# define write _write
# define close _close
# ifndef stat
#  define stat _stati64
# endif
# ifndef lseek
#   define lseek _lseek
# endif
#endif

#define TOO_LONG_NAME_LENGTH 65536
#define PATHMAX 4096

typedef struct {
  const char* path;
  double atime;
  double mtime;
} utime_check_t;


static int dummy_cb_count;
static int close_cb_count;
static int create_cb_count;
static int open_cb_count;
static int read_cb_count;
static int write_cb_count;
static int unlink_cb_count;
static int mkdir_cb_count;
static int mkdtemp_cb_count;
static int mkstemp_cb_count;
static int rmdir_cb_count;
static int scandir_cb_count;
static int stat_cb_count;
static int rename_cb_count;
static int fsync_cb_count;
static int fdatasync_cb_count;
static int ftruncate_cb_count;
static int sendfile_cb_count;
static int fstat_cb_count;
static int access_cb_count;
static int chmod_cb_count;
static int fchmod_cb_count;
static int chown_cb_count;
static int fchown_cb_count;
static int lchown_cb_count;
static int link_cb_count;
static int symlink_cb_count;
static int readlink_cb_count;
static int realpath_cb_count;
static int utime_cb_count;
static int futime_cb_count;
static int lutime_cb_count;
static int statfs_cb_count;

static uv_loop_t* loop;

static uv_fs_t open_req1;
static uv_fs_t open_req2;
static uv_fs_t read_req;
static uv_fs_t write_req;
static uv_fs_t unlink_req;
static uv_fs_t close_req;
static uv_fs_t mkdir_req;
static uv_fs_t mkdtemp_req1;
static uv_fs_t mkdtemp_req2;
static uv_fs_t mkstemp_req1;
static uv_fs_t mkstemp_req2;
static uv_fs_t mkstemp_req3;
static uv_fs_t rmdir_req;
static uv_fs_t scandir_req;
static uv_fs_t stat_req;
static uv_fs_t rename_req;
static uv_fs_t fsync_req;
static uv_fs_t fdatasync_req;
static uv_fs_t ftruncate_req;
static uv_fs_t sendfile_req;
static uv_fs_t utime_req;
static uv_fs_t futime_req;

static char buf[32];
static char buf2[32];
static char test_buf[] = "test-buffer\n";
static char test_buf2[] = "second-buffer\n";
static uv_buf_t iov;

#ifdef _WIN32
int uv_test_getiovmax(void) {
  return INT32_MAX; /* Emulated by libuv, so no real limit. */
}
#else
int uv_test_getiovmax(void) {
#if defined(IOV_MAX)
  return IOV_MAX;
#elif defined(_SC_IOV_MAX)
  static int iovmax = -1;
  if (iovmax == -1) {
    iovmax = sysconf(_SC_IOV_MAX);
    /* On some embedded devices (arm-linux-uclibc based ip camera),
     * sysconf(_SC_IOV_MAX) can not get the correct value. The return
     * value is -1 and the errno is EINPROGRESS. Degrade the value to 1.
     */
    if (iovmax == -1) iovmax = 1;
  }
  return iovmax;
#else
  return 1024;
#endif
}
#endif

#ifdef _WIN32
/*
 * This tag and guid have no special meaning, and don't conflict with
 * reserved ids.
*/
static unsigned REPARSE_TAG = 0x9913;
static GUID REPARSE_GUID = {
  0x1bf6205f, 0x46ae, 0x4527,
  { 0xb1, 0x0c, 0xc5, 0x09, 0xb7, 0x55, 0x22, 0x80 }};
#endif

static void check_permission(const char* filename, unsigned int mode) {
  int r;
  uv_fs_t req;
  uv_stat_t* s;

  r = uv_fs_stat(NULL, &req, filename, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);

  s = &req.statbuf;
#if defined(_WIN32) || defined(__CYGWIN__) || defined(__MSYS__)
  /*
   * On Windows, chmod can only modify S_IWUSR (_S_IWRITE) bit,
   * so only testing for the specified flags.
   */
  ASSERT((s->st_mode & 0777) & mode);
#else
  ASSERT((s->st_mode & 0777) == mode);
#endif

  uv_fs_req_cleanup(&req);
}


static void dummy_cb(uv_fs_t* req) {
  (void) req;
  dummy_cb_count++;
}


static void link_cb(uv_fs_t* req) {
  ASSERT(req->fs_type == UV_FS_LINK);
  ASSERT(req->result == 0);
  link_cb_count++;
  uv_fs_req_cleanup(req);
}


static void symlink_cb(uv_fs_t* req) {
  ASSERT(req->fs_type == UV_FS_SYMLINK);
  ASSERT(req->result == 0);
  symlink_cb_count++;
  uv_fs_req_cleanup(req);
}

static void readlink_cb(uv_fs_t* req) {
  ASSERT(req->fs_type == UV_FS_READLINK);
  ASSERT(req->result == 0);
  ASSERT(strcmp(req->ptr, "test_file_symlink2") == 0);
  readlink_cb_count++;
  uv_fs_req_cleanup(req);
}


static void realpath_cb(uv_fs_t* req) {
  char test_file_abs_buf[PATHMAX];
  size_t test_file_abs_size = sizeof(test_file_abs_buf);
  ASSERT(req->fs_type == UV_FS_REALPATH);
#ifdef _WIN32
  /*
   * Windows XP and Server 2003 don't support GetFinalPathNameByHandleW()
   */
  if (req->result == UV_ENOSYS) {
    realpath_cb_count++;
    uv_fs_req_cleanup(req);
    return;
  }
#endif
  ASSERT(req->result == 0);

  uv_cwd(test_file_abs_buf, &test_file_abs_size);
#ifdef _WIN32
  strcat(test_file_abs_buf, "\\test_file");
  ASSERT(stricmp(req->ptr, test_file_abs_buf) == 0);
#else
  strcat(test_file_abs_buf, "/test_file");
  ASSERT(strcmp(req->ptr, test_file_abs_buf) == 0);
#endif
  realpath_cb_count++;
  uv_fs_req_cleanup(req);
}


static void access_cb(uv_fs_t* req) {
  ASSERT(req->fs_type == UV_FS_ACCESS);
  access_cb_count++;
  uv_fs_req_cleanup(req);
}


static void fchmod_cb(uv_fs_t* req) {
  ASSERT(req->fs_type == UV_FS_FCHMOD);
  ASSERT(req->result == 0);
  fchmod_cb_count++;
  uv_fs_req_cleanup(req);
  check_permission("test_file", *(int*)req->data);
}


static void chmod_cb(uv_fs_t* req) {
  ASSERT(req->fs_type == UV_FS_CHMOD);
  ASSERT(req->result == 0);
  chmod_cb_count++;
  uv_fs_req_cleanup(req);
  check_permission("test_file", *(int*)req->data);
}


static void fchown_cb(uv_fs_t* req) {
  ASSERT(req->fs_type == UV_FS_FCHOWN);
  ASSERT(req->result == 0);
  fchown_cb_count++;
  uv_fs_req_cleanup(req);
}


static void chown_cb(uv_fs_t* req) {
  ASSERT(req->fs_type == UV_FS_CHOWN);
  ASSERT(req->result == 0);
  chown_cb_count++;
  uv_fs_req_cleanup(req);
}

static void lchown_cb(uv_fs_t* req) {
  ASSERT(req->fs_type == UV_FS_LCHOWN);
  ASSERT(req->result == 0);
  lchown_cb_count++;
  uv_fs_req_cleanup(req);
}

static void chown_root_cb(uv_fs_t* req) {
  ASSERT(req->fs_type == UV_FS_CHOWN);
#if defined(_WIN32) || defined(__MSYS__)
  /* On windows, chown is a no-op and always succeeds. */
  ASSERT(req->result == 0);
#else
  /* On unix, chown'ing the root directory is not allowed -
   * unless you're root, of course.
   */
  if (geteuid() == 0)
    ASSERT(req->result == 0);
  else
#   if defined(__CYGWIN__)
    /* On Cygwin, uid 0 is invalid (no root). */
    ASSERT(req->result == UV_EINVAL);
#   elif defined(__PASE__)
    /* On IBMi PASE, there is no root user. uid 0 is user qsecofr.
     * User may grant qsecofr's privileges, including changing 
     * the file's ownership to uid 0.
     */
    ASSERT(req->result == 0 || req->result == UV_EPERM);
#   else
    ASSERT(req->result == UV_EPERM);
#   endif
#endif
  chown_cb_count++;
  uv_fs_req_cleanup(req);
}

static void unlink_cb(uv_fs_t* req) {
  ASSERT(req == &unlink_req);
  ASSERT(req->fs_type == UV_FS_UNLINK);
  ASSERT(req->result == 0);
  unlink_cb_count++;
  uv_fs_req_cleanup(req);
}

static void fstat_cb(uv_fs_t* req) {
  uv_stat_t* s = req->ptr;
  ASSERT(req->fs_type == UV_FS_FSTAT);
  ASSERT(req->result == 0);
  ASSERT(s->st_size == sizeof(test_buf));
  uv_fs_req_cleanup(req);
  fstat_cb_count++;
}


static void statfs_cb(uv_fs_t* req) {
  uv_statfs_t* stats;

  ASSERT(req->fs_type == UV_FS_STATFS);
  ASSERT(req->result == 0);
  ASSERT(req->ptr != NULL);
  stats = req->ptr;

#if defined(_WIN32) || defined(__sun) || defined(_AIX) || defined(__MVS__) || \
  defined(__OpenBSD__) || defined(__NetBSD__)
  ASSERT(stats->f_type == 0);
#else
  ASSERT(stats->f_type > 0);
#endif

  ASSERT(stats->f_bsize > 0);
  ASSERT(stats->f_blocks > 0);
  ASSERT(stats->f_bfree <= stats->f_blocks);
  ASSERT(stats->f_bavail <= stats->f_bfree);

#ifdef _WIN32
  ASSERT(stats->f_files == 0);
  ASSERT(stats->f_ffree == 0);
#else
  /* There is no assertion for stats->f_files that makes sense, so ignore it. */
  ASSERT(stats->f_ffree <= stats->f_files);
#endif
  uv_fs_req_cleanup(req);
  ASSERT(req->ptr == NULL);
  statfs_cb_count++;
}


static void close_cb(uv_fs_t* req) {
  int r;
  ASSERT(req == &close_req);
  ASSERT(req->fs_type == UV_FS_CLOSE);
  ASSERT(req->result == 0);
  close_cb_count++;
  uv_fs_req_cleanup(req);
  if (close_cb_count == 3) {
    r = uv_fs_unlink(loop, &unlink_req, "test_file2", unlink_cb);
    ASSERT(r == 0);
  }
}


static void ftruncate_cb(uv_fs_t* req) {
  int r;
  ASSERT(req == &ftruncate_req);
  ASSERT(req->fs_type == UV_FS_FTRUNCATE);
  ASSERT(req->result == 0);
  ftruncate_cb_count++;
  uv_fs_req_cleanup(req);
  r = uv_fs_close(loop, &close_req, open_req1.result, close_cb);
  ASSERT(r == 0);
}

static void fail_cb(uv_fs_t* req) {
  FATAL("fail_cb should not have been called");
}

static void read_cb(uv_fs_t* req) {
  int r;
  ASSERT(req == &read_req);
  ASSERT(req->fs_type == UV_FS_READ);
  ASSERT(req->result >= 0);  /* FIXME(bnoordhuis) Check if requested size? */
  read_cb_count++;
  uv_fs_req_cleanup(req);
  if (read_cb_count == 1) {
    ASSERT(strcmp(buf, test_buf) == 0);
    r = uv_fs_ftruncate(loop, &ftruncate_req, open_req1.result, 7,
        ftruncate_cb);
  } else {
    ASSERT(strcmp(buf, "test-bu") == 0);
    r = uv_fs_close(loop, &close_req, open_req1.result, close_cb);
  }
  ASSERT(r == 0);
}


static void open_cb(uv_fs_t* req) {
  int r;
  ASSERT(req == &open_req1);
  ASSERT(req->fs_type == UV_FS_OPEN);
  if (req->result < 0) {
    fprintf(stderr, "async open error: %d\n", (int) req->result);
    ASSERT(0);
  }
  open_cb_count++;
  ASSERT(req->path);
  ASSERT(memcmp(req->path, "test_file2\0", 11) == 0);
  uv_fs_req_cleanup(req);
  memset(buf, 0, sizeof(buf));
  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(loop, &read_req, open_req1.result, &iov, 1, -1,
      read_cb);
  ASSERT(r == 0);
}


static void open_cb_simple(uv_fs_t* req) {
  ASSERT(req->fs_type == UV_FS_OPEN);
  if (req->result < 0) {
    fprintf(stderr, "async open error: %d\n", (int) req->result);
    ASSERT(0);
  }
  open_cb_count++;
  ASSERT(req->path);
  uv_fs_req_cleanup(req);
}


static void fsync_cb(uv_fs_t* req) {
  int r;
  ASSERT(req == &fsync_req);
  ASSERT(req->fs_type == UV_FS_FSYNC);
  ASSERT(req->result == 0);
  fsync_cb_count++;
  uv_fs_req_cleanup(req);
  r = uv_fs_close(loop, &close_req, open_req1.result, close_cb);
  ASSERT(r == 0);
}


static void fdatasync_cb(uv_fs_t* req) {
  int r;
  ASSERT(req == &fdatasync_req);
  ASSERT(req->fs_type == UV_FS_FDATASYNC);
  ASSERT(req->result == 0);
  fdatasync_cb_count++;
  uv_fs_req_cleanup(req);
  r = uv_fs_fsync(loop, &fsync_req, open_req1.result, fsync_cb);
  ASSERT(r == 0);
}


static void write_cb(uv_fs_t* req) {
  int r;
  ASSERT(req == &write_req);
  ASSERT(req->fs_type == UV_FS_WRITE);
  ASSERT(req->result >= 0);  /* FIXME(bnoordhuis) Check if requested size? */
  write_cb_count++;
  uv_fs_req_cleanup(req);
  r = uv_fs_fdatasync(loop, &fdatasync_req, open_req1.result, fdatasync_cb);
  ASSERT(r == 0);
}


static void create_cb(uv_fs_t* req) {
  int r;
  ASSERT(req == &open_req1);
  ASSERT(req->fs_type == UV_FS_OPEN);
  ASSERT(req->result >= 0);
  create_cb_count++;
  uv_fs_req_cleanup(req);
  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = uv_fs_write(loop, &write_req, req->result, &iov, 1, -1, write_cb);
  ASSERT(r == 0);
}


static void rename_cb(uv_fs_t* req) {
  ASSERT(req == &rename_req);
  ASSERT(req->fs_type == UV_FS_RENAME);
  ASSERT(req->result == 0);
  rename_cb_count++;
  uv_fs_req_cleanup(req);
}


static void mkdir_cb(uv_fs_t* req) {
  ASSERT(req == &mkdir_req);
  ASSERT(req->fs_type == UV_FS_MKDIR);
  ASSERT(req->result == 0);
  mkdir_cb_count++;
  ASSERT(req->path);
  ASSERT(memcmp(req->path, "test_dir\0", 9) == 0);
  uv_fs_req_cleanup(req);
}


static void check_mkdtemp_result(uv_fs_t* req) {
  int r;

  ASSERT(req->fs_type == UV_FS_MKDTEMP);
  ASSERT(req->result == 0);
  ASSERT(req->path);
  ASSERT(strlen(req->path) == 15);
  ASSERT(memcmp(req->path, "test_dir_", 9) == 0);
  ASSERT(memcmp(req->path + 9, "XXXXXX", 6) != 0);
  check_permission(req->path, 0700);

  /* Check if req->path is actually a directory */
  r = uv_fs_stat(NULL, &stat_req, req->path, NULL);
  ASSERT(r == 0);
  ASSERT(((uv_stat_t*)stat_req.ptr)->st_mode & S_IFDIR);
  uv_fs_req_cleanup(&stat_req);
}


static void mkdtemp_cb(uv_fs_t* req) {
  ASSERT(req == &mkdtemp_req1);
  check_mkdtemp_result(req);
  mkdtemp_cb_count++;
}


static void check_mkstemp_result(uv_fs_t* req) {
  int r;

  ASSERT(req->fs_type == UV_FS_MKSTEMP);
  ASSERT(req->result >= 0);
  ASSERT(req->path);
  ASSERT(strlen(req->path) == 16);
  ASSERT(memcmp(req->path, "test_file_", 10) == 0);
  ASSERT(memcmp(req->path + 10, "XXXXXX", 6) != 0);
  check_permission(req->path, 0600);

  /* Check if req->path is actually a file */
  r = uv_fs_stat(NULL, &stat_req, req->path, NULL);
  ASSERT(r == 0);
  ASSERT(stat_req.statbuf.st_mode & S_IFREG);
  uv_fs_req_cleanup(&stat_req);
}


static void mkstemp_cb(uv_fs_t* req) {
  ASSERT(req == &mkstemp_req1);
  check_mkstemp_result(req);
  mkstemp_cb_count++;
}


static void rmdir_cb(uv_fs_t* req) {
  ASSERT(req == &rmdir_req);
  ASSERT(req->fs_type == UV_FS_RMDIR);
  ASSERT(req->result == 0);
  rmdir_cb_count++;
  ASSERT(req->path);
  ASSERT(memcmp(req->path, "test_dir\0", 9) == 0);
  uv_fs_req_cleanup(req);
}


static void assert_is_file_type(uv_dirent_t dent) {
#ifdef HAVE_DIRENT_TYPES
  /*
   * For Apple and Windows, we know getdents is expected to work but for other
   * environments, the filesystem dictates whether or not getdents supports
   * returning the file type.
   *
   *   See:
   *     http://man7.org/linux/man-pages/man2/getdents.2.html
   *     https://github.com/libuv/libuv/issues/501
   */
  #if defined(__APPLE__) || defined(_WIN32)
    ASSERT(dent.type == UV_DIRENT_FILE);
  #else
    ASSERT(dent.type == UV_DIRENT_FILE || dent.type == UV_DIRENT_UNKNOWN);
  #endif
#else
  ASSERT(dent.type == UV_DIRENT_UNKNOWN);
#endif
}


static void scandir_cb(uv_fs_t* req) {
  uv_dirent_t dent;
  ASSERT(req == &scandir_req);
  ASSERT(req->fs_type == UV_FS_SCANDIR);
  ASSERT(req->result == 2);
  ASSERT(req->ptr);

  while (UV_EOF != uv_fs_scandir_next(req, &dent)) {
    ASSERT(strcmp(dent.name, "file1") == 0 || strcmp(dent.name, "file2") == 0);
    assert_is_file_type(dent);
  }
  scandir_cb_count++;
  ASSERT(req->path);
  ASSERT(memcmp(req->path, "test_dir\0", 9) == 0);
  uv_fs_req_cleanup(req);
  ASSERT(!req->ptr);
}


static void empty_scandir_cb(uv_fs_t* req) {
  uv_dirent_t dent;

  ASSERT(req == &scandir_req);
  ASSERT(req->fs_type == UV_FS_SCANDIR);
  ASSERT(req->result == 0);
  ASSERT(req->ptr == NULL);
  ASSERT(UV_EOF == uv_fs_scandir_next(req, &dent));
  uv_fs_req_cleanup(req);
  scandir_cb_count++;
}

static void non_existent_scandir_cb(uv_fs_t* req) {
  uv_dirent_t dent;

  ASSERT(req == &scandir_req);
  ASSERT(req->fs_type == UV_FS_SCANDIR);
  ASSERT(req->result == UV_ENOENT);
  ASSERT(req->ptr == NULL);
  ASSERT(UV_ENOENT == uv_fs_scandir_next(req, &dent));
  uv_fs_req_cleanup(req);
  scandir_cb_count++;
}


static void file_scandir_cb(uv_fs_t* req) {
  ASSERT(req == &scandir_req);
  ASSERT(req->fs_type == UV_FS_SCANDIR);
  ASSERT(req->result == UV_ENOTDIR);
  ASSERT(req->ptr == NULL);
  uv_fs_req_cleanup(req);
  scandir_cb_count++;
}


static void stat_cb(uv_fs_t* req) {
  ASSERT(req == &stat_req);
  ASSERT(req->fs_type == UV_FS_STAT || req->fs_type == UV_FS_LSTAT);
  ASSERT(req->result == 0);
  ASSERT(req->ptr);
  stat_cb_count++;
  uv_fs_req_cleanup(req);
  ASSERT(!req->ptr);
}


static void sendfile_cb(uv_fs_t* req) {
  ASSERT(req == &sendfile_req);
  ASSERT(req->fs_type == UV_FS_SENDFILE);
  ASSERT(req->result == 65545);
  sendfile_cb_count++;
  uv_fs_req_cleanup(req);
}


static void sendfile_nodata_cb(uv_fs_t* req) {
  ASSERT(req == &sendfile_req);
  ASSERT(req->fs_type == UV_FS_SENDFILE);
  ASSERT(req->result == 0);
  sendfile_cb_count++;
  uv_fs_req_cleanup(req);
}


static void open_noent_cb(uv_fs_t* req) {
  ASSERT(req->fs_type == UV_FS_OPEN);
  ASSERT(req->result == UV_ENOENT);
  open_cb_count++;
  uv_fs_req_cleanup(req);
}

static void open_nametoolong_cb(uv_fs_t* req) {
  ASSERT(req->fs_type == UV_FS_OPEN);
  ASSERT(req->result == UV_ENAMETOOLONG);
  open_cb_count++;
  uv_fs_req_cleanup(req);
}

static void open_loop_cb(uv_fs_t* req) {
  ASSERT(req->fs_type == UV_FS_OPEN);
  ASSERT(req->result == UV_ELOOP);
  open_cb_count++;
  uv_fs_req_cleanup(req);
}


TEST_IMPL(fs_file_noent) {
  uv_fs_t req;
  int r;

  loop = uv_default_loop();

  r = uv_fs_open(NULL, &req, "does_not_exist", O_RDONLY, 0, NULL);
  ASSERT(r == UV_ENOENT);
  ASSERT(req.result == UV_ENOENT);
  uv_fs_req_cleanup(&req);

  r = uv_fs_open(loop, &req, "does_not_exist", O_RDONLY, 0, open_noent_cb);
  ASSERT(r == 0);

  ASSERT(open_cb_count == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(open_cb_count == 1);

  /* TODO add EACCES test */

  MAKE_VALGRIND_HAPPY();
  return 0;
}

TEST_IMPL(fs_file_nametoolong) {
  uv_fs_t req;
  int r;
  char name[TOO_LONG_NAME_LENGTH + 1];

  loop = uv_default_loop();

  memset(name, 'a', TOO_LONG_NAME_LENGTH);
  name[TOO_LONG_NAME_LENGTH] = 0;

  r = uv_fs_open(NULL, &req, name, O_RDONLY, 0, NULL);
  ASSERT(r == UV_ENAMETOOLONG);
  ASSERT(req.result == UV_ENAMETOOLONG);
  uv_fs_req_cleanup(&req);

  r = uv_fs_open(loop, &req, name, O_RDONLY, 0, open_nametoolong_cb);
  ASSERT(r == 0);

  ASSERT(open_cb_count == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(open_cb_count == 1);

  MAKE_VALGRIND_HAPPY();
  return 0;
}

TEST_IMPL(fs_file_loop) {
  uv_fs_t req;
  int r;

  loop = uv_default_loop();

  unlink("test_symlink");
  r = uv_fs_symlink(NULL, &req, "test_symlink", "test_symlink", 0, NULL);
#ifdef _WIN32
  /*
   * Windows XP and Server 2003 don't support symlinks; we'll get UV_ENOTSUP.
   * Starting with vista they are supported, but only when elevated, otherwise
   * we'll see UV_EPERM.
   */
  if (r == UV_ENOTSUP || r == UV_EPERM)
    return 0;
#elif defined(__MSYS__)
  /* MSYS2's approximation of symlinks with copies does not work for broken
     links.  */
  if (r == UV_ENOENT)
    return 0;
#endif
  ASSERT(r == 0);
  uv_fs_req_cleanup(&req);

  r = uv_fs_open(NULL, &req, "test_symlink", O_RDONLY, 0, NULL);
  ASSERT(r == UV_ELOOP);
  ASSERT(req.result == UV_ELOOP);
  uv_fs_req_cleanup(&req);

  r = uv_fs_open(loop, &req, "test_symlink", O_RDONLY, 0, open_loop_cb);
  ASSERT(r == 0);

  ASSERT(open_cb_count == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(open_cb_count == 1);

  unlink("test_symlink");

  MAKE_VALGRIND_HAPPY();
  return 0;
}

static void check_utime(const char* path,
                        double atime,
                        double mtime,
                        int test_lutime) {
  uv_stat_t* s;
  uv_fs_t req;
  int r;

  if (test_lutime)
    r = uv_fs_lstat(loop, &req, path, NULL);
  else
    r = uv_fs_stat(loop, &req, path, NULL);

  ASSERT_EQ(r, 0);

  ASSERT_EQ(req.result, 0);
  s = &req.statbuf;

  if (s->st_atim.tv_nsec == 0 && s->st_mtim.tv_nsec == 0) {
    /*
     * Test sub-second timestamps only when supported (such as Windows with
     * NTFS). Some other platforms support sub-second timestamps, but that
     * support is filesystem-dependent. Notably OS X (HFS Plus) does NOT
     * support sub-second timestamps. But kernels may round or truncate in
     * either direction, so we may accept either possible answer.
     */
#ifdef _WIN32
    ASSERT_DOUBLE_EQ(atime, (long) atime);
    ASSERT_DOUBLE_EQ(mtime, (long) atime);
#endif
    if (atime > 0 || (long) atime == atime)
      ASSERT_EQ(s->st_atim.tv_sec, (long) atime);
    if (mtime > 0 || (long) mtime == mtime)
      ASSERT_EQ(s->st_mtim.tv_sec, (long) mtime);
    ASSERT_GE(s->st_atim.tv_sec, (long) atime - 1);
    ASSERT_GE(s->st_mtim.tv_sec, (long) mtime - 1);
    ASSERT_LE(s->st_atim.tv_sec, (long) atime);
    ASSERT_LE(s->st_mtim.tv_sec, (long) mtime);
  } else {
    double st_atim;
    double st_mtim;
#ifndef __APPLE__
    /* TODO(vtjnash): would it be better to normalize this? */
    ASSERT_DOUBLE_GE(s->st_atim.tv_nsec, 0);
    ASSERT_DOUBLE_GE(s->st_mtim.tv_nsec, 0);
#endif
    st_atim = s->st_atim.tv_sec + s->st_atim.tv_nsec / 1e9;
    st_mtim = s->st_mtim.tv_sec + s->st_mtim.tv_nsec / 1e9;
    ASSERT_DOUBLE_EQ(st_atim, atime);
    ASSERT_DOUBLE_EQ(st_mtim, mtime);
  }

  uv_fs_req_cleanup(&req);
}


static void utime_cb(uv_fs_t* req) {
  utime_check_t* c;

  ASSERT(req == &utime_req);
  ASSERT(req->result == 0);
  ASSERT(req->fs_type == UV_FS_UTIME);

  c = req->data;
  check_utime(c->path, c->atime, c->mtime, /* test_lutime */ 0);

  uv_fs_req_cleanup(req);
  utime_cb_count++;
}


static void futime_cb(uv_fs_t* req) {
  utime_check_t* c;

  ASSERT(req == &futime_req);
  ASSERT(req->result == 0);
  ASSERT(req->fs_type == UV_FS_FUTIME);

  c = req->data;
  check_utime(c->path, c->atime, c->mtime, /* test_lutime */ 0);

  uv_fs_req_cleanup(req);
  futime_cb_count++;
}


static void lutime_cb(uv_fs_t* req) {
  utime_check_t* c;

  ASSERT(req->result == 0);
  ASSERT(req->fs_type == UV_FS_LUTIME);

  c = req->data;
  check_utime(c->path, c->atime, c->mtime, /* test_lutime */ 1);

  uv_fs_req_cleanup(req);
  lutime_cb_count++;
}


TEST_IMPL(fs_file_async) {
  int r;

  /* Setup. */
  unlink("test_file");
  unlink("test_file2");

  loop = uv_default_loop();

  r = uv_fs_open(loop, &open_req1, "test_file", O_WRONLY | O_CREAT,
      S_IRUSR | S_IWUSR, create_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);

  ASSERT(create_cb_count == 1);
  ASSERT(write_cb_count == 1);
  ASSERT(fsync_cb_count == 1);
  ASSERT(fdatasync_cb_count == 1);
  ASSERT(close_cb_count == 1);

  r = uv_fs_rename(loop, &rename_req, "test_file", "test_file2", rename_cb);
  ASSERT(r == 0);

  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(create_cb_count == 1);
  ASSERT(write_cb_count == 1);
  ASSERT(close_cb_count == 1);
  ASSERT(rename_cb_count == 1);

  r = uv_fs_open(loop, &open_req1, "test_file2", O_RDWR, 0, open_cb);
  ASSERT(r == 0);

  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(open_cb_count == 1);
  ASSERT(read_cb_count == 1);
  ASSERT(close_cb_count == 2);
  ASSERT(rename_cb_count == 1);
  ASSERT(create_cb_count == 1);
  ASSERT(write_cb_count == 1);
  ASSERT(ftruncate_cb_count == 1);

  r = uv_fs_open(loop, &open_req1, "test_file2", O_RDONLY, 0, open_cb);
  ASSERT(r == 0);

  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(open_cb_count == 2);
  ASSERT(read_cb_count == 2);
  ASSERT(close_cb_count == 3);
  ASSERT(rename_cb_count == 1);
  ASSERT(unlink_cb_count == 1);
  ASSERT(create_cb_count == 1);
  ASSERT(write_cb_count == 1);
  ASSERT(ftruncate_cb_count == 1);

  /* Cleanup. */
  unlink("test_file");
  unlink("test_file2");

  MAKE_VALGRIND_HAPPY();
  return 0;
}


static void fs_file_sync(int add_flags) {
  int r;

  /* Setup. */
  unlink("test_file");
  unlink("test_file2");

  loop = uv_default_loop();

  r = uv_fs_open(loop, &open_req1, "test_file",
      O_WRONLY | O_CREAT | add_flags, S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = uv_fs_write(NULL, &write_req, open_req1.result, &iov, 1, -1, NULL);
  ASSERT(r >= 0);
  ASSERT(write_req.result >= 0);
  uv_fs_req_cleanup(&write_req);

  r = uv_fs_close(NULL, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_open(NULL, &open_req1, "test_file", O_RDWR | add_flags, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(NULL, &read_req, open_req1.result, &iov, 1, -1, NULL);
  ASSERT(r >= 0);
  ASSERT(read_req.result >= 0);
  ASSERT(strcmp(buf, test_buf) == 0);
  uv_fs_req_cleanup(&read_req);

  r = uv_fs_ftruncate(NULL, &ftruncate_req, open_req1.result, 7, NULL);
  ASSERT(r == 0);
  ASSERT(ftruncate_req.result == 0);
  uv_fs_req_cleanup(&ftruncate_req);

  r = uv_fs_close(NULL, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_rename(NULL, &rename_req, "test_file", "test_file2", NULL);
  ASSERT(r == 0);
  ASSERT(rename_req.result == 0);
  uv_fs_req_cleanup(&rename_req);

  r = uv_fs_open(NULL, &open_req1, "test_file2", O_RDONLY | add_flags, 0,
      NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  memset(buf, 0, sizeof(buf));
  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(NULL, &read_req, open_req1.result, &iov, 1, -1, NULL);
  ASSERT(r >= 0);
  ASSERT(read_req.result >= 0);
  ASSERT(strcmp(buf, "test-bu") == 0);
  uv_fs_req_cleanup(&read_req);

  r = uv_fs_close(NULL, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_unlink(NULL, &unlink_req, "test_file2", NULL);
  ASSERT(r == 0);
  ASSERT(unlink_req.result == 0);
  uv_fs_req_cleanup(&unlink_req);

  /* Cleanup */
  unlink("test_file");
  unlink("test_file2");
}
TEST_IMPL(fs_file_sync) {
  fs_file_sync(0);
  fs_file_sync(UV_FS_O_FILEMAP);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


static void fs_file_write_null_buffer(int add_flags) {
  int r;

  /* Setup. */
  unlink("test_file");

  loop = uv_default_loop();

  r = uv_fs_open(NULL, &open_req1, "test_file",
      O_WRONLY | O_CREAT | add_flags, S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  iov = uv_buf_init(NULL, 0);
  r = uv_fs_write(NULL, &write_req, open_req1.result, &iov, 1, -1, NULL);
  ASSERT(r == 0);
  ASSERT(write_req.result == 0);
  uv_fs_req_cleanup(&write_req);

  r = uv_fs_close(NULL, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  unlink("test_file");
}
TEST_IMPL(fs_file_write_null_buffer) {
  fs_file_write_null_buffer(0);
  fs_file_write_null_buffer(UV_FS_O_FILEMAP);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(fs_async_dir) {
  int r;
  uv_dirent_t dent;

  /* Setup */
  unlink("test_dir/file1");
  unlink("test_dir/file2");
  rmdir("test_dir");

  loop = uv_default_loop();

  r = uv_fs_mkdir(loop, &mkdir_req, "test_dir", 0755, mkdir_cb);
  ASSERT(r == 0);

  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(mkdir_cb_count == 1);

  /* Create 2 files synchronously. */
  r = uv_fs_open(NULL, &open_req1, "test_dir/file1", O_WRONLY | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  uv_fs_req_cleanup(&open_req1);
  r = uv_fs_close(NULL, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_open(NULL, &open_req1, "test_dir/file2", O_WRONLY | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  uv_fs_req_cleanup(&open_req1);
  r = uv_fs_close(NULL, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_scandir(loop, &scandir_req, "test_dir", 0, scandir_cb);
  ASSERT(r == 0);

  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(scandir_cb_count == 1);

  /* sync uv_fs_scandir */
  r = uv_fs_scandir(NULL, &scandir_req, "test_dir", 0, NULL);
  ASSERT(r == 2);
  ASSERT(scandir_req.result == 2);
  ASSERT(scandir_req.ptr);
  while (UV_EOF != uv_fs_scandir_next(&scandir_req, &dent)) {
    ASSERT(strcmp(dent.name, "file1") == 0 || strcmp(dent.name, "file2") == 0);
    assert_is_file_type(dent);
  }
  uv_fs_req_cleanup(&scandir_req);
  ASSERT(!scandir_req.ptr);

  r = uv_fs_stat(loop, &stat_req, "test_dir", stat_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);

  r = uv_fs_stat(loop, &stat_req, "test_dir/", stat_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);

  r = uv_fs_lstat(loop, &stat_req, "test_dir", stat_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);

  r = uv_fs_lstat(loop, &stat_req, "test_dir/", stat_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);

  ASSERT(stat_cb_count == 4);

  r = uv_fs_unlink(loop, &unlink_req, "test_dir/file1", unlink_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(unlink_cb_count == 1);

  r = uv_fs_unlink(loop, &unlink_req, "test_dir/file2", unlink_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(unlink_cb_count == 2);

  r = uv_fs_rmdir(loop, &rmdir_req, "test_dir", rmdir_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(rmdir_cb_count == 1);

  /* Cleanup */
  unlink("test_dir/file1");
  unlink("test_dir/file2");
  rmdir("test_dir");

  MAKE_VALGRIND_HAPPY();
  return 0;
}


static int test_sendfile(void (*setup)(int), uv_fs_cb cb, off_t expected_size) {
  int f, r;
  struct stat s1, s2;
  uv_fs_t req;
  char buf1[1];

  loop = uv_default_loop();

  /* Setup. */
  unlink("test_file");
  unlink("test_file2");

  f = open("test_file", O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
  ASSERT(f != -1);

  if (setup != NULL)
    setup(f);

  r = close(f);
  ASSERT(r == 0);

  /* Test starts here. */
  r = uv_fs_open(NULL, &open_req1, "test_file", O_RDWR, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  r = uv_fs_open(NULL, &open_req2, "test_file2", O_WRONLY | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req2.result >= 0);
  uv_fs_req_cleanup(&open_req2);

  r = uv_fs_sendfile(loop, &sendfile_req, open_req2.result, open_req1.result,
      1, 131072, cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);

  ASSERT(sendfile_cb_count == 1);

  r = uv_fs_close(NULL, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&close_req);
  r = uv_fs_close(NULL, &close_req, open_req2.result, NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&close_req);

  ASSERT(0 == stat("test_file", &s1));
  ASSERT(0 == stat("test_file2", &s2));
  ASSERT(s2.st_size == expected_size);

  if (expected_size > 0) {
    ASSERT_UINT64_EQ(s1.st_size, s2.st_size + 1);
    r = uv_fs_open(NULL, &open_req1, "test_file2", O_RDWR, 0, NULL);
    ASSERT(r >= 0);
    ASSERT(open_req1.result >= 0);
    uv_fs_req_cleanup(&open_req1);

    memset(buf1, 0, sizeof(buf1));
    iov = uv_buf_init(buf1, sizeof(buf1));
    r = uv_fs_read(NULL, &req, open_req1.result, &iov, 1, -1, NULL);
    ASSERT(r >= 0);
    ASSERT(req.result >= 0);
    ASSERT_EQ(buf1[0], 'e'); /* 'e' from begin */
    uv_fs_req_cleanup(&req);
  } else {
    ASSERT_UINT64_EQ(s1.st_size, s2.st_size);
  }

  /* Cleanup. */
  unlink("test_file");
  unlink("test_file2");

  MAKE_VALGRIND_HAPPY();
  return 0;
}


static void sendfile_setup(int f) {
  ASSERT(6 == write(f, "begin\n", 6));
  ASSERT(65542 == lseek(f, 65536, SEEK_CUR));
  ASSERT(4 == write(f, "end\n", 4));
}


TEST_IMPL(fs_async_sendfile) {
  return test_sendfile(sendfile_setup, sendfile_cb, 65545);
}


TEST_IMPL(fs_async_sendfile_nodata) {
  return test_sendfile(NULL, sendfile_nodata_cb, 0);
}


TEST_IMPL(fs_mkdtemp) {
  int r;
  const char* path_template = "test_dir_XXXXXX";

  loop = uv_default_loop();

  r = uv_fs_mkdtemp(loop, &mkdtemp_req1, path_template, mkdtemp_cb);
  ASSERT(r == 0);

  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(mkdtemp_cb_count == 1);

  /* sync mkdtemp */
  r = uv_fs_mkdtemp(NULL, &mkdtemp_req2, path_template, NULL);
  ASSERT(r == 0);
  check_mkdtemp_result(&mkdtemp_req2);

  /* mkdtemp return different values on subsequent calls */
  ASSERT(strcmp(mkdtemp_req1.path, mkdtemp_req2.path) != 0);

  /* Cleanup */
  rmdir(mkdtemp_req1.path);
  rmdir(mkdtemp_req2.path);
  uv_fs_req_cleanup(&mkdtemp_req1);
  uv_fs_req_cleanup(&mkdtemp_req2);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(fs_mkstemp) {
  int r;
  int fd;
  const char path_template[] = "test_file_XXXXXX";
  uv_fs_t req;

  loop = uv_default_loop();

  r = uv_fs_mkstemp(loop, &mkstemp_req1, path_template, mkstemp_cb);
  ASSERT(r == 0);

  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(mkstemp_cb_count == 1);

  /* sync mkstemp */
  r = uv_fs_mkstemp(NULL, &mkstemp_req2, path_template, NULL);
  ASSERT(r >= 0);
  check_mkstemp_result(&mkstemp_req2);

  /* mkstemp return different values on subsequent calls */
  ASSERT(strcmp(mkstemp_req1.path, mkstemp_req2.path) != 0);

  /* invalid template returns EINVAL */
  ASSERT_EQ(UV_EINVAL, uv_fs_mkstemp(NULL, &mkstemp_req3, "test_file", NULL));

  /* Make sure that path is empty string */
  ASSERT_EQ(0, strlen(mkstemp_req3.path));

  /* We can write to the opened file */
  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = uv_fs_write(NULL, &req, mkstemp_req1.result, &iov, 1, -1, NULL);
  ASSERT(r == sizeof(test_buf));
  ASSERT(req.result == sizeof(test_buf));
  uv_fs_req_cleanup(&req);

  /* Cleanup */
  uv_fs_close(NULL, &req, mkstemp_req1.result, NULL);
  uv_fs_req_cleanup(&req);
  uv_fs_close(NULL, &req, mkstemp_req2.result, NULL);
  uv_fs_req_cleanup(&req);

  fd = uv_fs_open(NULL, &req, mkstemp_req1.path , O_RDONLY, 0, NULL);
  ASSERT(fd >= 0);
  uv_fs_req_cleanup(&req);

  memset(buf, 0, sizeof(buf));
  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(NULL, &req, fd, &iov, 1, -1, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  ASSERT(strcmp(buf, test_buf) == 0);
  uv_fs_req_cleanup(&req);

  uv_fs_close(NULL, &req, fd, NULL);
  uv_fs_req_cleanup(&req);

  unlink(mkstemp_req1.path);
  unlink(mkstemp_req2.path);
  uv_fs_req_cleanup(&mkstemp_req1);
  uv_fs_req_cleanup(&mkstemp_req2);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(fs_fstat) {
  int r;
  uv_fs_t req;
  uv_file file;
  uv_stat_t* s;
#ifndef _WIN32
  struct stat t;
#endif

  /* Setup. */
  unlink("test_file");

  loop = uv_default_loop();

  r = uv_fs_open(NULL, &req, "test_file", O_RDWR | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  file = req.result;
  uv_fs_req_cleanup(&req);

#ifndef _WIN32
  ASSERT(0 == fstat(file, &t));
  ASSERT(0 == uv_fs_fstat(NULL, &req, file, NULL));
  ASSERT(req.result == 0);
  s = req.ptr;
# if defined(__APPLE__)
  ASSERT(s->st_birthtim.tv_sec == t.st_birthtimespec.tv_sec);
  ASSERT(s->st_birthtim.tv_nsec == t.st_birthtimespec.tv_nsec);
# elif defined(__linux__)
  /* If statx() is supported, the birth time should be equal to the change time
   * because we just created the file. On older kernels, it's set to zero.
   */
  ASSERT(s->st_birthtim.tv_sec == 0 ||
         s->st_birthtim.tv_sec == t.st_ctim.tv_sec);
  ASSERT(s->st_birthtim.tv_nsec == 0 ||
         s->st_birthtim.tv_nsec == t.st_ctim.tv_nsec);
# endif
#endif

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = uv_fs_write(NULL, &req, file, &iov, 1, -1, NULL);
  ASSERT(r == sizeof(test_buf));
  ASSERT(req.result == sizeof(test_buf));
  uv_fs_req_cleanup(&req);

  memset(&req.statbuf, 0xaa, sizeof(req.statbuf));
  r = uv_fs_fstat(NULL, &req, file, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  s = req.ptr;
  ASSERT(s->st_size == sizeof(test_buf));

#ifndef _WIN32
  r = fstat(file, &t);
  ASSERT(r == 0);

  ASSERT(s->st_dev == (uint64_t) t.st_dev);
  ASSERT(s->st_mode == (uint64_t) t.st_mode);
  ASSERT(s->st_nlink == (uint64_t) t.st_nlink);
  ASSERT(s->st_uid == (uint64_t) t.st_uid);
  ASSERT(s->st_gid == (uint64_t) t.st_gid);
  ASSERT(s->st_rdev == (uint64_t) t.st_rdev);
  ASSERT(s->st_ino == (uint64_t) t.st_ino);
  ASSERT(s->st_size == (uint64_t) t.st_size);
  ASSERT(s->st_blksize == (uint64_t) t.st_blksize);
  ASSERT(s->st_blocks == (uint64_t) t.st_blocks);
#if defined(__APPLE__)
  ASSERT(s->st_atim.tv_sec == t.st_atimespec.tv_sec);
  ASSERT(s->st_atim.tv_nsec == t.st_atimespec.tv_nsec);
  ASSERT(s->st_mtim.tv_sec == t.st_mtimespec.tv_sec);
  ASSERT(s->st_mtim.tv_nsec == t.st_mtimespec.tv_nsec);
  ASSERT(s->st_ctim.tv_sec == t.st_ctimespec.tv_sec);
  ASSERT(s->st_ctim.tv_nsec == t.st_ctimespec.tv_nsec);
#elif defined(_AIX)
  ASSERT(s->st_atim.tv_sec == t.st_atime);
  ASSERT(s->st_atim.tv_nsec == 0);
  ASSERT(s->st_mtim.tv_sec == t.st_mtime);
  ASSERT(s->st_mtim.tv_nsec == 0);
  ASSERT(s->st_ctim.tv_sec == t.st_ctime);
  ASSERT(s->st_ctim.tv_nsec == 0);
#elif defined(__ANDROID__)
  ASSERT(s->st_atim.tv_sec == t.st_atime);
  ASSERT(s->st_atim.tv_nsec == t.st_atimensec);
  ASSERT(s->st_mtim.tv_sec == t.st_mtime);
  ASSERT(s->st_mtim.tv_nsec == t.st_mtimensec);
  ASSERT(s->st_ctim.tv_sec == t.st_ctime);
  ASSERT(s->st_ctim.tv_nsec == t.st_ctimensec);
#elif defined(__sun)           || \
      defined(__DragonFly__)   || \
      defined(__FreeBSD__)     || \
      defined(__OpenBSD__)     || \
      defined(__NetBSD__)      || \
      defined(_GNU_SOURCE)     || \
      defined(_BSD_SOURCE)     || \
      defined(_SVID_SOURCE)    || \
      defined(_XOPEN_SOURCE)   || \
      defined(_DEFAULT_SOURCE)
  ASSERT(s->st_atim.tv_sec == t.st_atim.tv_sec);
  ASSERT(s->st_atim.tv_nsec == t.st_atim.tv_nsec);
  ASSERT(s->st_mtim.tv_sec == t.st_mtim.tv_sec);
  ASSERT(s->st_mtim.tv_nsec == t.st_mtim.tv_nsec);
  ASSERT(s->st_ctim.tv_sec == t.st_ctim.tv_sec);
  ASSERT(s->st_ctim.tv_nsec == t.st_ctim.tv_nsec);
# if defined(__FreeBSD__)    || \
     defined(__NetBSD__)
  ASSERT(s->st_birthtim.tv_sec == t.st_birthtim.tv_sec);
  ASSERT(s->st_birthtim.tv_nsec == t.st_birthtim.tv_nsec);
# endif
#else
  ASSERT(s->st_atim.tv_sec == t.st_atime);
  ASSERT(s->st_atim.tv_nsec == 0);
  ASSERT(s->st_mtim.tv_sec == t.st_mtime);
  ASSERT(s->st_mtim.tv_nsec == 0);
  ASSERT(s->st_ctim.tv_sec == t.st_ctime);
  ASSERT(s->st_ctim.tv_nsec == 0);
#endif
#endif

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__)
  ASSERT(s->st_flags == t.st_flags);
  ASSERT(s->st_gen == t.st_gen);
#else
  ASSERT(s->st_flags == 0);
  ASSERT(s->st_gen == 0);
#endif

  uv_fs_req_cleanup(&req);

  /* Now do the uv_fs_fstat call asynchronously */
  r = uv_fs_fstat(loop, &req, file, fstat_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(fstat_cb_count == 1);


  r = uv_fs_close(NULL, &req, file, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  uv_fs_req_cleanup(&req);

  /*
   * Run the loop just to check we don't have make any extraneous uv_ref()
   * calls. This should drop out immediately.
   */
  uv_run(loop, UV_RUN_DEFAULT);

  /* Cleanup. */
  unlink("test_file");

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(fs_access) {
  int r;
  uv_fs_t req;
  uv_file file;

  /* Setup. */
  unlink("test_file");
  rmdir("test_dir");

  loop = uv_default_loop();

  /* File should not exist */
  r = uv_fs_access(NULL, &req, "test_file", F_OK, NULL);
  ASSERT(r < 0);
  ASSERT(req.result < 0);
  uv_fs_req_cleanup(&req);

  /* File should not exist */
  r = uv_fs_access(loop, &req, "test_file", F_OK, access_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(access_cb_count == 1);
  access_cb_count = 0; /* reset for the next test */

  /* Create file */
  r = uv_fs_open(NULL, &req, "test_file", O_RDWR | O_CREAT,
                 S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  file = req.result;
  uv_fs_req_cleanup(&req);

  /* File should exist */
  r = uv_fs_access(NULL, &req, "test_file", F_OK, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  uv_fs_req_cleanup(&req);

  /* File should exist */
  r = uv_fs_access(loop, &req, "test_file", F_OK, access_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(access_cb_count == 1);
  access_cb_count = 0; /* reset for the next test */

  /* Close file */
  r = uv_fs_close(NULL, &req, file, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  uv_fs_req_cleanup(&req);

  /* Directory access */
  r = uv_fs_mkdir(NULL, &req, "test_dir", 0777, NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&req);

  r = uv_fs_access(NULL, &req, "test_dir", W_OK, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  uv_fs_req_cleanup(&req);

  /*
   * Run the loop just to check we don't have make any extraneous uv_ref()
   * calls. This should drop out immediately.
   */
  uv_run(loop, UV_RUN_DEFAULT);

  /* Cleanup. */
  unlink("test_file");
  rmdir("test_dir");

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(fs_chmod) {
  int r;
  uv_fs_t req;
  uv_file file;

  /* Setup. */
  unlink("test_file");

  loop = uv_default_loop();

  r = uv_fs_open(NULL, &req, "test_file", O_RDWR | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  file = req.result;
  uv_fs_req_cleanup(&req);

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = uv_fs_write(NULL, &req, file, &iov, 1, -1, NULL);
  ASSERT(r == sizeof(test_buf));
  ASSERT(req.result == sizeof(test_buf));
  uv_fs_req_cleanup(&req);

#ifndef _WIN32
  /* Make the file write-only */
  r = uv_fs_chmod(NULL, &req, "test_file", 0200, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  uv_fs_req_cleanup(&req);

  check_permission("test_file", 0200);
#endif

  /* Make the file read-only */
  r = uv_fs_chmod(NULL, &req, "test_file", 0400, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  uv_fs_req_cleanup(&req);

  check_permission("test_file", 0400);

  /* Make the file read+write with sync uv_fs_fchmod */
  r = uv_fs_fchmod(NULL, &req, file, 0600, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  uv_fs_req_cleanup(&req);

  check_permission("test_file", 0600);

#ifndef _WIN32
  /* async chmod */
  {
    static int mode = 0200;
    req.data = &mode;
  }
  r = uv_fs_chmod(loop, &req, "test_file", 0200, chmod_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(chmod_cb_count == 1);
  chmod_cb_count = 0; /* reset for the next test */
#endif

  /* async chmod */
  {
    static int mode = 0400;
    req.data = &mode;
  }
  r = uv_fs_chmod(loop, &req, "test_file", 0400, chmod_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(chmod_cb_count == 1);

  /* async fchmod */
  {
    static int mode = 0600;
    req.data = &mode;
  }
  r = uv_fs_fchmod(loop, &req, file, 0600, fchmod_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(fchmod_cb_count == 1);

  uv_fs_close(loop, &req, file, NULL);

  /*
   * Run the loop just to check we don't have make any extraneous uv_ref()
   * calls. This should drop out immediately.
   */
  uv_run(loop, UV_RUN_DEFAULT);

  /* Cleanup. */
  unlink("test_file");

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(fs_unlink_readonly) {
  int r;
  uv_fs_t req;
  uv_file file;

  /* Setup. */
  unlink("test_file");

  loop = uv_default_loop();

  r = uv_fs_open(NULL,
                 &req,
                 "test_file",
                 O_RDWR | O_CREAT,
                 S_IWUSR | S_IRUSR,
                 NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  file = req.result;
  uv_fs_req_cleanup(&req);

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = uv_fs_write(NULL, &req, file, &iov, 1, -1, NULL);
  ASSERT(r == sizeof(test_buf));
  ASSERT(req.result == sizeof(test_buf));
  uv_fs_req_cleanup(&req);

  uv_fs_close(loop, &req, file, NULL);

  /* Make the file read-only */
  r = uv_fs_chmod(NULL, &req, "test_file", 0400, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  uv_fs_req_cleanup(&req);

  check_permission("test_file", 0400);

  /* Try to unlink the file */
  r = uv_fs_unlink(NULL, &req, "test_file", NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  uv_fs_req_cleanup(&req);

  /*
  * Run the loop just to check we don't have make any extraneous uv_ref()
  * calls. This should drop out immediately.
  */
  uv_run(loop, UV_RUN_DEFAULT);

  /* Cleanup. */
  uv_fs_chmod(NULL, &req, "test_file", 0600, NULL);
  uv_fs_req_cleanup(&req);
  unlink("test_file");

  MAKE_VALGRIND_HAPPY();
  return 0;
}

#ifdef _WIN32
TEST_IMPL(fs_unlink_archive_readonly) {
  int r;
  uv_fs_t req;
  uv_file file;

  /* Setup. */
  unlink("test_file");

  loop = uv_default_loop();

  r = uv_fs_open(NULL,
                 &req,
                 "test_file",
                 O_RDWR | O_CREAT,
                 S_IWUSR | S_IRUSR,
                 NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  file = req.result;
  uv_fs_req_cleanup(&req);

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = uv_fs_write(NULL, &req, file, &iov, 1, -1, NULL);
  ASSERT(r == sizeof(test_buf));
  ASSERT(req.result == sizeof(test_buf));
  uv_fs_req_cleanup(&req);

  uv_fs_close(loop, &req, file, NULL);

  /* Make the file read-only and clear archive flag */
  r = SetFileAttributes("test_file", FILE_ATTRIBUTE_READONLY);
  ASSERT(r != 0);
  uv_fs_req_cleanup(&req);

  check_permission("test_file", 0400);

  /* Try to unlink the file */
  r = uv_fs_unlink(NULL, &req, "test_file", NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  uv_fs_req_cleanup(&req);

  /*
  * Run the loop just to check we don't have make any extraneous uv_ref()
  * calls. This should drop out immediately.
  */
  uv_run(loop, UV_RUN_DEFAULT);

  /* Cleanup. */
  uv_fs_chmod(NULL, &req, "test_file", 0600, NULL);
  uv_fs_req_cleanup(&req);
  unlink("test_file");

  MAKE_VALGRIND_HAPPY();
  return 0;
}
#endif

TEST_IMPL(fs_chown) {
  int r;
  uv_fs_t req;
  uv_file file;

  /* Setup. */
  unlink("test_file");
  unlink("test_file_link");

  loop = uv_default_loop();

  r = uv_fs_open(NULL, &req, "test_file", O_RDWR | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  file = req.result;
  uv_fs_req_cleanup(&req);

  /* sync chown */
  r = uv_fs_chown(NULL, &req, "test_file", -1, -1, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  uv_fs_req_cleanup(&req);

  /* sync fchown */
  r = uv_fs_fchown(NULL, &req, file, -1, -1, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  uv_fs_req_cleanup(&req);

  /* async chown */
  r = uv_fs_chown(loop, &req, "test_file", -1, -1, chown_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(chown_cb_count == 1);

#ifndef __MVS__
  /* chown to root (fail) */
  chown_cb_count = 0;
  r = uv_fs_chown(loop, &req, "test_file", 0, 0, chown_root_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(chown_cb_count == 1);
#endif

  /* async fchown */
  r = uv_fs_fchown(loop, &req, file, -1, -1, fchown_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(fchown_cb_count == 1);

#ifndef __HAIKU__
  /* Haiku doesn't support hardlink */
  /* sync link */
  r = uv_fs_link(NULL, &req, "test_file", "test_file_link", NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  uv_fs_req_cleanup(&req);

  /* sync lchown */
  r = uv_fs_lchown(NULL, &req, "test_file_link", -1, -1, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  uv_fs_req_cleanup(&req);

  /* async lchown */
  r = uv_fs_lchown(loop, &req, "test_file_link", -1, -1, lchown_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(lchown_cb_count == 1);
#endif

  /* Close file */
  r = uv_fs_close(NULL, &req, file, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  uv_fs_req_cleanup(&req);

  /*
   * Run the loop just to check we don't have make any extraneous uv_ref()
   * calls. This should drop out immediately.
   */
  uv_run(loop, UV_RUN_DEFAULT);

  /* Cleanup. */
  unlink("test_file");
  unlink("test_file_link");

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(fs_link) {
  int r;
  uv_fs_t req;
  uv_file file;
  uv_file link;

  /* Setup. */
  unlink("test_file");
  unlink("test_file_link");
  unlink("test_file_link2");

  loop = uv_default_loop();

  r = uv_fs_open(NULL, &req, "test_file", O_RDWR | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  file = req.result;
  uv_fs_req_cleanup(&req);

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = uv_fs_write(NULL, &req, file, &iov, 1, -1, NULL);
  ASSERT(r == sizeof(test_buf));
  ASSERT(req.result == sizeof(test_buf));
  uv_fs_req_cleanup(&req);

  uv_fs_close(loop, &req, file, NULL);

  /* sync link */
  r = uv_fs_link(NULL, &req, "test_file", "test_file_link", NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  uv_fs_req_cleanup(&req);

  r = uv_fs_open(NULL, &req, "test_file_link", O_RDWR, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  link = req.result;
  uv_fs_req_cleanup(&req);

  memset(buf, 0, sizeof(buf));
  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(NULL, &req, link, &iov, 1, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  ASSERT(strcmp(buf, test_buf) == 0);

  close(link);

  /* async link */
  r = uv_fs_link(loop, &req, "test_file", "test_file_link2", link_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(link_cb_count == 1);

  r = uv_fs_open(NULL, &req, "test_file_link2", O_RDWR, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  link = req.result;
  uv_fs_req_cleanup(&req);

  memset(buf, 0, sizeof(buf));
  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(NULL, &req, link, &iov, 1, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  ASSERT(strcmp(buf, test_buf) == 0);

  uv_fs_close(loop, &req, link, NULL);

  /*
   * Run the loop just to check we don't have make any extraneous uv_ref()
   * calls. This should drop out immediately.
   */
  uv_run(loop, UV_RUN_DEFAULT);

  /* Cleanup. */
  unlink("test_file");
  unlink("test_file_link");
  unlink("test_file_link2");

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(fs_readlink) {
  uv_fs_t req;

  loop = uv_default_loop();
  ASSERT(0 == uv_fs_readlink(loop, &req, "no_such_file", dummy_cb));
  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));
  ASSERT(dummy_cb_count == 1);
  ASSERT(req.ptr == NULL);
  ASSERT(req.result == UV_ENOENT);
  uv_fs_req_cleanup(&req);

  ASSERT(UV_ENOENT == uv_fs_readlink(NULL, &req, "no_such_file", NULL));
  ASSERT(req.ptr == NULL);
  ASSERT(req.result == UV_ENOENT);
  uv_fs_req_cleanup(&req);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(fs_realpath) {
  uv_fs_t req;

  loop = uv_default_loop();
  ASSERT(0 == uv_fs_realpath(loop, &req, "no_such_file", dummy_cb));
  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));
  ASSERT(dummy_cb_count == 1);
  ASSERT(req.ptr == NULL);
#ifdef _WIN32
  /*
   * Windows XP and Server 2003 don't support GetFinalPathNameByHandleW()
   */
  if (req.result == UV_ENOSYS) {
    uv_fs_req_cleanup(&req);
    RETURN_SKIP("realpath is not supported on Windows XP");
  }
#endif
  ASSERT(req.result == UV_ENOENT);
  uv_fs_req_cleanup(&req);

  ASSERT(UV_ENOENT == uv_fs_realpath(NULL, &req, "no_such_file", NULL));
  ASSERT(req.ptr == NULL);
  ASSERT(req.result == UV_ENOENT);
  uv_fs_req_cleanup(&req);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(fs_symlink) {
  int r;
  uv_fs_t req;
  uv_file file;
  uv_file link;
  char test_file_abs_buf[PATHMAX];
  size_t test_file_abs_size;

  /* Setup. */
  unlink("test_file");
  unlink("test_file_symlink");
  unlink("test_file_symlink2");
  unlink("test_file_symlink_symlink");
  unlink("test_file_symlink2_symlink");
  test_file_abs_size = sizeof(test_file_abs_buf);
#ifdef _WIN32
  uv_cwd(test_file_abs_buf, &test_file_abs_size);
  strcat(test_file_abs_buf, "\\test_file");
#else
  uv_cwd(test_file_abs_buf, &test_file_abs_size);
  strcat(test_file_abs_buf, "/test_file");
#endif

  loop = uv_default_loop();

  r = uv_fs_open(NULL, &req, "test_file", O_RDWR | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  file = req.result;
  uv_fs_req_cleanup(&req);

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = uv_fs_write(NULL, &req, file, &iov, 1, -1, NULL);
  ASSERT(r == sizeof(test_buf));
  ASSERT(req.result == sizeof(test_buf));
  uv_fs_req_cleanup(&req);

  uv_fs_close(loop, &req, file, NULL);

  /* sync symlink */
  r = uv_fs_symlink(NULL, &req, "test_file", "test_file_symlink", 0, NULL);
#ifdef _WIN32
  if (r < 0) {
    if (r == UV_ENOTSUP) {
      /*
       * Windows doesn't support symlinks on older versions.
       * We just pass the test and bail out early if we get ENOTSUP.
       */
      return 0;
    } else if (r == UV_EPERM) {
      /*
       * Creating a symlink is only allowed when running elevated.
       * We pass the test and bail out early if we get UV_EPERM.
       */
      return 0;
    }
  }
#endif
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  uv_fs_req_cleanup(&req);

  r = uv_fs_open(NULL, &req, "test_file_symlink", O_RDWR, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  link = req.result;
  uv_fs_req_cleanup(&req);

  memset(buf, 0, sizeof(buf));
  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(NULL, &req, link, &iov, 1, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  ASSERT(strcmp(buf, test_buf) == 0);

  uv_fs_close(loop, &req, link, NULL);

  r = uv_fs_symlink(NULL,
                    &req,
                    "test_file_symlink",
                    "test_file_symlink_symlink",
                    0,
                    NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&req);

#if defined(__MSYS__)
  RETURN_SKIP("symlink reading is not supported on MSYS2");
#endif

  r = uv_fs_readlink(NULL, &req, "test_file_symlink_symlink", NULL);
  ASSERT(r == 0);
  ASSERT(strcmp(req.ptr, "test_file_symlink") == 0);
  uv_fs_req_cleanup(&req);

  r = uv_fs_realpath(NULL, &req, "test_file_symlink_symlink", NULL);
#ifdef _WIN32
  /*
   * Windows XP and Server 2003 don't support GetFinalPathNameByHandleW()
   */
  if (r == UV_ENOSYS) {
    uv_fs_req_cleanup(&req);
    RETURN_SKIP("realpath is not supported on Windows XP");
  }
#endif
  ASSERT(r == 0);
#ifdef _WIN32
  ASSERT(stricmp(req.ptr, test_file_abs_buf) == 0);
#else
  ASSERT(strcmp(req.ptr, test_file_abs_buf) == 0);
#endif
  uv_fs_req_cleanup(&req);

  /* async link */
  r = uv_fs_symlink(loop,
                    &req,
                    "test_file",
                    "test_file_symlink2",
                    0,
                    symlink_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(symlink_cb_count == 1);

  r = uv_fs_open(NULL, &req, "test_file_symlink2", O_RDWR, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  link = req.result;
  uv_fs_req_cleanup(&req);

  memset(buf, 0, sizeof(buf));
  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(NULL, &req, link, &iov, 1, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  ASSERT(strcmp(buf, test_buf) == 0);

  uv_fs_close(loop, &req, link, NULL);

  r = uv_fs_symlink(NULL,
                    &req,
                    "test_file_symlink2",
                    "test_file_symlink2_symlink",
                    0,
                    NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&req);

  r = uv_fs_readlink(loop, &req, "test_file_symlink2_symlink", readlink_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(readlink_cb_count == 1);

  r = uv_fs_realpath(loop, &req, "test_file", realpath_cb);
#ifdef _WIN32
  /*
   * Windows XP and Server 2003 don't support GetFinalPathNameByHandleW()
   */
  if (r == UV_ENOSYS) {
    uv_fs_req_cleanup(&req);
    RETURN_SKIP("realpath is not supported on Windows XP");
  }
#endif
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(realpath_cb_count == 1);

  /*
   * Run the loop just to check we don't have make any extraneous uv_ref()
   * calls. This should drop out immediately.
   */
  uv_run(loop, UV_RUN_DEFAULT);

  /* Cleanup. */
  unlink("test_file");
  unlink("test_file_symlink");
  unlink("test_file_symlink_symlink");
  unlink("test_file_symlink2");
  unlink("test_file_symlink2_symlink");

  MAKE_VALGRIND_HAPPY();
  return 0;
}


int test_symlink_dir_impl(int type) {
  uv_fs_t req;
  int r;
  char* test_dir;
  uv_dirent_t dent;
  static char test_dir_abs_buf[PATHMAX];
  size_t test_dir_abs_size;

  /* set-up */
  unlink("test_dir/file1");
  unlink("test_dir/file2");
  rmdir("test_dir");
  rmdir("test_dir_symlink");
  test_dir_abs_size = sizeof(test_dir_abs_buf);

  loop = uv_default_loop();

  uv_fs_mkdir(NULL, &req, "test_dir", 0777, NULL);
  uv_fs_req_cleanup(&req);

#ifdef _WIN32
  strcpy(test_dir_abs_buf, "\\\\?\\");
  uv_cwd(test_dir_abs_buf + 4, &test_dir_abs_size);
  test_dir_abs_size += 4;
  strcat(test_dir_abs_buf, "\\test_dir\\");
  test_dir_abs_size += strlen("\\test_dir\\");
  test_dir = test_dir_abs_buf;
#else
  uv_cwd(test_dir_abs_buf, &test_dir_abs_size);
  strcat(test_dir_abs_buf, "/test_dir");
  test_dir_abs_size += strlen("/test_dir");
  test_dir = "test_dir";
#endif

  r = uv_fs_symlink(NULL, &req, test_dir, "test_dir_symlink", type, NULL);
  if (type == UV_FS_SYMLINK_DIR && (r == UV_ENOTSUP || r == UV_EPERM)) {
    uv_fs_req_cleanup(&req);
    RETURN_SKIP("this version of Windows doesn't support unprivileged "
                "creation of directory symlinks");
  }
  fprintf(stderr, "r == %i\n", r);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  uv_fs_req_cleanup(&req);

  r = uv_fs_stat(NULL, &req, "test_dir_symlink", NULL);
  ASSERT(r == 0);
  ASSERT(((uv_stat_t*)req.ptr)->st_mode & S_IFDIR);
  uv_fs_req_cleanup(&req);

  r = uv_fs_lstat(NULL, &req, "test_dir_symlink", NULL);
  ASSERT(r == 0);
#if defined(__MSYS__)
  RETURN_SKIP("symlink reading is not supported on MSYS2");
#endif
  ASSERT(((uv_stat_t*)req.ptr)->st_mode & S_IFLNK);
#ifdef _WIN32
  ASSERT(((uv_stat_t*)req.ptr)->st_size == strlen(test_dir + 4));
#else
# ifdef __PASE__
  /* On IBMi PASE, st_size returns the length of the symlink itself. */
  ASSERT(((uv_stat_t*)req.ptr)->st_size == strlen("test_dir_symlink"));
# else
  ASSERT(((uv_stat_t*)req.ptr)->st_size == strlen(test_dir));
# endif
#endif
  uv_fs_req_cleanup(&req);

  r = uv_fs_readlink(NULL, &req, "test_dir_symlink", NULL);
  ASSERT(r == 0);
#ifdef _WIN32
  ASSERT(strcmp(req.ptr, test_dir + 4) == 0);
#else
  ASSERT(strcmp(req.ptr, test_dir) == 0);
#endif
  uv_fs_req_cleanup(&req);

  r = uv_fs_realpath(NULL, &req, "test_dir_symlink", NULL);
#ifdef _WIN32
  /*
   * Windows XP and Server 2003 don't support GetFinalPathNameByHandleW()
   */
  if (r == UV_ENOSYS) {
    uv_fs_req_cleanup(&req);
    RETURN_SKIP("realpath is not supported on Windows XP");
  }
#endif
  ASSERT(r == 0);
#ifdef _WIN32
  ASSERT(strlen(req.ptr) == test_dir_abs_size - 5);
  ASSERT(strnicmp(req.ptr, test_dir + 4, test_dir_abs_size - 5) == 0);
#else
  ASSERT(strcmp(req.ptr, test_dir_abs_buf) == 0);
#endif
  uv_fs_req_cleanup(&req);

  r = uv_fs_open(NULL, &open_req1, "test_dir/file1", O_WRONLY | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  uv_fs_req_cleanup(&open_req1);
  r = uv_fs_close(NULL, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_open(NULL, &open_req1, "test_dir/file2", O_WRONLY | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  uv_fs_req_cleanup(&open_req1);
  r = uv_fs_close(NULL, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_scandir(NULL, &scandir_req, "test_dir_symlink", 0, NULL);
  ASSERT(r == 2);
  ASSERT(scandir_req.result == 2);
  ASSERT(scandir_req.ptr);
  while (UV_EOF != uv_fs_scandir_next(&scandir_req, &dent)) {
    ASSERT(strcmp(dent.name, "file1") == 0 || strcmp(dent.name, "file2") == 0);
    assert_is_file_type(dent);
  }
  uv_fs_req_cleanup(&scandir_req);
  ASSERT(!scandir_req.ptr);

  /* unlink will remove the directory symlink */
  r = uv_fs_unlink(NULL, &req, "test_dir_symlink", NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&req);

  r = uv_fs_scandir(NULL, &scandir_req, "test_dir_symlink", 0, NULL);
  ASSERT(r == UV_ENOENT);
  uv_fs_req_cleanup(&scandir_req);

  r = uv_fs_scandir(NULL, &scandir_req, "test_dir", 0, NULL);
  ASSERT(r == 2);
  ASSERT(scandir_req.result == 2);
  ASSERT(scandir_req.ptr);
  while (UV_EOF != uv_fs_scandir_next(&scandir_req, &dent)) {
    ASSERT(strcmp(dent.name, "file1") == 0 || strcmp(dent.name, "file2") == 0);
    assert_is_file_type(dent);
  }
  uv_fs_req_cleanup(&scandir_req);
  ASSERT(!scandir_req.ptr);

  /* clean-up */
  unlink("test_dir/file1");
  unlink("test_dir/file2");
  rmdir("test_dir");
  rmdir("test_dir_symlink");

  MAKE_VALGRIND_HAPPY();
  return 0;
}

TEST_IMPL(fs_symlink_dir) {
  return test_symlink_dir_impl(UV_FS_SYMLINK_DIR);
}

TEST_IMPL(fs_symlink_junction) {
  return test_symlink_dir_impl(UV_FS_SYMLINK_JUNCTION);
}

#ifdef _WIN32
TEST_IMPL(fs_non_symlink_reparse_point) {
  uv_fs_t req;
  int r;
  HANDLE file_handle;
  REPARSE_GUID_DATA_BUFFER reparse_buffer;
  DWORD bytes_returned;
  uv_dirent_t dent;

  /* set-up */
  unlink("test_dir/test_file");
  rmdir("test_dir");

  loop = uv_default_loop();

  uv_fs_mkdir(NULL, &req, "test_dir", 0777, NULL);
  uv_fs_req_cleanup(&req);

  file_handle = CreateFile("test_dir/test_file",
                           GENERIC_WRITE | FILE_WRITE_ATTRIBUTES,
                           0,
                           NULL,
                           CREATE_ALWAYS,
                           FILE_FLAG_OPEN_REPARSE_POINT |
                             FILE_FLAG_BACKUP_SEMANTICS,
                           NULL);
  ASSERT(file_handle != INVALID_HANDLE_VALUE);

  memset(&reparse_buffer, 0, REPARSE_GUID_DATA_BUFFER_HEADER_SIZE);
  reparse_buffer.ReparseTag = REPARSE_TAG;
  reparse_buffer.ReparseDataLength = 0;
  reparse_buffer.ReparseGuid = REPARSE_GUID;

  r = DeviceIoControl(file_handle,
                      FSCTL_SET_REPARSE_POINT,
                      &reparse_buffer,
                      REPARSE_GUID_DATA_BUFFER_HEADER_SIZE,
                      NULL,
                      0,
                      &bytes_returned,
                      NULL);
  ASSERT(r != 0);

  CloseHandle(file_handle);

  r = uv_fs_readlink(NULL, &req, "test_dir/test_file", NULL);
  ASSERT(r == UV_EINVAL && GetLastError() == ERROR_SYMLINK_NOT_SUPPORTED);
  uv_fs_req_cleanup(&req);

/*
  Placeholder tests for exercising the behavior fixed in issue #995.
  To run, update the path with the IP address of a Mac with the hard drive
  shared via SMB as "Macintosh HD".

  r = uv_fs_stat(NULL, &req, "\\\\<mac_ip>\\Macintosh HD\\.DS_Store", NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&req);

  r = uv_fs_lstat(NULL, &req, "\\\\<mac_ip>\\Macintosh HD\\.DS_Store", NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&req);
*/

/*
  uv_fs_stat and uv_fs_lstat can only work on non-symlink reparse
  points when a minifilter driver is registered which intercepts
  associated filesystem requests. Installing a driver is beyond
  the scope of this test.

  r = uv_fs_stat(NULL, &req, "test_dir/test_file", NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&req);

  r = uv_fs_lstat(NULL, &req, "test_dir/test_file", NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&req);
*/

  r = uv_fs_scandir(NULL, &scandir_req, "test_dir", 0, NULL);
  ASSERT(r == 1);
  ASSERT(scandir_req.result == 1);
  ASSERT(scandir_req.ptr);
  while (UV_EOF != uv_fs_scandir_next(&scandir_req, &dent)) {
    ASSERT(strcmp(dent.name, "test_file") == 0);
    /* uv_fs_scandir incorrectly identifies non-symlink reparse points
       as links because it doesn't open the file and verify the reparse
       point tag. The PowerShell Get-ChildItem command shares this
       behavior, so it's reasonable to leave it as is. */
    ASSERT(dent.type == UV_DIRENT_LINK);
  }
  uv_fs_req_cleanup(&scandir_req);
  ASSERT(!scandir_req.ptr);

  /* clean-up */
  unlink("test_dir/test_file");
  rmdir("test_dir");

  MAKE_VALGRIND_HAPPY();
  return 0;
}

TEST_IMPL(fs_lstat_windows_store_apps) {
  uv_loop_t* loop;
  char localappdata[MAX_PATH];
  char windowsapps_path[MAX_PATH];
  char file_path[MAX_PATH];
  size_t len;
  int r;
  uv_fs_t req;
  uv_fs_t stat_req;
  uv_dirent_t dirent;

  loop = uv_default_loop();
  ASSERT_NOT_NULL(loop);
  len = sizeof(localappdata);
  r = uv_os_getenv("LOCALAPPDATA", localappdata, &len);
  if (r == UV_ENOENT) {
    MAKE_VALGRIND_HAPPY();
    return TEST_SKIP;
  }
  ASSERT_EQ(r, 0);
  r = snprintf(windowsapps_path,
              sizeof(localappdata),
              "%s\\Microsoft\\WindowsApps",
              localappdata);
  ASSERT_GT(r, 0);
  if (uv_fs_opendir(loop, &req, windowsapps_path, NULL) != 0) {
    /* If we cannot read the directory, skip the test. */
    MAKE_VALGRIND_HAPPY();
    return TEST_SKIP;
  }
  if (uv_fs_scandir(loop, &req, windowsapps_path, 0, NULL) <= 0) {
    MAKE_VALGRIND_HAPPY();
    return TEST_SKIP;
  }
  while (uv_fs_scandir_next(&req, &dirent) != UV_EOF) {
    if (dirent.type != UV_DIRENT_LINK) {
      continue;
    }
    if (snprintf(file_path,
                 sizeof(file_path),
                 "%s\\%s",
                 windowsapps_path,
                 dirent.name) < 0) {
      continue;
    }
    ASSERT_EQ(uv_fs_lstat(loop, &stat_req, file_path, NULL), 0);
  }
  MAKE_VALGRIND_HAPPY();
  return 0;
}
#endif


TEST_IMPL(fs_utime) {
  utime_check_t checkme;
  const char* path = "test_file";
  double atime;
  double mtime;
  uv_fs_t req;
  int r;

  /* Setup. */
  loop = uv_default_loop();
  unlink(path);
  r = uv_fs_open(NULL, &req, path, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  uv_fs_req_cleanup(&req);
  uv_fs_close(loop, &req, r, NULL);

  atime = mtime = 400497753.25; /* 1982-09-10 11:22:33.25 */

  r = uv_fs_utime(NULL, &req, path, atime, mtime, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  uv_fs_req_cleanup(&req);

  check_utime(path, atime, mtime, /* test_lutime */ 0);

  atime = mtime = 1291404900.25; /* 2010-12-03 20:35:00.25 - mees <3 */
  checkme.path = path;
  checkme.atime = atime;
  checkme.mtime = mtime;

  /* async utime */
  utime_req.data = &checkme;
  r = uv_fs_utime(loop, &utime_req, path, atime, mtime, utime_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(utime_cb_count == 1);

  /* Cleanup. */
  unlink(path);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(fs_utime_round) {
  const char path[] = "test_file";
  double atime;
  double mtime;
  uv_fs_t req;
  int r;

  loop = uv_default_loop();
  unlink(path);
  r = uv_fs_open(NULL, &req, path, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR, NULL);
  ASSERT_GE(r, 0);
  ASSERT_GE(req.result, 0);
  uv_fs_req_cleanup(&req);
  ASSERT_EQ(0, uv_fs_close(loop, &req, r, NULL));

  atime = mtime = -14245440.25;  /* 1969-07-20T02:56:00.25Z */

  r = uv_fs_utime(NULL, &req, path, atime, mtime, NULL);
#if !defined(__linux__)     && \
    !defined(_WIN32)        && \
    !defined(__APPLE__)     && \
    !defined(__FreeBSD__)   && \
    !defined(__sun)
  if (r != 0) {
    ASSERT_EQ(r, UV_EINVAL);
    RETURN_SKIP("utime on some OS (z/OS, IBM i PASE, AIX) or filesystems may reject pre-epoch timestamps");
  }
#endif
  ASSERT_EQ(0, r);
  ASSERT_EQ(0, req.result);
  uv_fs_req_cleanup(&req);
  check_utime(path, atime, mtime, /* test_lutime */ 0);
  unlink(path);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


#ifdef _WIN32
TEST_IMPL(fs_stat_root) {
  int r;

  r = uv_fs_stat(NULL, &stat_req, "\\", NULL);
  ASSERT(r == 0);

  r = uv_fs_stat(NULL, &stat_req, "..\\..\\..\\..\\..\\..\\..", NULL);
  ASSERT(r == 0);

  r = uv_fs_stat(NULL, &stat_req, "..", NULL);
  ASSERT(r == 0);

  r = uv_fs_stat(NULL, &stat_req, "..\\", NULL);
  ASSERT(r == 0);

  /* stats the current directory on c: */
  r = uv_fs_stat(NULL, &stat_req, "c:", NULL);
  ASSERT(r == 0);

  r = uv_fs_stat(NULL, &stat_req, "c:\\", NULL);
  ASSERT(r == 0);

  r = uv_fs_stat(NULL, &stat_req, "\\\\?\\C:\\", NULL);
  ASSERT(r == 0);

  MAKE_VALGRIND_HAPPY();
  return 0;
}
#endif


TEST_IMPL(fs_futime) {
  utime_check_t checkme;
  const char* path = "test_file";
  double atime;
  double mtime;
  uv_file file;
  uv_fs_t req;
  int r;
#if defined(_AIX) && !defined(_AIX71)
  RETURN_SKIP("futime is not implemented for AIX versions below 7.1");
#endif

  /* Setup. */
  loop = uv_default_loop();
  unlink(path);
  r = uv_fs_open(NULL, &req, path, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  uv_fs_req_cleanup(&req);
  uv_fs_close(loop, &req, r, NULL);

  atime = mtime = 400497753.25; /* 1982-09-10 11:22:33.25 */

  r = uv_fs_open(NULL, &req, path, O_RDWR, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  file = req.result; /* FIXME probably not how it's supposed to be used */
  uv_fs_req_cleanup(&req);

  r = uv_fs_futime(NULL, &req, file, atime, mtime, NULL);
#if defined(__CYGWIN__) || defined(__MSYS__)
  ASSERT(r == UV_ENOSYS);
  RETURN_SKIP("futime not supported on Cygwin");
#else
  ASSERT(r == 0);
  ASSERT(req.result == 0);
#endif
  uv_fs_req_cleanup(&req);

  check_utime(path, atime, mtime, /* test_lutime */ 0);

  atime = mtime = 1291404900; /* 2010-12-03 20:35:00 - mees <3 */

  checkme.atime = atime;
  checkme.mtime = mtime;
  checkme.path = path;

  /* async futime */
  futime_req.data = &checkme;
  r = uv_fs_futime(loop, &futime_req, file, atime, mtime, futime_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(futime_cb_count == 1);

  /* Cleanup. */
  unlink(path);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(fs_lutime) {
  utime_check_t checkme;
  const char* path = "test_file";
  const char* symlink_path = "test_file_symlink";
  double atime;
  double mtime;
  uv_fs_t req;
  int r, s;


  /* Setup */
  loop = uv_default_loop();
  unlink(path);
  r = uv_fs_open(NULL, &req, path, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  uv_fs_req_cleanup(&req);
  uv_fs_close(loop, &req, r, NULL);

  unlink(symlink_path);
  s = uv_fs_symlink(NULL, &req, path, symlink_path, 0, NULL);
#ifdef _WIN32
  if (s == UV_EPERM) {
    /*
     * Creating a symlink before Windows 10 Creators Update was only allowed
     * when running elevated console (with admin rights)
     */
    RETURN_SKIP(
        "Symlink creation requires elevated console (with admin rights)");
  }
#endif
  ASSERT(s == 0);
  ASSERT(req.result == 0);
  uv_fs_req_cleanup(&req);

  /* Test the synchronous version. */
  atime = mtime = 400497753.25; /* 1982-09-10 11:22:33.25 */

  checkme.atime = atime;
  checkme.mtime = mtime;
  checkme.path = symlink_path;
  req.data = &checkme;

  r = uv_fs_lutime(NULL, &req, symlink_path, atime, mtime, NULL);
#if (defined(_AIX) && !defined(_AIX71)) ||                                    \
     defined(__MVS__)
  ASSERT(r == UV_ENOSYS);
  RETURN_SKIP("lutime is not implemented for z/OS and AIX versions below 7.1");
#endif
  ASSERT(r == 0);
  lutime_cb(&req);
  ASSERT(lutime_cb_count == 1);

  /* Test the asynchronous version. */
  atime = mtime = 1291404900; /* 2010-12-03 20:35:00 */

  checkme.atime = atime;
  checkme.mtime = mtime;
  checkme.path = symlink_path;

  r = uv_fs_lutime(loop, &req, symlink_path, atime, mtime, lutime_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(lutime_cb_count == 2);

  /* Cleanup. */
  unlink(path);
  unlink(symlink_path);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(fs_stat_missing_path) {
  uv_fs_t req;
  int r;

  loop = uv_default_loop();

  r = uv_fs_stat(NULL, &req, "non_existent_file", NULL);
  ASSERT(r == UV_ENOENT);
  ASSERT(req.result == UV_ENOENT);
  uv_fs_req_cleanup(&req);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(fs_scandir_empty_dir) {
  const char* path;
  uv_fs_t req;
  uv_dirent_t dent;
  int r;

  path = "./empty_dir/";
  loop = uv_default_loop();

  uv_fs_mkdir(NULL, &req, path, 0777, NULL);
  uv_fs_req_cleanup(&req);

  /* Fill the req to ensure that required fields are cleaned up */
  memset(&req, 0xdb, sizeof(req));

  r = uv_fs_scandir(NULL, &req, path, 0, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  ASSERT(req.ptr == NULL);
  ASSERT(UV_EOF == uv_fs_scandir_next(&req, &dent));
  uv_fs_req_cleanup(&req);

  r = uv_fs_scandir(loop, &scandir_req, path, 0, empty_scandir_cb);
  ASSERT(r == 0);

  ASSERT(scandir_cb_count == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(scandir_cb_count == 1);

  uv_fs_rmdir(NULL, &req, path, NULL);
  uv_fs_req_cleanup(&req);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(fs_scandir_non_existent_dir) {
  const char* path;
  uv_fs_t req;
  uv_dirent_t dent;
  int r;

  path = "./non_existent_dir/";
  loop = uv_default_loop();

  uv_fs_rmdir(NULL, &req, path, NULL);
  uv_fs_req_cleanup(&req);

  /* Fill the req to ensure that required fields are cleaned up */
  memset(&req, 0xdb, sizeof(req));

  r = uv_fs_scandir(NULL, &req, path, 0, NULL);
  ASSERT(r == UV_ENOENT);
  ASSERT(req.result == UV_ENOENT);
  ASSERT(req.ptr == NULL);
  ASSERT(UV_ENOENT == uv_fs_scandir_next(&req, &dent));
  uv_fs_req_cleanup(&req);

  r = uv_fs_scandir(loop, &scandir_req, path, 0, non_existent_scandir_cb);
  ASSERT(r == 0);

  ASSERT(scandir_cb_count == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(scandir_cb_count == 1);

  MAKE_VALGRIND_HAPPY();
  return 0;
}

TEST_IMPL(fs_scandir_file) {
#if defined(__ASAN__)
  RETURN_SKIP("Test does not currently work in ASAN");
#endif
  const char* path;
  int r;

  path = "test/fixtures/empty_file";
  loop = uv_default_loop();

  r = uv_fs_scandir(NULL, &scandir_req, path, 0, NULL);
  ASSERT(r == UV_ENOTDIR);
  uv_fs_req_cleanup(&scandir_req);

  r = uv_fs_scandir(loop, &scandir_req, path, 0, file_scandir_cb);
  ASSERT(r == 0);

  ASSERT(scandir_cb_count == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(scandir_cb_count == 1);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(fs_open_dir) {
  const char* path;
  uv_fs_t req;
  int r, file;

  path = ".";
  loop = uv_default_loop();

  r = uv_fs_open(NULL, &req, path, O_RDONLY, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  ASSERT(req.ptr == NULL);
  file = r;
  uv_fs_req_cleanup(&req);

  r = uv_fs_close(NULL, &req, file, NULL);
  ASSERT(r == 0);

  r = uv_fs_open(loop, &req, path, O_RDONLY, 0, open_cb_simple);
  ASSERT(r == 0);

  ASSERT(open_cb_count == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(open_cb_count == 1);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


static void fs_file_open_append(int add_flags) {
  int r;

  /* Setup. */
  unlink("test_file");

  loop = uv_default_loop();

  r = uv_fs_open(NULL, &open_req1, "test_file",
      O_WRONLY | O_CREAT | add_flags, S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = uv_fs_write(NULL, &write_req, open_req1.result, &iov, 1, -1, NULL);
  ASSERT(r >= 0);
  ASSERT(write_req.result >= 0);
  uv_fs_req_cleanup(&write_req);

  r = uv_fs_close(NULL, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_open(NULL, &open_req1, "test_file",
      O_RDWR | O_APPEND | add_flags, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = uv_fs_write(NULL, &write_req, open_req1.result, &iov, 1, -1, NULL);
  ASSERT(r >= 0);
  ASSERT(write_req.result >= 0);
  uv_fs_req_cleanup(&write_req);

  r = uv_fs_close(NULL, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_open(NULL, &open_req1, "test_file", O_RDONLY | add_flags,
      S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(NULL, &read_req, open_req1.result, &iov, 1, -1, NULL);
  printf("read = %d\n", r);
  ASSERT(r == 26);
  ASSERT(read_req.result == 26);
  ASSERT(memcmp(buf,
                "test-buffer\n\0test-buffer\n\0",
                sizeof("test-buffer\n\0test-buffer\n\0") - 1) == 0);
  uv_fs_req_cleanup(&read_req);

  r = uv_fs_close(NULL, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  /* Cleanup */
  unlink("test_file");
}
TEST_IMPL(fs_file_open_append) {
  fs_file_open_append(0);
  fs_file_open_append(UV_FS_O_FILEMAP);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(fs_rename_to_existing_file) {
  int r;

  /* Setup. */
  unlink("test_file");
  unlink("test_file2");

  loop = uv_default_loop();

  r = uv_fs_open(NULL, &open_req1, "test_file", O_WRONLY | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = uv_fs_write(NULL, &write_req, open_req1.result, &iov, 1, -1, NULL);
  ASSERT(r >= 0);
  ASSERT(write_req.result >= 0);
  uv_fs_req_cleanup(&write_req);

  r = uv_fs_close(NULL, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_open(NULL, &open_req1, "test_file2", O_WRONLY | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  r = uv_fs_close(NULL, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_rename(NULL, &rename_req, "test_file", "test_file2", NULL);
  ASSERT(r == 0);
  ASSERT(rename_req.result == 0);
  uv_fs_req_cleanup(&rename_req);

  r = uv_fs_open(NULL, &open_req1, "test_file2", O_RDONLY, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  memset(buf, 0, sizeof(buf));
  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(NULL, &read_req, open_req1.result, &iov, 1, -1, NULL);
  ASSERT(r >= 0);
  ASSERT(read_req.result >= 0);
  ASSERT(strcmp(buf, test_buf) == 0);
  uv_fs_req_cleanup(&read_req);

  r = uv_fs_close(NULL, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  /* Cleanup */
  unlink("test_file");
  unlink("test_file2");

  MAKE_VALGRIND_HAPPY();
  return 0;
}


static void fs_read_bufs(int add_flags) {
  char scratch[768];
  uv_buf_t bufs[4];

  ASSERT(0 <= uv_fs_open(NULL, &open_req1,
                         "test/fixtures/lorem_ipsum.txt",
                         O_RDONLY | add_flags, 0, NULL));
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  ASSERT(UV_EINVAL == uv_fs_read(NULL, &read_req, open_req1.result,
                                 NULL, 0, 0, NULL));
  ASSERT(UV_EINVAL == uv_fs_read(NULL, &read_req, open_req1.result,
                                 NULL, 1, 0, NULL));
  ASSERT(UV_EINVAL == uv_fs_read(NULL, &read_req, open_req1.result,
                                 bufs, 0, 0, NULL));

  bufs[0] = uv_buf_init(scratch + 0, 256);
  bufs[1] = uv_buf_init(scratch + 256, 256);
  bufs[2] = uv_buf_init(scratch + 512, 128);
  bufs[3] = uv_buf_init(scratch + 640, 128);

  ASSERT(446 == uv_fs_read(NULL,
                           &read_req,
                           open_req1.result,
                           bufs + 0,
                           2,  /* 2x 256 bytes. */
                           0,  /* Positional read. */
                           NULL));
  ASSERT(read_req.result == 446);
  uv_fs_req_cleanup(&read_req);

  ASSERT(190 == uv_fs_read(NULL,
                           &read_req,
                           open_req1.result,
                           bufs + 2,
                           2,  /* 2x 128 bytes. */
                           256,  /* Positional read. */
                           NULL));
  ASSERT(read_req.result == /* 446 - 256 */ 190);
  uv_fs_req_cleanup(&read_req);

  ASSERT(0 == memcmp(bufs[1].base + 0, bufs[2].base, 128));
  ASSERT(0 == memcmp(bufs[1].base + 128, bufs[3].base, 190 - 128));

  ASSERT(0 == uv_fs_close(NULL, &close_req, open_req1.result, NULL));
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);
}
TEST_IMPL(fs_read_bufs) {
#if defined(__ASAN__)
  RETURN_SKIP("Test does not currently work in ASAN");
#endif
  fs_read_bufs(0);
  fs_read_bufs(UV_FS_O_FILEMAP);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


static void fs_read_file_eof(int add_flags) {
#if defined(__CYGWIN__) || defined(__MSYS__)
  RETURN_SKIP("Cygwin pread at EOF may (incorrectly) return data!");
#endif
  int r;

  /* Setup. */
  unlink("test_file");

  loop = uv_default_loop();

  r = uv_fs_open(NULL, &open_req1, "test_file",
      O_WRONLY | O_CREAT | add_flags, S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = uv_fs_write(NULL, &write_req, open_req1.result, &iov, 1, -1, NULL);
  ASSERT(r >= 0);
  ASSERT(write_req.result >= 0);
  uv_fs_req_cleanup(&write_req);

  r = uv_fs_close(NULL, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_open(NULL, &open_req1, "test_file", O_RDONLY | add_flags, 0,
      NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  memset(buf, 0, sizeof(buf));
  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(NULL, &read_req, open_req1.result, &iov, 1, -1, NULL);
  ASSERT(r >= 0);
  ASSERT(read_req.result >= 0);
  ASSERT(strcmp(buf, test_buf) == 0);
  uv_fs_req_cleanup(&read_req);

  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(NULL, &read_req, open_req1.result, &iov, 1,
                 read_req.result, NULL);
  ASSERT(r == 0);
  ASSERT(read_req.result == 0);
  uv_fs_req_cleanup(&read_req);

  r = uv_fs_close(NULL, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  /* Cleanup */
  unlink("test_file");
}
TEST_IMPL(fs_read_file_eof) {
  fs_read_file_eof(0);
  fs_read_file_eof(UV_FS_O_FILEMAP);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


static void fs_write_multiple_bufs(int add_flags) {
  uv_buf_t iovs[2];
  int r;

  /* Setup. */
  unlink("test_file");

  loop = uv_default_loop();

  r = uv_fs_open(NULL, &open_req1, "test_file",
      O_WRONLY | O_CREAT | add_flags, S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  iovs[0] = uv_buf_init(test_buf, sizeof(test_buf));
  iovs[1] = uv_buf_init(test_buf2, sizeof(test_buf2));
  r = uv_fs_write(NULL, &write_req, open_req1.result, iovs, 2, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(write_req.result >= 0);
  uv_fs_req_cleanup(&write_req);

  r = uv_fs_close(NULL, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_open(NULL, &open_req1, "test_file", O_RDONLY | add_flags, 0,
      NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  memset(buf, 0, sizeof(buf));
  memset(buf2, 0, sizeof(buf2));
  /* Read the strings back to separate buffers. */
  iovs[0] = uv_buf_init(buf, sizeof(test_buf));
  iovs[1] = uv_buf_init(buf2, sizeof(test_buf2));
  ASSERT(lseek(open_req1.result, 0, SEEK_CUR) == 0);
  r = uv_fs_read(NULL, &read_req, open_req1.result, iovs, 2, -1, NULL);
  ASSERT(r >= 0);
  ASSERT(read_req.result == sizeof(test_buf) + sizeof(test_buf2));
  ASSERT(strcmp(buf, test_buf) == 0);
  ASSERT(strcmp(buf2, test_buf2) == 0);
  uv_fs_req_cleanup(&read_req);

  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(NULL, &read_req, open_req1.result, &iov, 1, -1, NULL);
  ASSERT(r == 0);
  ASSERT(read_req.result == 0);
  uv_fs_req_cleanup(&read_req);

  /* Read the strings back to separate buffers. */
  iovs[0] = uv_buf_init(buf, sizeof(test_buf));
  iovs[1] = uv_buf_init(buf2, sizeof(test_buf2));
  r = uv_fs_read(NULL, &read_req, open_req1.result, iovs, 2, 0, NULL);
  ASSERT(r >= 0);
  if (read_req.result == sizeof(test_buf)) {
    /* Infer that preadv is not available. */
    uv_fs_req_cleanup(&read_req);
    r = uv_fs_read(NULL, &read_req, open_req1.result, &iovs[1], 1, read_req.result, NULL);
    ASSERT(r >= 0);
    ASSERT(read_req.result == sizeof(test_buf2));
  } else {
    ASSERT(read_req.result == sizeof(test_buf) + sizeof(test_buf2));
  }
  ASSERT(strcmp(buf, test_buf) == 0);
  ASSERT(strcmp(buf2, test_buf2) == 0);
  uv_fs_req_cleanup(&read_req);

  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(NULL, &read_req, open_req1.result, &iov, 1,
                 sizeof(test_buf) + sizeof(test_buf2), NULL);
  ASSERT(r == 0);
  ASSERT(read_req.result == 0);
  uv_fs_req_cleanup(&read_req);

  r = uv_fs_close(NULL, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  /* Cleanup */
  unlink("test_file");
}
TEST_IMPL(fs_write_multiple_bufs) {
  fs_write_multiple_bufs(0);
  fs_write_multiple_bufs(UV_FS_O_FILEMAP);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


static void fs_write_alotof_bufs(int add_flags) {
  size_t iovcount;
  size_t iovmax;
  uv_buf_t* iovs;
  char* buffer;
  size_t index;
  int r;

  iovcount = 54321;

  /* Setup. */
  unlink("test_file");

  loop = uv_default_loop();

  iovs = malloc(sizeof(*iovs) * iovcount);
  ASSERT(iovs != NULL);
  iovmax = uv_test_getiovmax();

  r = uv_fs_open(NULL,
                 &open_req1,
                 "test_file",
                 O_RDWR | O_CREAT | add_flags,
                 S_IWUSR | S_IRUSR,
                 NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  for (index = 0; index < iovcount; ++index)
    iovs[index] = uv_buf_init(test_buf, sizeof(test_buf));

  r = uv_fs_write(NULL,
                  &write_req,
                  open_req1.result,
                  iovs,
                  iovcount,
                  -1,
                  NULL);
  ASSERT(r >= 0);
  ASSERT((size_t)write_req.result == sizeof(test_buf) * iovcount);
  uv_fs_req_cleanup(&write_req);

  /* Read the strings back to separate buffers. */
  buffer = malloc(sizeof(test_buf) * iovcount);
  ASSERT(buffer != NULL);

  for (index = 0; index < iovcount; ++index)
    iovs[index] = uv_buf_init(buffer + index * sizeof(test_buf),
                              sizeof(test_buf));

  r = uv_fs_close(NULL, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_open(NULL, &open_req1, "test_file", O_RDONLY | add_flags, 0,
    NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  r = uv_fs_read(NULL, &read_req, open_req1.result, iovs, iovcount, -1, NULL);
  if (iovcount > iovmax)
    iovcount = iovmax;
  ASSERT(r >= 0);
  ASSERT((size_t)read_req.result == sizeof(test_buf) * iovcount);

  for (index = 0; index < iovcount; ++index)
    ASSERT(strncmp(buffer + index * sizeof(test_buf),
                   test_buf,
                   sizeof(test_buf)) == 0);

  uv_fs_req_cleanup(&read_req);
  free(buffer);

  ASSERT(lseek(open_req1.result, write_req.result, SEEK_SET) == write_req.result);
  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(NULL,
                 &read_req,
                 open_req1.result,
                 &iov,
                 1,
                 -1,
                 NULL);
  ASSERT(r == 0);
  ASSERT(read_req.result == 0);
  uv_fs_req_cleanup(&read_req);

  r = uv_fs_close(NULL, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  /* Cleanup */
  unlink("test_file");
  free(iovs);
}
TEST_IMPL(fs_write_alotof_bufs) {
  fs_write_alotof_bufs(0);
  fs_write_alotof_bufs(UV_FS_O_FILEMAP);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


static void fs_write_alotof_bufs_with_offset(int add_flags) {
  size_t iovcount;
  size_t iovmax;
  uv_buf_t* iovs;
  char* buffer;
  size_t index;
  int r;
  int64_t offset;
  char* filler;
  int filler_len;

  filler = "0123456789";
  filler_len = strlen(filler);
  iovcount = 54321;

  /* Setup. */
  unlink("test_file");

  loop = uv_default_loop();

  iovs = malloc(sizeof(*iovs) * iovcount);
  ASSERT(iovs != NULL);
  iovmax = uv_test_getiovmax();

  r = uv_fs_open(NULL,
                 &open_req1,
                 "test_file",
                 O_RDWR | O_CREAT | add_flags,
                 S_IWUSR | S_IRUSR,
                 NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  iov = uv_buf_init(filler, filler_len);
  r = uv_fs_write(NULL, &write_req, open_req1.result, &iov, 1, -1, NULL);
  ASSERT(r == filler_len);
  ASSERT(write_req.result == filler_len);
  uv_fs_req_cleanup(&write_req);
  offset = (int64_t)r;

  for (index = 0; index < iovcount; ++index)
    iovs[index] = uv_buf_init(test_buf, sizeof(test_buf));

  r = uv_fs_write(NULL,
                  &write_req,
                  open_req1.result,
                  iovs,
                  iovcount,
                  offset,
                  NULL);
  ASSERT(r >= 0);
  ASSERT((size_t)write_req.result == sizeof(test_buf) * iovcount);
  uv_fs_req_cleanup(&write_req);

  /* Read the strings back to separate buffers. */
  buffer = malloc(sizeof(test_buf) * iovcount);
  ASSERT(buffer != NULL);

  for (index = 0; index < iovcount; ++index)
    iovs[index] = uv_buf_init(buffer + index * sizeof(test_buf),
                              sizeof(test_buf));

  r = uv_fs_read(NULL, &read_req, open_req1.result,
                 iovs, iovcount, offset, NULL);
  ASSERT(r >= 0);
  if (r == sizeof(test_buf))
    iovcount = 1; /* Infer that preadv is not available. */
  else if (iovcount > iovmax)
    iovcount = iovmax;
  ASSERT((size_t)read_req.result == sizeof(test_buf) * iovcount);

  for (index = 0; index < iovcount; ++index)
    ASSERT(strncmp(buffer + index * sizeof(test_buf),
                   test_buf,
                   sizeof(test_buf)) == 0);

  uv_fs_req_cleanup(&read_req);
  free(buffer);

  r = uv_fs_stat(NULL, &stat_req, "test_file", NULL);
  ASSERT(r == 0);
  ASSERT((int64_t)((uv_stat_t*)stat_req.ptr)->st_size ==
         offset + (int64_t)write_req.result);
  uv_fs_req_cleanup(&stat_req);

  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(NULL,
                 &read_req,
                 open_req1.result,
                 &iov,
                 1,
                 offset + write_req.result,
                 NULL);
  ASSERT(r == 0);
  ASSERT(read_req.result == 0);
  uv_fs_req_cleanup(&read_req);

  r = uv_fs_close(NULL, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  /* Cleanup */
  unlink("test_file");
  free(iovs);
}
TEST_IMPL(fs_write_alotof_bufs_with_offset) {
  fs_write_alotof_bufs_with_offset(0);
  fs_write_alotof_bufs_with_offset(UV_FS_O_FILEMAP);

  MAKE_VALGRIND_HAPPY();
  return 0;
}

TEST_IMPL(fs_read_dir) {
  int r;
  char buf[2];
  loop = uv_default_loop();

  /* Setup */
  rmdir("test_dir");
  r = uv_fs_mkdir(loop, &mkdir_req, "test_dir", 0755, mkdir_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(mkdir_cb_count == 1);
  /* Setup Done Here */

  /* Get a file descriptor for the directory */
  r = uv_fs_open(loop,
                 &open_req1,
                 "test_dir",
                 UV_FS_O_RDONLY | UV_FS_O_DIRECTORY,
                 S_IWUSR | S_IRUSR,
                 NULL);
  ASSERT(r >= 0);
  uv_fs_req_cleanup(&open_req1);

  /* Try to read data from the directory */
  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(NULL, &read_req, open_req1.result, &iov, 1, 0, NULL);
#if defined(__FreeBSD__)   || \
    defined(__OpenBSD__)   || \
    defined(__NetBSD__)    || \
    defined(__DragonFly__) || \
    defined(_AIX)          || \
    defined(__sun)         || \
    defined(__MVS__)
  /*
   * As of now, these operating systems support reading from a directory,
   * that too depends on the filesystem this temporary test directory is
   * created on. That is why this assertion is a bit lenient.
   */
  ASSERT((r >= 0) || (r == UV_EISDIR));
#else
  ASSERT(r == UV_EISDIR);
#endif
  uv_fs_req_cleanup(&read_req);

  r = uv_fs_close(NULL, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&close_req);

  /* Cleanup */
  rmdir("test_dir");

  MAKE_VALGRIND_HAPPY();
  return 0;
}

#ifdef _WIN32

TEST_IMPL(fs_partial_read) {
  RETURN_SKIP("Test not implemented on Windows.");
}

TEST_IMPL(fs_partial_write) {
  RETURN_SKIP("Test not implemented on Windows.");
}

#else  /* !_WIN32 */

struct thread_ctx {
  pthread_t pid;
  int fd;
  char* data;
  int size;
  int interval;
  int doread;
};

static void thread_main(void* arg) {
  const struct thread_ctx* ctx;
  int size;
  char* data;

  ctx = (struct thread_ctx*)arg;
  size = ctx->size;
  data = ctx->data;

  while (size > 0) {
    ssize_t result;
    int nbytes;
    nbytes = size < ctx->interval ? size : ctx->interval;
    if (ctx->doread) {
      result = write(ctx->fd, data, nbytes);
      /* Should not see EINTR (or other errors) */
      ASSERT(result == nbytes);
    } else {
      result = read(ctx->fd, data, nbytes);
      /* Should not see EINTR (or other errors),
       * but might get a partial read if we are faster than the writer
       */
      ASSERT(result > 0 && result <= nbytes);
    }

    pthread_kill(ctx->pid, SIGUSR1);
    size -= result;
    data += result;
  }
}

static void sig_func(uv_signal_t* handle, int signum) {
  uv_signal_stop(handle);
}

static size_t uv_test_fs_buf_offset(uv_buf_t* bufs, size_t size) {
  size_t offset;
  /* Figure out which bufs are done */
  for (offset = 0; size > 0 && bufs[offset].len <= size; ++offset)
    size -= bufs[offset].len;

  /* Fix a partial read/write */
  if (size > 0) {
    bufs[offset].base += size;
    bufs[offset].len -= size;
  }
  return offset;
}

static void test_fs_partial(int doread) {
  struct thread_ctx ctx;
  uv_thread_t thread;
  uv_signal_t signal;
  int pipe_fds[2];
  size_t iovcount;
  uv_buf_t* iovs;
  char* buffer;
  size_t index;

  iovcount = 54321;

  iovs = malloc(sizeof(*iovs) * iovcount);
  ASSERT(iovs != NULL);

  ctx.pid = pthread_self();
  ctx.doread = doread;
  ctx.interval = 1000;
  ctx.size = sizeof(test_buf) * iovcount;
  ctx.data = malloc(ctx.size);
  ASSERT(ctx.data != NULL);
  buffer = malloc(ctx.size);
  ASSERT(buffer != NULL);

  for (index = 0; index < iovcount; ++index)
    iovs[index] = uv_buf_init(buffer + index * sizeof(test_buf), sizeof(test_buf));

  loop = uv_default_loop();

  ASSERT(0 == uv_signal_init(loop, &signal));
  ASSERT(0 == uv_signal_start(&signal, sig_func, SIGUSR1));

  ASSERT(0 == pipe(pipe_fds));

  ctx.fd = pipe_fds[doread];
  ASSERT(0 == uv_thread_create(&thread, thread_main, &ctx));

  if (doread) {
    uv_buf_t* read_iovs;
    int nread;
    read_iovs = iovs;
    nread = 0;
    while (nread < ctx.size) {
      int result;
      result = uv_fs_read(loop, &read_req, pipe_fds[0], read_iovs, iovcount, -1, NULL);
      if (result > 0) {
        size_t read_iovcount;
        read_iovcount = uv_test_fs_buf_offset(read_iovs, result);
        read_iovs += read_iovcount;
        iovcount -= read_iovcount;
        nread += result;
      } else {
        ASSERT(result == UV_EINTR);
      }
      uv_fs_req_cleanup(&read_req);
    }
  } else {
    int result;
    result = uv_fs_write(loop, &write_req, pipe_fds[1], iovs, iovcount, -1, NULL);
    ASSERT(write_req.result == result);
    ASSERT(result == ctx.size);
    uv_fs_req_cleanup(&write_req);
  }

  ASSERT(0 == memcmp(buffer, ctx.data, ctx.size));

  ASSERT(0 == uv_thread_join(&thread));
  ASSERT(0 == uv_run(loop, UV_RUN_DEFAULT));

  ASSERT(0 == close(pipe_fds[1]));
  uv_close((uv_handle_t*) &signal, NULL);

  { /* Make sure we read everything that we wrote. */
      int result;
      result = uv_fs_read(loop, &read_req, pipe_fds[0], iovs, 1, -1, NULL);
      ASSERT(result == 0);
      uv_fs_req_cleanup(&read_req);
  }
  ASSERT(0 == close(pipe_fds[0]));

  free(iovs);
  free(buffer);
  free(ctx.data);

  MAKE_VALGRIND_HAPPY();
}

TEST_IMPL(fs_partial_read) {
  test_fs_partial(1);
  return 0;
}

TEST_IMPL(fs_partial_write) {
  test_fs_partial(0);
  return 0;
}

#endif/* _WIN32 */

TEST_IMPL(fs_read_write_null_arguments) {
  int r;

  r = uv_fs_read(NULL, &read_req, 0, NULL, 0, -1, NULL);
  ASSERT(r == UV_EINVAL);
  uv_fs_req_cleanup(&read_req);

  r = uv_fs_write(NULL, &write_req, 0, NULL, 0, -1, NULL);
  /* Validate some memory management on failed input validation before sending
     fs work to the thread pool. */
  ASSERT(r == UV_EINVAL);
  ASSERT(write_req.path == NULL);
  ASSERT(write_req.ptr == NULL);
#ifdef _WIN32
  ASSERT(write_req.file.pathw == NULL);
  ASSERT(write_req.fs.info.new_pathw == NULL);
  ASSERT(write_req.fs.info.bufs == NULL);
#else
  ASSERT(write_req.new_path == NULL);
  ASSERT(write_req.bufs == NULL);
#endif
  uv_fs_req_cleanup(&write_req);

  iov = uv_buf_init(NULL, 0);
  r = uv_fs_read(NULL, &read_req, 0, &iov, 0, -1, NULL);
  ASSERT(r == UV_EINVAL);
  uv_fs_req_cleanup(&read_req);

  iov = uv_buf_init(NULL, 0);
  r = uv_fs_write(NULL, &write_req, 0, &iov, 0, -1, NULL);
  ASSERT(r == UV_EINVAL);
  uv_fs_req_cleanup(&write_req);

  /* If the arguments are invalid, the loop should not be kept open */
  loop = uv_default_loop();

  r = uv_fs_read(loop, &read_req, 0, NULL, 0, -1, fail_cb);
  ASSERT(r == UV_EINVAL);
  uv_run(loop, UV_RUN_DEFAULT);
  uv_fs_req_cleanup(&read_req);

  r = uv_fs_write(loop, &write_req, 0, NULL, 0, -1, fail_cb);
  ASSERT(r == UV_EINVAL);
  uv_run(loop, UV_RUN_DEFAULT);
  uv_fs_req_cleanup(&write_req);

  iov = uv_buf_init(NULL, 0);
  r = uv_fs_read(loop, &read_req, 0, &iov, 0, -1, fail_cb);
  ASSERT(r == UV_EINVAL);
  uv_run(loop, UV_RUN_DEFAULT);
  uv_fs_req_cleanup(&read_req);

  iov = uv_buf_init(NULL, 0);
  r = uv_fs_write(loop, &write_req, 0, &iov, 0, -1, fail_cb);
  ASSERT(r == UV_EINVAL);
  uv_run(loop, UV_RUN_DEFAULT);
  uv_fs_req_cleanup(&write_req);

  return 0;
}


TEST_IMPL(get_osfhandle_valid_handle) {
  int r;
  uv_os_fd_t fd;

  /* Setup. */
  unlink("test_file");

  loop = uv_default_loop();

  r = uv_fs_open(NULL,
                 &open_req1,
                 "test_file",
                 O_RDWR | O_CREAT,
                 S_IWUSR | S_IRUSR,
                 NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  fd = uv_get_osfhandle(open_req1.result);
#ifdef _WIN32
  ASSERT(fd != INVALID_HANDLE_VALUE);
#else
  ASSERT(fd >= 0);
#endif

  r = uv_fs_close(NULL, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  /* Cleanup. */
  unlink("test_file");

  MAKE_VALGRIND_HAPPY();
  return 0;
}

TEST_IMPL(open_osfhandle_valid_handle) {
  int r;
  uv_os_fd_t handle;
  int fd;

  /* Setup. */
  unlink("test_file");

  loop = uv_default_loop();

  r = uv_fs_open(NULL,
                 &open_req1,
                 "test_file",
                 O_RDWR | O_CREAT,
                 S_IWUSR | S_IRUSR,
                 NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  handle = uv_get_osfhandle(open_req1.result);
#ifdef _WIN32
  ASSERT(handle != INVALID_HANDLE_VALUE);
#else
  ASSERT(handle >= 0);
#endif

  fd = uv_open_osfhandle(handle);
#ifdef _WIN32
  ASSERT(fd > 0);
#else
  ASSERT(fd == open_req1.result);
#endif

  r = uv_fs_close(NULL, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  /* Cleanup. */
  unlink("test_file");

  MAKE_VALGRIND_HAPPY();
  return 0;
}

TEST_IMPL(fs_file_pos_after_op_with_offset) {
  int r;

  /* Setup. */
  unlink("test_file");
  loop = uv_default_loop();

  r = uv_fs_open(loop,
                 &open_req1,
                 "test_file",
                 O_RDWR | O_CREAT,
                 S_IWUSR | S_IRUSR,
                 NULL);
  ASSERT(r > 0);
  uv_fs_req_cleanup(&open_req1);

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = uv_fs_write(NULL, &write_req, open_req1.result, &iov, 1, 0, NULL);
  ASSERT(r == sizeof(test_buf));
  ASSERT(lseek(open_req1.result, 0, SEEK_CUR) == 0);
  uv_fs_req_cleanup(&write_req);

  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(NULL, &read_req, open_req1.result, &iov, 1, 0, NULL);
  ASSERT(r == sizeof(test_buf));
  ASSERT(strcmp(buf, test_buf) == 0);
  ASSERT(lseek(open_req1.result, 0, SEEK_CUR) == 0);
  uv_fs_req_cleanup(&read_req);

  r = uv_fs_close(NULL, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&close_req);

  /* Cleanup */
  unlink("test_file");

  MAKE_VALGRIND_HAPPY();
  return 0;
}

#ifdef _WIN32
static void fs_file_pos_common(void) {
  int r;

  iov = uv_buf_init("abc", 3);
  r = uv_fs_write(NULL, &write_req, open_req1.result, &iov, 1, -1, NULL);
  ASSERT(r == 3);
  uv_fs_req_cleanup(&write_req);

  /* Read with offset should not change the position */
  iov = uv_buf_init(buf, 1);
  r = uv_fs_read(NULL, &read_req, open_req1.result, &iov, 1, 1, NULL);
  ASSERT(r == 1);
  ASSERT(buf[0] == 'b');
  uv_fs_req_cleanup(&read_req);

  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(NULL, &read_req, open_req1.result, &iov, 1, -1, NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&read_req);

  /* Write without offset should change the position */
  iov = uv_buf_init("d", 1);
  r = uv_fs_write(NULL, &write_req, open_req1.result, &iov, 1, -1, NULL);
  ASSERT(r == 1);
  uv_fs_req_cleanup(&write_req);

  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(NULL, &read_req, open_req1.result, &iov, 1, -1, NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&read_req);
}

static void fs_file_pos_close_check(const char *contents, int size) {
  int r;

  /* Close */
  r = uv_fs_close(NULL, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&close_req);

  /* Confirm file contents */
  r = uv_fs_open(NULL, &open_req1, "test_file", O_RDONLY, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(NULL, &read_req, open_req1.result, &iov, 1, -1, NULL);
  ASSERT(r == size);
  ASSERT(strncmp(buf, contents, size) == 0);
  uv_fs_req_cleanup(&read_req);

  r = uv_fs_close(NULL, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&close_req);

  /* Cleanup */
  unlink("test_file");
}

static void fs_file_pos_write(int add_flags) {
  int r;

  /* Setup. */
  unlink("test_file");

  r = uv_fs_open(NULL,
                 &open_req1,
                 "test_file",
                 O_TRUNC | O_CREAT | O_RDWR | add_flags,
                 S_IWUSR | S_IRUSR,
                 NULL);
  ASSERT(r > 0);
  uv_fs_req_cleanup(&open_req1);

  fs_file_pos_common();

  /* Write with offset should not change the position */
  iov = uv_buf_init("e", 1);
  r = uv_fs_write(NULL, &write_req, open_req1.result, &iov, 1, 1, NULL);
  ASSERT(r == 1);
  uv_fs_req_cleanup(&write_req);

  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(NULL, &read_req, open_req1.result, &iov, 1, -1, NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&read_req);

  fs_file_pos_close_check("aecd", 4);
}
TEST_IMPL(fs_file_pos_write) {
  fs_file_pos_write(0);
  fs_file_pos_write(UV_FS_O_FILEMAP);

  MAKE_VALGRIND_HAPPY();
  return 0;
}

static void fs_file_pos_append(int add_flags) {
  int r;

  /* Setup. */
  unlink("test_file");

  r = uv_fs_open(NULL,
                 &open_req1,
                 "test_file",
                 O_APPEND | O_CREAT | O_RDWR | add_flags,
                 S_IWUSR | S_IRUSR,
                 NULL);
  ASSERT(r > 0);
  uv_fs_req_cleanup(&open_req1);

  fs_file_pos_common();

  /* Write with offset appends (ignoring offset)
   * but does not change the position */
  iov = uv_buf_init("e", 1);
  r = uv_fs_write(NULL, &write_req, open_req1.result, &iov, 1, 1, NULL);
  ASSERT(r == 1);
  uv_fs_req_cleanup(&write_req);

  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(NULL, &read_req, open_req1.result, &iov, 1, -1, NULL);
  ASSERT(r == 1);
  ASSERT(buf[0] == 'e');
  uv_fs_req_cleanup(&read_req);

  fs_file_pos_close_check("abcde", 5);
}
TEST_IMPL(fs_file_pos_append) {
  fs_file_pos_append(0);
  fs_file_pos_append(UV_FS_O_FILEMAP);

  MAKE_VALGRIND_HAPPY();
  return 0;
}
#endif

TEST_IMPL(fs_null_req) {
  /* Verify that all fs functions return UV_EINVAL when the request is NULL. */
  int r;

  r = uv_fs_open(NULL, NULL, NULL, 0, 0, NULL);
  ASSERT(r == UV_EINVAL);

  r = uv_fs_close(NULL, NULL, 0, NULL);
  ASSERT(r == UV_EINVAL);

  r = uv_fs_read(NULL, NULL, 0, NULL, 0, -1, NULL);
  ASSERT(r == UV_EINVAL);

  r = uv_fs_write(NULL, NULL, 0, NULL, 0, -1, NULL);
  ASSERT(r == UV_EINVAL);

  r = uv_fs_unlink(NULL, NULL, NULL, NULL);
  ASSERT(r == UV_EINVAL);

  r = uv_fs_mkdir(NULL, NULL, NULL, 0, NULL);
  ASSERT(r == UV_EINVAL);

  r = uv_fs_mkdtemp(NULL, NULL, NULL, NULL);
  ASSERT(r == UV_EINVAL);

  r = uv_fs_mkstemp(NULL, NULL, NULL, NULL);
  ASSERT(r == UV_EINVAL);

  r = uv_fs_rmdir(NULL, NULL, NULL, NULL);
  ASSERT(r == UV_EINVAL);

  r = uv_fs_scandir(NULL, NULL, NULL, 0, NULL);
  ASSERT(r == UV_EINVAL);

  r = uv_fs_link(NULL, NULL, NULL, NULL, NULL);
  ASSERT(r == UV_EINVAL);

  r = uv_fs_symlink(NULL, NULL, NULL, NULL, 0, NULL);
  ASSERT(r == UV_EINVAL);

  r = uv_fs_readlink(NULL, NULL, NULL, NULL);
  ASSERT(r == UV_EINVAL);

  r = uv_fs_realpath(NULL, NULL, NULL, NULL);
  ASSERT(r == UV_EINVAL);

  r = uv_fs_chown(NULL, NULL, NULL, 0, 0, NULL);
  ASSERT(r == UV_EINVAL);

  r = uv_fs_fchown(NULL, NULL, 0, 0, 0, NULL);
  ASSERT(r == UV_EINVAL);

  r = uv_fs_stat(NULL, NULL, NULL, NULL);
  ASSERT(r == UV_EINVAL);

  r = uv_fs_lstat(NULL, NULL, NULL, NULL);
  ASSERT(r == UV_EINVAL);

  r = uv_fs_fstat(NULL, NULL, 0, NULL);
  ASSERT(r == UV_EINVAL);

  r = uv_fs_rename(NULL, NULL, NULL, NULL, NULL);
  ASSERT(r == UV_EINVAL);

  r = uv_fs_fsync(NULL, NULL, 0, NULL);
  ASSERT(r == UV_EINVAL);

  r = uv_fs_fdatasync(NULL, NULL, 0, NULL);
  ASSERT(r == UV_EINVAL);

  r = uv_fs_ftruncate(NULL, NULL, 0, 0, NULL);
  ASSERT(r == UV_EINVAL);

  r = uv_fs_copyfile(NULL, NULL, NULL, NULL, 0, NULL);
  ASSERT(r == UV_EINVAL);

  r = uv_fs_sendfile(NULL, NULL, 0, 0, 0, 0, NULL);
  ASSERT(r == UV_EINVAL);

  r = uv_fs_access(NULL, NULL, NULL, 0, NULL);
  ASSERT(r == UV_EINVAL);

  r = uv_fs_chmod(NULL, NULL, NULL, 0, NULL);
  ASSERT(r == UV_EINVAL);

  r = uv_fs_fchmod(NULL, NULL, 0, 0, NULL);
  ASSERT(r == UV_EINVAL);

  r = uv_fs_utime(NULL, NULL, NULL, 0.0, 0.0, NULL);
  ASSERT(r == UV_EINVAL);

  r = uv_fs_futime(NULL, NULL, 0, 0.0, 0.0, NULL);
  ASSERT(r == UV_EINVAL);

  r = uv_fs_statfs(NULL, NULL, NULL, NULL);
  ASSERT(r == UV_EINVAL);

  /* This should be a no-op. */
  uv_fs_req_cleanup(NULL);

  return 0;
}

#ifdef _WIN32
TEST_IMPL(fs_exclusive_sharing_mode) {
  int r;

  /* Setup. */
  unlink("test_file");

  ASSERT(UV_FS_O_EXLOCK > 0);

  r = uv_fs_open(NULL,
                 &open_req1,
                 "test_file",
                 O_RDWR | O_CREAT | UV_FS_O_EXLOCK,
                 S_IWUSR | S_IRUSR,
                 NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  r = uv_fs_open(NULL,
                 &open_req2,
                 "test_file",
                 O_RDONLY | UV_FS_O_EXLOCK,
                 S_IWUSR | S_IRUSR,
                 NULL);
  ASSERT(r < 0);
  ASSERT(open_req2.result < 0);
  uv_fs_req_cleanup(&open_req2);

  r = uv_fs_close(NULL, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_open(NULL,
                 &open_req2,
                 "test_file",
                 O_RDONLY | UV_FS_O_EXLOCK,
                 S_IWUSR | S_IRUSR,
                 NULL);
  ASSERT(r >= 0);
  ASSERT(open_req2.result >= 0);
  uv_fs_req_cleanup(&open_req2);

  r = uv_fs_close(NULL, &close_req, open_req2.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  /* Cleanup */
  unlink("test_file");

  MAKE_VALGRIND_HAPPY();
  return 0;
}
#endif

#ifdef _WIN32
TEST_IMPL(fs_file_flag_no_buffering) {
  int r;

  /* Setup. */
  unlink("test_file");

  ASSERT(UV_FS_O_APPEND > 0);
  ASSERT(UV_FS_O_CREAT > 0);
  ASSERT(UV_FS_O_DIRECT > 0);
  ASSERT(UV_FS_O_RDWR > 0);

  /* FILE_APPEND_DATA must be excluded from FILE_GENERIC_WRITE: */
  r = uv_fs_open(NULL,
                 &open_req1,
                 "test_file",
                 UV_FS_O_RDWR | UV_FS_O_CREAT | UV_FS_O_DIRECT,
                 S_IWUSR | S_IRUSR,
                 NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  r = uv_fs_close(NULL, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  /* FILE_APPEND_DATA and FILE_FLAG_NO_BUFFERING are mutually exclusive: */
  r = uv_fs_open(NULL,
                 &open_req2,
                 "test_file",
                 UV_FS_O_APPEND | UV_FS_O_DIRECT,
                 S_IWUSR | S_IRUSR,
                 NULL);
  ASSERT(r == UV_EINVAL);
  ASSERT(open_req2.result == UV_EINVAL);
  uv_fs_req_cleanup(&open_req2);

  /* Cleanup */
  unlink("test_file");

  MAKE_VALGRIND_HAPPY();
  return 0;
}
#endif

#ifdef _WIN32
int call_icacls(const char* command, ...) {
    char icacls_command[1024];
    va_list args;

    va_start(args, command);
    vsnprintf(icacls_command, ARRAYSIZE(icacls_command), command, args);
    va_end(args);
    return system(icacls_command);
}

TEST_IMPL(fs_open_readonly_acl) {
    uv_passwd_t pwd;
    uv_fs_t req;
    int r;

    /*
        Based on Node.js test from
        https://github.com/nodejs/node/commit/3ba81e34e86a5c32658e218cb6e65b13e8326bc5

        If anything goes wrong, you can delte the test_fle_icacls with:

            icacls test_file_icacls /remove "%USERNAME%" /inheritance:e
            attrib -r test_file_icacls
            del test_file_icacls
    */

    /* Setup - clear the ACL and remove the file */
    loop = uv_default_loop();
    r = uv_os_get_passwd(&pwd);
    ASSERT(r == 0);
    call_icacls("icacls test_file_icacls /remove \"%s\" /inheritance:e",
                pwd.username);
    uv_fs_chmod(loop, &req, "test_file_icacls", S_IWUSR, NULL);
    unlink("test_file_icacls");

    /* Create the file */
    r = uv_fs_open(loop,
                   &open_req1,
                   "test_file_icacls",
                   O_RDONLY | O_CREAT,
                   S_IRUSR,
                   NULL);
    ASSERT(r >= 0);
    ASSERT(open_req1.result >= 0);
    uv_fs_req_cleanup(&open_req1);
    r = uv_fs_close(NULL, &close_req, open_req1.result, NULL);
    ASSERT(r == 0);
    ASSERT(close_req.result == 0);
    uv_fs_req_cleanup(&close_req);

    /* Set up ACL */
    r = call_icacls("icacls test_file_icacls /inheritance:r /remove \"%s\"",
                    pwd.username);
    if (r != 0) {
        goto acl_cleanup;
    }
    r = call_icacls("icacls test_file_icacls /grant \"%s\":RX", pwd.username);
    if (r != 0) {
        goto acl_cleanup;
    }

    /* Try opening the file */
    r = uv_fs_open(NULL, &open_req1, "test_file_icacls", O_RDONLY, 0, NULL);
    if (r < 0) {
        goto acl_cleanup;
    }
    uv_fs_req_cleanup(&open_req1);
    r = uv_fs_close(NULL, &close_req, open_req1.result, NULL);
    if (r != 0) {
        goto acl_cleanup;
    }
    uv_fs_req_cleanup(&close_req);

 acl_cleanup:
    /* Cleanup */
    call_icacls("icacls test_file_icacls /remove \"%s\" /inheritance:e",
                pwd.username);
    unlink("test_file_icacls");
    uv_os_free_passwd(&pwd);
    ASSERT(r == 0);
    MAKE_VALGRIND_HAPPY();
    return 0;
}
#endif

#ifdef _WIN32
TEST_IMPL(fs_fchmod_archive_readonly) {
    uv_fs_t req;
    uv_file file;
    int r;
    /* Test clearing read-only flag from files with Archive flag cleared */

    /* Setup*/
    unlink("test_file");
    r = uv_fs_open(NULL,
                   &req,
                   "test_file",
                   O_WRONLY | O_CREAT,
                   S_IWUSR | S_IRUSR,
                   NULL);
    ASSERT(r >= 0);
    ASSERT(req.result >= 0);
    file = req.result;
    uv_fs_req_cleanup(&req);
    r = uv_fs_close(NULL, &req, file, NULL);
    ASSERT(r == 0);
    uv_fs_req_cleanup(&req);
    /* Make the file read-only and clear archive flag */
    r = SetFileAttributes("test_file", FILE_ATTRIBUTE_READONLY);
    ASSERT(r != 0);
    check_permission("test_file", 0400);
    /* Try fchmod */
    r = uv_fs_open(NULL, &req, "test_file", O_RDONLY, 0, NULL);
    ASSERT(r >= 0);
    ASSERT(req.result >= 0);
    file = req.result;
    uv_fs_req_cleanup(&req);
    r = uv_fs_fchmod(NULL, &req, file, S_IWUSR, NULL);
    ASSERT(r == 0);
    ASSERT(req.result == 0);
    uv_fs_req_cleanup(&req);
    r = uv_fs_close(NULL, &req, file, NULL);
    ASSERT(r == 0);
    uv_fs_req_cleanup(&req);
    check_permission("test_file", S_IWUSR);

    /* Restore Archive flag for rest of the tests */
    r = SetFileAttributes("test_file", FILE_ATTRIBUTE_ARCHIVE);
    ASSERT(r != 0);

    return 0;
}

TEST_IMPL(fs_invalid_mkdir_name) {
  uv_loop_t* loop;
  uv_fs_t req;
  int r;

  loop = uv_default_loop();
  r = uv_fs_mkdir(loop, &req, "invalid>", 0, NULL);
  ASSERT(r == UV_EINVAL);
  ASSERT_EQ(UV_EINVAL, uv_fs_mkdir(loop, &req, "test:lol", 0, NULL));

  return 0;
}
#endif

TEST_IMPL(fs_statfs) {
  uv_fs_t req;
  int r;

  loop = uv_default_loop();

  /* Test the synchronous version. */
  r = uv_fs_statfs(NULL, &req, ".", NULL);
  ASSERT(r == 0);
  statfs_cb(&req);
  ASSERT(statfs_cb_count == 1);

  /* Test the asynchronous version. */
  r = uv_fs_statfs(loop, &req, ".", statfs_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(statfs_cb_count == 2);

  return 0;
}

TEST_IMPL(fs_get_system_error) {
  uv_fs_t req;
  int r;
  int system_error;

  r = uv_fs_statfs(NULL, &req, "non_existing_file", NULL);
  ASSERT(r != 0);

  system_error = uv_fs_get_system_error(&req);
#ifdef _WIN32
  ASSERT(system_error == ERROR_FILE_NOT_FOUND);
#else
  ASSERT(system_error == ENOENT);
#endif

  return 0;
}
