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

/* FIXME we shouldn't need to branch in this file */
#if defined(__unix__) || defined(__POSIX__) || \
    defined(__APPLE__) || defined(_AIX) || defined(__MVS__)
#include <unistd.h> /* unlink, rmdir, etc. */
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
#define PATHMAX 1024

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
/*
 * This tag and guid have no special meaning, and don't conflict with
 * reserved ids.
*/
static unsigned REPARSE_TAG = 0x9913;
static GUID REPARSE_GUID = {
  0x1bf6205f, 0x46ae, 0x4527,
  0xb1, 0x0c, 0xc5, 0x09, 0xb7, 0x55, 0x22, 0x80 };
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
  ASSERT(req->result == 65546);
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

static void check_utime(const char* path, double atime, double mtime) {
  uv_stat_t* s;
  uv_fs_t req;
  int r;

  r = uv_fs_stat(loop, &req, path, NULL);
  ASSERT(r == 0);

  ASSERT(req.result == 0);
  s = &req.statbuf;

  ASSERT(s->st_atim.tv_sec + (s->st_atim.tv_nsec / 1000000000.0) == atime);
  ASSERT(s->st_mtim.tv_sec + (s->st_mtim.tv_nsec / 1000000000.0) == mtime);

  uv_fs_req_cleanup(&req);
}


static void utime_cb(uv_fs_t* req) {
  utime_check_t* c;

  ASSERT(req == &utime_req);
  ASSERT(req->result == 0);
  ASSERT(req->fs_type == UV_FS_UTIME);

  c = req->data;
  check_utime(c->path, c->atime, c->mtime);

  uv_fs_req_cleanup(req);
  utime_cb_count++;
}


