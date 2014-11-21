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
    defined(__APPLE__) || defined(_AIX)
#include <unistd.h> /* unlink, rmdir, etc. */
#else
# include <direct.h>
# include <io.h>
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
static int link_cb_count;
static int symlink_cb_count;
static int readlink_cb_count;
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

static void check_permission(const char* filename, unsigned int mode) {
  int r;
  uv_fs_t req;
  uv_stat_t* s;

  r = uv_fs_stat(uv_default_loop(), &req, filename, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);

  s = &req.statbuf;
#ifdef _WIN32
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

static void chown_root_cb(uv_fs_t* req) {
  ASSERT(req->fs_type == UV_FS_CHOWN);
#ifdef _WIN32
  /* On windows, chown is a no-op and always succeeds. */
  ASSERT(req->result == 0);
#else
  /* On unix, chown'ing the root directory is not allowed -
   * unless you're root, of course.
   */
  if (geteuid() == 0)
    ASSERT(req->result == 0);
  else
    ASSERT(req->result == UV_EPERM);
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
  r = uv_fs_stat(uv_default_loop(), &stat_req, req->path, NULL);
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


static void scandir_cb(uv_fs_t* req) {
  uv_dirent_t dent;
  ASSERT(req == &scandir_req);
  ASSERT(req->fs_type == UV_FS_SCANDIR);
  ASSERT(req->result == 2);
  ASSERT(req->ptr);

  while (UV_EOF != uv_fs_scandir_next(req, &dent)) {
    ASSERT(strcmp(dent.name, "file1") == 0 || strcmp(dent.name, "file2") == 0);
    ASSERT(dent.type == UV_DIRENT_FILE || dent.type == UV_DIRENT_UNKNOWN);
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

  r = uv_fs_open(loop, &req, "does_not_exist", O_RDONLY, 0, NULL);
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

  r = uv_fs_open(loop, &req, name, O_RDONLY, 0, NULL);
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
  uv_fs_symlink(loop, &req, "test_symlink", "test_symlink", 0, NULL);
  uv_fs_req_cleanup(&req);

  r = uv_fs_open(loop, &req, "test_symlink", O_RDONLY, 0, NULL);
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

  ASSERT(s->st_atim.tv_sec  == atime);
  ASSERT(s->st_mtim.tv_sec  == mtime);

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
  r = uv_fs_write(loop, &write_req, open_req1.result, &iov, 1, -1, NULL);
  ASSERT(r >= 0);
  ASSERT(write_req.result >= 0);
  uv_fs_req_cleanup(&write_req);

  r = uv_fs_close(loop, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_open(loop, &open_req1, "test_file", O_RDWR, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(loop, &read_req, open_req1.result, &iov, 1, -1, NULL);
  ASSERT(r >= 0);
  ASSERT(read_req.result >= 0);
  ASSERT(strcmp(buf, test_buf) == 0);
  uv_fs_req_cleanup(&read_req);

  r = uv_fs_ftruncate(loop, &ftruncate_req, open_req1.result, 7, NULL);
  ASSERT(r == 0);
  ASSERT(ftruncate_req.result == 0);
  uv_fs_req_cleanup(&ftruncate_req);

  r = uv_fs_close(loop, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_rename(loop, &rename_req, "test_file", "test_file2", NULL);
  ASSERT(r == 0);
  ASSERT(rename_req.result == 0);
  uv_fs_req_cleanup(&rename_req);

  r = uv_fs_open(loop, &open_req1, "test_file2", O_RDONLY, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  memset(buf, 0, sizeof(buf));
  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(loop, &read_req, open_req1.result, &iov, 1, -1,
      NULL);
  ASSERT(r >= 0);
  ASSERT(read_req.result >= 0);
  ASSERT(strcmp(buf, "test-bu") == 0);
  uv_fs_req_cleanup(&read_req);

  r = uv_fs_close(loop, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_unlink(loop, &unlink_req, "test_file2", NULL);
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

  r = uv_fs_open(loop, &open_req1, "test_file", O_WRONLY | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  iov = uv_buf_init(NULL, 0);
  r = uv_fs_write(loop, &write_req, open_req1.result, &iov, 1, -1, NULL);
  ASSERT(r == 0);
  ASSERT(write_req.result == 0);
  uv_fs_req_cleanup(&write_req);

  r = uv_fs_close(loop, &close_req, open_req1.result, NULL);
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
  r = uv_fs_open(loop, &open_req1, "test_dir/file1", O_WRONLY | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  uv_fs_req_cleanup(&open_req1);
  r = uv_fs_close(loop, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_open(loop, &open_req1, "test_dir/file2", O_WRONLY | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  uv_fs_req_cleanup(&open_req1);
  r = uv_fs_close(loop, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_scandir(loop, &scandir_req, "test_dir", 0, scandir_cb);
  ASSERT(r == 0);

  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(scandir_cb_count == 1);

  /* sync uv_fs_scandir */
  r = uv_fs_scandir(loop, &scandir_req, "test_dir", 0, NULL);
  ASSERT(r == 2);
  ASSERT(scandir_req.result == 2);
  ASSERT(scandir_req.ptr);
  while (UV_EOF != uv_fs_scandir_next(&scandir_req, &dent)) {
    ASSERT(strcmp(dent.name, "file1") == 0 || strcmp(dent.name, "file2") == 0);
    ASSERT(dent.type == UV_DIRENT_FILE || dent.type == UV_DIRENT_UNKNOWN);
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
  r = uv_fs_open(loop, &open_req1, "test_file", O_RDWR, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  r = uv_fs_open(loop, &open_req2, "test_file2", O_WRONLY | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req2.result >= 0);
  uv_fs_req_cleanup(&open_req2);

  r = uv_fs_sendfile(loop, &sendfile_req, open_req2.result, open_req1.result,
      0, 131072, sendfile_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);

  ASSERT(sendfile_cb_count == 1);

  r = uv_fs_close(loop, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&close_req);
  r = uv_fs_close(loop, &close_req, open_req2.result, NULL);
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
  r = uv_fs_mkdtemp(loop, &mkdtemp_req2, path_template, NULL);
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

  r = uv_fs_open(loop, &req, "test_file", O_RDWR | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  file = req.result;
  uv_fs_req_cleanup(&req);

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = uv_fs_write(loop, &req, file, &iov, 1, -1, NULL);
  ASSERT(r == sizeof(test_buf));
  ASSERT(req.result == sizeof(test_buf));
  uv_fs_req_cleanup(&req);

  r = uv_fs_fstat(loop, &req, file, NULL);
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
#elif defined(__sun) || \
      defined(_BSD_SOURCE) || \
      defined(_SVID_SOURCE) || \
      defined(_XOPEN_SOURCE)
  ASSERT(s->st_atim.tv_sec == t.st_atim.tv_sec);
  ASSERT(s->st_atim.tv_nsec == t.st_atim.tv_nsec);
  ASSERT(s->st_mtim.tv_sec == t.st_mtim.tv_sec);
  ASSERT(s->st_mtim.tv_nsec == t.st_mtim.tv_nsec);
  ASSERT(s->st_ctim.tv_sec == t.st_ctim.tv_sec);
  ASSERT(s->st_ctim.tv_nsec == t.st_ctim.tv_nsec);
# if defined(__DragonFly__)  || \
      defined(__FreeBSD__)    || \
      defined(__OpenBSD__)    || \
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


  r = uv_fs_close(loop, &req, file, NULL);
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

  loop = uv_default_loop();

  /* File should not exist */
  r = uv_fs_access(loop, &req, "test_file", F_OK, NULL);
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
  r = uv_fs_open(loop, &req, "test_file", O_RDWR | O_CREAT,
                 S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  file = req.result;
  uv_fs_req_cleanup(&req);

  /* File should exist */
  r = uv_fs_access(loop, &req, "test_file", F_OK, NULL);
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
  r = uv_fs_close(loop, &req, file, NULL);
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


TEST_IMPL(fs_chmod) {
  int r;
  uv_fs_t req;
  uv_file file;

  /* Setup. */
  unlink("test_file");

  loop = uv_default_loop();

  r = uv_fs_open(loop, &req, "test_file", O_RDWR | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  file = req.result;
  uv_fs_req_cleanup(&req);

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = uv_fs_write(loop, &req, file, &iov, 1, -1, NULL);
  ASSERT(r == sizeof(test_buf));
  ASSERT(req.result == sizeof(test_buf));
  uv_fs_req_cleanup(&req);

#ifndef _WIN32
  /* Make the file write-only */
  r = uv_fs_chmod(loop, &req, "test_file", 0200, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  uv_fs_req_cleanup(&req);

  check_permission("test_file", 0200);
#endif

  /* Make the file read-only */
  r = uv_fs_chmod(loop, &req, "test_file", 0400, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  uv_fs_req_cleanup(&req);

  check_permission("test_file", 0400);

  /* Make the file read+write with sync uv_fs_fchmod */
  r = uv_fs_fchmod(loop, &req, file, 0600, NULL);
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


TEST_IMPL(fs_chown) {
  int r;
  uv_fs_t req;
  uv_file file;

  /* Setup. */
  unlink("test_file");

  loop = uv_default_loop();

  r = uv_fs_open(loop, &req, "test_file", O_RDWR | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  file = req.result;
  uv_fs_req_cleanup(&req);

  /* sync chown */
  r = uv_fs_chown(loop, &req, "test_file", -1, -1, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  uv_fs_req_cleanup(&req);

  /* sync fchown */
  r = uv_fs_fchown(loop, &req, file, -1, -1, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  uv_fs_req_cleanup(&req);

  /* async chown */
  r = uv_fs_chown(loop, &req, "test_file", -1, -1, chown_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(chown_cb_count == 1);

  /* chown to root (fail) */
  chown_cb_count = 0;
  r = uv_fs_chown(loop, &req, "test_file", 0, 0, chown_root_cb);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(chown_cb_count == 1);

  /* async fchown */
  r = uv_fs_fchown(loop, &req, file, -1, -1, fchown_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(fchown_cb_count == 1);

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

  r = uv_fs_open(loop, &req, "test_file", O_RDWR | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  file = req.result;
  uv_fs_req_cleanup(&req);

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = uv_fs_write(loop, &req, file, &iov, 1, -1, NULL);
  ASSERT(r == sizeof(test_buf));
  ASSERT(req.result == sizeof(test_buf));
  uv_fs_req_cleanup(&req);

  close(file);

  /* sync link */
  r = uv_fs_link(loop, &req, "test_file", "test_file_link", NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  uv_fs_req_cleanup(&req);

  r = uv_fs_open(loop, &req, "test_file_link", O_RDWR, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  link = req.result;
  uv_fs_req_cleanup(&req);

  memset(buf, 0, sizeof(buf));
  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(loop, &req, link, &iov, 1, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  ASSERT(strcmp(buf, test_buf) == 0);

  close(link);

  /* async link */
  r = uv_fs_link(loop, &req, "test_file", "test_file_link2", link_cb);
  ASSERT(r == 0);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(link_cb_count == 1);

  r = uv_fs_open(loop, &req, "test_file_link2", O_RDWR, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  link = req.result;
  uv_fs_req_cleanup(&req);

  memset(buf, 0, sizeof(buf));
  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(loop, &req, link, &iov, 1, 0, NULL);
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

  ASSERT(UV_ENOENT == uv_fs_readlink(loop, &req, "no_such_file", NULL));
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

  /* Setup. */
  unlink("test_file");
  unlink("test_file_symlink");
  unlink("test_file_symlink2");
  unlink("test_file_symlink_symlink");
  unlink("test_file_symlink2_symlink");

  loop = uv_default_loop();

  r = uv_fs_open(loop, &req, "test_file", O_RDWR | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  file = req.result;
  uv_fs_req_cleanup(&req);

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = uv_fs_write(loop, &req, file, &iov, 1, -1, NULL);
  ASSERT(r == sizeof(test_buf));
  ASSERT(req.result == sizeof(test_buf));
  uv_fs_req_cleanup(&req);

  close(file);

  /* sync symlink */
  r = uv_fs_symlink(loop, &req, "test_file", "test_file_symlink", 0, NULL);
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

  r = uv_fs_open(loop, &req, "test_file_symlink", O_RDWR, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  link = req.result;
  uv_fs_req_cleanup(&req);

  memset(buf, 0, sizeof(buf));
  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(loop, &req, link, &iov, 1, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  ASSERT(strcmp(buf, test_buf) == 0);

  close(link);

  r = uv_fs_symlink(loop,
                    &req,
                    "test_file_symlink",
                    "test_file_symlink_symlink",
                    0,
                    NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&req);

  r = uv_fs_readlink(loop, &req, "test_file_symlink_symlink", NULL);
  ASSERT(r == 0);
  ASSERT(strcmp(req.ptr, "test_file_symlink") == 0);
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

  r = uv_fs_open(loop, &req, "test_file_symlink2", O_RDWR, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  link = req.result;
  uv_fs_req_cleanup(&req);

  memset(buf, 0, sizeof(buf));
  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(loop, &req, link, &iov, 1, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  ASSERT(strcmp(buf, test_buf) == 0);

  close(link);

  r = uv_fs_symlink(loop,
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


TEST_IMPL(fs_symlink_dir) {
  uv_fs_t req;
  int r;
  char* test_dir;
  uv_dirent_t dent;

  /* set-up */
  unlink("test_dir/file1");
  unlink("test_dir/file2");
  rmdir("test_dir");
  rmdir("test_dir_symlink");

  loop = uv_default_loop();

  uv_fs_mkdir(loop, &req, "test_dir", 0777, NULL);
  uv_fs_req_cleanup(&req);

#ifdef _WIN32
  {
    static char src_path_buf[PATHMAX];
    size_t size;
    size = sizeof(src_path_buf);
    strcpy(src_path_buf, "\\\\?\\");
    uv_cwd(src_path_buf + 4, &size);
    strcat(src_path_buf, "\\test_dir\\");
    test_dir = src_path_buf;
  }
#else
  test_dir = "test_dir";
#endif

  r = uv_fs_symlink(loop, &req, test_dir, "test_dir_symlink",
    UV_FS_SYMLINK_JUNCTION, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  uv_fs_req_cleanup(&req);

  r = uv_fs_stat(loop, &req, "test_dir_symlink", NULL);
  ASSERT(r == 0);
  ASSERT(((uv_stat_t*)req.ptr)->st_mode & S_IFDIR);
  uv_fs_req_cleanup(&req);

  r = uv_fs_lstat(loop, &req, "test_dir_symlink", NULL);
  ASSERT(r == 0);
  ASSERT(((uv_stat_t*)req.ptr)->st_mode & S_IFLNK);
#ifdef _WIN32
  ASSERT(((uv_stat_t*)req.ptr)->st_size == strlen(test_dir + 4));
#else
  ASSERT(((uv_stat_t*)req.ptr)->st_size == strlen(test_dir));
#endif
  uv_fs_req_cleanup(&req);

  r = uv_fs_readlink(loop, &req, "test_dir_symlink", NULL);
  ASSERT(r == 0);
#ifdef _WIN32
  ASSERT(strcmp(req.ptr, test_dir + 4) == 0);
#else
  ASSERT(strcmp(req.ptr, test_dir) == 0);
#endif
  uv_fs_req_cleanup(&req);

  r = uv_fs_open(loop, &open_req1, "test_dir/file1", O_WRONLY | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  uv_fs_req_cleanup(&open_req1);
  r = uv_fs_close(loop, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_open(loop, &open_req1, "test_dir/file2", O_WRONLY | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  uv_fs_req_cleanup(&open_req1);
  r = uv_fs_close(loop, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_scandir(loop, &scandir_req, "test_dir_symlink", 0, NULL);
  ASSERT(r == 2);
  ASSERT(scandir_req.result == 2);
  ASSERT(scandir_req.ptr);
  while (UV_EOF != uv_fs_scandir_next(&scandir_req, &dent)) {
    ASSERT(strcmp(dent.name, "file1") == 0 || strcmp(dent.name, "file2") == 0);
    ASSERT(dent.type == UV_DIRENT_FILE || dent.type == UV_DIRENT_UNKNOWN);
  }
  uv_fs_req_cleanup(&scandir_req);
  ASSERT(!scandir_req.ptr);

  /* unlink will remove the directory symlink */
  r = uv_fs_unlink(loop, &req, "test_dir_symlink", NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&req);

  r = uv_fs_scandir(loop, &scandir_req, "test_dir_symlink", 0, NULL);
  ASSERT(r == UV_ENOENT);
  uv_fs_req_cleanup(&scandir_req);

  r = uv_fs_scandir(loop, &scandir_req, "test_dir", 0, NULL);
  ASSERT(r == 2);
  ASSERT(scandir_req.result == 2);
  ASSERT(scandir_req.ptr);
  while (UV_EOF != uv_fs_scandir_next(&scandir_req, &dent)) {
    ASSERT(strcmp(dent.name, "file1") == 0 || strcmp(dent.name, "file2") == 0);
    ASSERT(dent.type == UV_DIRENT_FILE || dent.type == UV_DIRENT_UNKNOWN);
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
  r = uv_fs_open(loop, &req, path, O_RDWR | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  uv_fs_req_cleanup(&req);
  close(r);

  atime = mtime = 400497753; /* 1982-09-10 11:22:33 */

  r = uv_fs_utime(loop, &req, path, atime, mtime, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  uv_fs_req_cleanup(&req);

  r = uv_fs_stat(loop, &req, path, NULL);
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
  uv_loop_t* loop = uv_default_loop();

  r = uv_fs_stat(loop, &stat_req, "\\", NULL);
  ASSERT(r == 0);

  r = uv_fs_stat(loop, &stat_req, "..\\..\\..\\..\\..\\..\\..", NULL);
  ASSERT(r == 0);

  r = uv_fs_stat(loop, &stat_req, "..", NULL);
  ASSERT(r == 0);

  r = uv_fs_stat(loop, &stat_req, "..\\", NULL);
  ASSERT(r == 0);

  /* stats the current directory on c: */
  r = uv_fs_stat(loop, &stat_req, "c:", NULL);
  ASSERT(r == 0);

  r = uv_fs_stat(loop, &stat_req, "c:\\", NULL);
  ASSERT(r == 0);

  r = uv_fs_stat(loop, &stat_req, "\\\\?\\C:\\", NULL);
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

  /* Setup. */
  loop = uv_default_loop();
  unlink(path);
  r = uv_fs_open(loop, &req, path, O_RDWR | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  uv_fs_req_cleanup(&req);
  close(r);

  atime = mtime = 400497753; /* 1982-09-10 11:22:33 */

  r = uv_fs_open(loop, &req, path, O_RDWR, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  file = req.result; /* FIXME probably not how it's supposed to be used */
  uv_fs_req_cleanup(&req);

  r = uv_fs_futime(loop, &req, file, atime, mtime, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  uv_fs_req_cleanup(&req);

  r = uv_fs_stat(loop, &req, path, NULL);
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
}


TEST_IMPL(fs_stat_missing_path) {
  uv_fs_t req;
  int r;

  loop = uv_default_loop();

  r = uv_fs_stat(loop, &req, "non_existent_file", NULL);
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

  uv_fs_mkdir(loop, &req, path, 0777, NULL);
  uv_fs_req_cleanup(&req);

  /* Fill the req to ensure that required fields are cleaned up */
  memset(&req, 0xdb, sizeof(req));

  r = uv_fs_scandir(loop, &req, path, 0, NULL);
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

  uv_fs_rmdir(loop, &req, path, NULL);
  uv_fs_req_cleanup(&req);

  MAKE_VALGRIND_HAPPY();
  return 0;
}


TEST_IMPL(fs_scandir_file) {
  const char* path;
  int r;

  path = "test/fixtures/empty_file";
  loop = uv_default_loop();

  r = uv_fs_scandir(loop, &scandir_req, path, 0, NULL);
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

  r = uv_fs_open(loop, &req, path, O_RDONLY, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(req.result >= 0);
  ASSERT(req.ptr == NULL);
  file = r;
  uv_fs_req_cleanup(&req);

  r = uv_fs_close(loop, &req, file, NULL);
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

  r = uv_fs_open(loop, &open_req1, "test_file", O_WRONLY | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = uv_fs_write(loop, &write_req, open_req1.result, &iov, 1, -1, NULL);
  ASSERT(r >= 0);
  ASSERT(write_req.result >= 0);
  uv_fs_req_cleanup(&write_req);

  r = uv_fs_close(loop, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_open(loop, &open_req1, "test_file", O_RDWR | O_APPEND, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = uv_fs_write(loop, &write_req, open_req1.result, &iov, 1, -1, NULL);
  ASSERT(r >= 0);
  ASSERT(write_req.result >= 0);
  uv_fs_req_cleanup(&write_req);

  r = uv_fs_close(loop, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_open(loop, &open_req1, "test_file", O_RDONLY, S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(loop, &read_req, open_req1.result, &iov, 1, -1,
      NULL);
  printf("read = %d\n", r);
  ASSERT(r == 26);
  ASSERT(read_req.result == 26);
  ASSERT(memcmp(buf,
                "test-buffer\n\0test-buffer\n\0",
                sizeof("test-buffer\n\0test-buffer\n\0") - 1) == 0);
  uv_fs_req_cleanup(&read_req);

  r = uv_fs_close(loop, &close_req, open_req1.result, NULL);
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

  r = uv_fs_open(loop, &open_req1, "test_file", O_WRONLY | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = uv_fs_write(loop, &write_req, open_req1.result, &iov, 1, -1, NULL);
  ASSERT(r >= 0);
  ASSERT(write_req.result >= 0);
  uv_fs_req_cleanup(&write_req);

  r = uv_fs_close(loop, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_open(loop, &open_req1, "test_file2", O_WRONLY | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  r = uv_fs_close(loop, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_rename(loop, &rename_req, "test_file", "test_file2", NULL);
  ASSERT(r == 0);
  ASSERT(rename_req.result == 0);
  uv_fs_req_cleanup(&rename_req);

  r = uv_fs_open(loop, &open_req1, "test_file2", O_RDONLY, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  memset(buf, 0, sizeof(buf));
  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(loop, &read_req, open_req1.result, &iov, 1, -1,
      NULL);
  ASSERT(r >= 0);
  ASSERT(read_req.result >= 0);
  ASSERT(strcmp(buf, test_buf) == 0);
  uv_fs_req_cleanup(&read_req);

  r = uv_fs_close(loop, &close_req, open_req1.result, NULL);
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
  int r;

  /* Setup. */
  unlink("test_file");

  loop = uv_default_loop();

  r = uv_fs_open(loop, &open_req1, "test_file", O_WRONLY | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  iov = uv_buf_init(test_buf, sizeof(test_buf));
  r = uv_fs_write(loop, &write_req, open_req1.result, &iov, 1, -1, NULL);
  ASSERT(r >= 0);
  ASSERT(write_req.result >= 0);
  uv_fs_req_cleanup(&write_req);

  r = uv_fs_close(loop, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_open(loop, &open_req1, "test_file", O_RDONLY, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  memset(buf, 0, sizeof(buf));
  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(loop, &read_req, open_req1.result, &iov, 1, -1, NULL);
  ASSERT(r >= 0);
  ASSERT(read_req.result >= 0);
  ASSERT(strcmp(buf, test_buf) == 0);
  uv_fs_req_cleanup(&read_req);

  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(loop, &read_req, open_req1.result, &iov, 1,
                 read_req.result, NULL);
  ASSERT(r == 0);
  ASSERT(read_req.result == 0);
  uv_fs_req_cleanup(&read_req);

  r = uv_fs_close(loop, &close_req, open_req1.result, NULL);
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

  r = uv_fs_open(loop, &open_req1, "test_file", O_WRONLY | O_CREAT,
      S_IWUSR | S_IRUSR, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  iovs[0] = uv_buf_init(test_buf, sizeof(test_buf));
  iovs[1] = uv_buf_init(test_buf2, sizeof(test_buf2));
  r = uv_fs_write(loop, &write_req, open_req1.result, iovs, 2, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(write_req.result >= 0);
  uv_fs_req_cleanup(&write_req);

  r = uv_fs_close(loop, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_open(loop, &open_req1, "test_file", O_RDONLY, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(open_req1.result >= 0);
  uv_fs_req_cleanup(&open_req1);

  memset(buf, 0, sizeof(buf));
  memset(buf2, 0, sizeof(buf2));
  /* Read the strings back to separate buffers. */
  iovs[0] = uv_buf_init(buf, sizeof(test_buf));
  iovs[1] = uv_buf_init(buf2, sizeof(test_buf2));
  r = uv_fs_read(loop, &read_req, open_req1.result, iovs, 2, 0, NULL);
  ASSERT(r >= 0);
  ASSERT(read_req.result >= 0);
  ASSERT(strcmp(buf, test_buf) == 0);
  ASSERT(strcmp(buf2, test_buf2) == 0);
  uv_fs_req_cleanup(&read_req);

  iov = uv_buf_init(buf, sizeof(buf));
  r = uv_fs_read(loop, &read_req, open_req1.result, &iov, 1,
                 read_req.result, NULL);
  ASSERT(r == 0);
  ASSERT(read_req.result == 0);
  uv_fs_req_cleanup(&read_req);

  r = uv_fs_close(loop, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result == 0);
  uv_fs_req_cleanup(&close_req);

  /* Cleanup */
  unlink("test_file");

  MAKE_VALGRIND_HAPPY();
  return 0;
}