static void futime_cb(uv_fs_t* req) {
  utime_check_t* c;

  ASSERT(req == &futime_req);
  ASSERT(req->result == 0);
  ASSERT(req->fs_type == UV_FS_FUTIME);

  c = req->data;
  check_utime(c->path, c->atime, c->mtime);

  uv_fs_req_cleanup(req);
  futime_cb_count++;
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


TEST_IMPL(fs_file_sync) {
  int r;

  /* Setup. */
  unlink("test_file");
  unlink("test_file2");

  loop = uv_default_loop();

  r = uv_fs_open(loop, &open_req1, "test_file", O_WRONLY | O_CREAT,
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

  r = uv_fs_open(NULL, &open_req1, "test_file", O_RDWR, 0, NULL);
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

  r = uv_fs_open(NULL, &open_req1, "test_file2", O_RDONLY, 0, NULL);
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

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(fs_file_write_null_buffer) {
  int r;

  /* Setup. */
  unlink("test_file");

  loop = uv_default_loop();

  r = uv_fs_open(NULL, &open_req1, "test_file", O_WRONLY | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
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


TEST_IMPL(fs_async_sendfile) {
  int f, r;
  struct stat s1, s2;

  loop = uv_default_loop();

  /* Setup. */
  unlink("test_file");
  unlink("test_file2");

  f = open("test_file", O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR);
  ASSERT(f != -1);

  r = write(f, "begin\n", 6);
  ASSERT(r == 6);

  r = lseek(f, 65536, SEEK_CUR);
  ASSERT(r == 65542);

  r = write(f, "end\n", 4);
  ASSERT(r != -1);

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
      0, 131072, sendfile_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);

  ASSERT(sendfile_cb_count == 1);

  r = uv_fs_close(NULL, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&close_req);
  r = uv_fs_close(NULL, &close_req, open_req2.result, NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&close_req);

  stat("test_file", &s1);
  stat("test_file2", &s2);
  ASSERT(65546 == s2.st_size && s1.st_size == s2.st_size);

  /* Cleanup. */
  unlink("test_file");
  unlink("test_file2");

  MAKE_VALGRIND_HAPPY();
  return 0;
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

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = uv_fs_write(NULL, &req, file, &iov, 1, -1, NULL);
  ASSERT(r == sizeof(test_buf));
  ASSERT(req.result == sizeof(test_buf));
  uv_fs_req_cleanup(&req);

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
  ASSERT(s->st_birthtim.tv_sec == t.st_birthtimespec.tv_sec);
  ASSERT(s->st_birthtim.tv_nsec == t.st_birthtimespec.tv_nsec);
  ASSERT(s->st_flags == t.st_flags);
  ASSERT(s->st_gen == t.st_gen);
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
  ASSERT(s->st_flags == t.st_flags);
  ASSERT(s->st_gen == t.st_gen);
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

  close(file);

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

  close(file);

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

  close(file);

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

  close(file);

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

  close(link);

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

  close(file);

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

  close(link);

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

  close(link);

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
  ASSERT(((uv_stat_t*)req.ptr)->st_size == strlen(test_dir));
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
  close(r);

  atime = mtime = 400497753; /* 1982-09-10 11:22:33 */

  /*
   * Test sub-second timestamps only on Windows (assuming NTFS). Some other
   * platforms support sub-second timestamps, but that support is filesystem-
   * dependent. Notably OS X (HFS Plus) does NOT support sub-second timestamps.
   */
#ifdef _WIN32
  mtime += 0.444;            /* 1982-09-10 11:22:33.444 */
#endif

  r = uv_fs_utime(NULL, &req, path, atime, mtime, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  uv_fs_req_cleanup(&req);

  r = uv_fs_stat(NULL, &req, path, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  check_utime(path, atime, mtime);
  uv_fs_req_cleanup(&req);

  atime = mtime = 1291404900; /* 2010-12-03 20:35:00 - mees <3 */
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
#if defined(_AIX) && !defined(_AIX71)
  RETURN_SKIP("futime is not implemented for AIX versions below 7.1");
#else
  utime_check_t checkme;
  const char* path = "test_file";
  double atime;
  double mtime;
  uv_file file;
  uv_fs_t req;
  int r;

  /* Setup. */
  loop = uv_default_loop();
  unlink(path);
  r = uv_fs_open(NULL, &req, path, O_RDWR | O_CREAT, S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  uv_fs_req_cleanup(&req);
  close(r);

  atime = mtime = 400497753; /* 1982-09-10 11:22:33 */

  /*
   * Test sub-second timestamps only on Windows (assuming NTFS). Some other
   * platforms support sub-second timestamps, but that support is filesystem-
   * dependent. Notably OS X (HFS Plus) does NOT support sub-second timestamps.
   */
#ifdef _WIN32
  mtime += 0.444;            /* 1982-09-10 11:22:33.444 */
#endif

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

  r = uv_fs_stat(NULL, &req, path, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  check_utime(path, atime, mtime);
  uv_fs_req_cleanup(&req);

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
#endif
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


TEST_IMPL(fs_file_open_append) {
  int r;

  /* Setup. */
  unlink("test_file");

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

  r = uv_fs_open(NULL, &open_req1, "test_file", O_RDWR | O_APPEND, 0, NULL);
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

  r = uv_fs_open(NULL, &open_req1, "test_file", O_RDONLY, S_IRUSR, NULL);
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


TEST_IMPL(fs_read_file_eof) {
#if defined(__CYGWIN__) || defined(__MSYS__)
  RETURN_SKIP("Cygwin pread at EOF may (incorrectly) return data!");
#endif
  int r;

  /* Setup. */
  unlink("test_file");

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

  r = uv_fs_open(NULL, &open_req1, "test_file", O_RDONLY, 0, NULL);
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

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(fs_write_multiple_bufs) {
  uv_buf_t iovs[2];
  int r;

  /* Setup. */
  unlink("test_file");

  loop = uv_default_loop();

  r = uv_fs_open(NULL, &open_req1, "test_file", O_WRONLY | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
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

  r = uv_fs_open(NULL, &open_req1, "test_file", O_RDONLY, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  memset(buf, 0, sizeof(buf));
  memset(buf2, 0, sizeof(buf2));
  /* Read the strings back to separate buffers. */
  iovs[0] = uv_buf_init(buf, sizeof(test_buf));
  iovs[1] = uv_buf_init(buf2, sizeof(test_buf2));
  r = uv_fs_read(NULL, &read_req, open_req1.result, iovs, 2, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(read_req.result >= 0);
  ASSERT(strcmp(buf, test_buf) == 0);
  ASSERT(strcmp(buf2, test_buf2) == 0);
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

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(fs_write_alotof_bufs) {
  const size_t iovcount = 54321;
  uv_buf_t* iovs;
  char* buffer;
  size_t index;
  int r;

  /* Setup. */
  unlink("test_file");

  loop = uv_default_loop();

  iovs = malloc(sizeof(*iovs) * iovcount);
  ASSERT(iovs != NULL);

  r = uv_fs_open(NULL,
                 &open_req1,
                 "test_file",
                 O_RDWR | O_CREAT,
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

  r = uv_fs_read(NULL, &read_req, open_req1.result, iovs, iovcount, 0, NULL);
  ASSERT(r >= 0);
  ASSERT((size_t)read_req.result == sizeof(test_buf) * iovcount);

  for (index = 0; index < iovcount; ++index)
    ASSERT(strncmp(buffer + index * sizeof(test_buf),
                   test_buf,
                   sizeof(test_buf)) == 0);

  uv_fs_req_cleanup(&read_req);
  free(buffer);

  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(NULL,
                 &read_req,
                 open_req1.result,
                 &iov,
                 1,
                 read_req.result,
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

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(fs_write_alotof_bufs_with_offset) {
  const size_t iovcount = 54321;
  uv_buf_t* iovs;
  char* buffer;
  size_t index;
  int r;
  int64_t offset;
  char* filler = "0123456789";
  int filler_len = strlen(filler);

  /* Setup. */
  unlink("test_file");

  loop = uv_default_loop();

  iovs = malloc(sizeof(*iovs) * iovcount);
  ASSERT(iovs != NULL);

  r = uv_fs_open(NULL,
                 &open_req1,
                 "test_file",
                 O_RDWR | O_CREAT,
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
         offset + (int64_t)(iovcount * sizeof(test_buf)));
  uv_fs_req_cleanup(&stat_req);

  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(NULL,
                 &read_req,
                 open_req1.result,
                 &iov,
                 1,
                 read_req.result + offset,
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

  MAKE_VALGRIND_HAPPY();
  return 0;
}


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
#endif
