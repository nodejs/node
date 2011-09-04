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

/* FIXME we shouldnt need to branch in this file */
#define UNIX (defined(__unix__) || defined(__POSIX__) || defined(__APPLE__))

#include "uv.h"
#include "task.h"

#include <string.h> /* memset */
#include <fcntl.h>
#include <sys/stat.h>


#if !UNIX
# include <direct.h>
# include <io.h>
# define unlink _unlink
# define rmdir _rmdir
# define stat _stat
# define open _open
# define write _write
# define lseek _lseek
# define close _close
#endif

static int close_cb_count;
static int create_cb_count;
static int open_cb_count;
static int read_cb_count;
static int write_cb_count;
static int unlink_cb_count;
static int mkdir_cb_count;
static int rmdir_cb_count;
static int readdir_cb_count;
static int stat_cb_count;
static int rename_cb_count;
static int fsync_cb_count;
static int fdatasync_cb_count;
static int ftruncate_cb_count;
static int sendfile_cb_count;
static int fstat_cb_count;
static int chmod_cb_count;
static int fchmod_cb_count;
static int chown_cb_count;
static int fchown_cb_count;

static uv_loop_t* loop;

static uv_fs_t open_req1;
static uv_fs_t open_req2;
static uv_fs_t read_req;
static uv_fs_t write_req;
static uv_fs_t unlink_req;
static uv_fs_t close_req;
static uv_fs_t mkdir_req;
static uv_fs_t rmdir_req;
static uv_fs_t readdir_req;
static uv_fs_t stat_req;
static uv_fs_t rename_req;
static uv_fs_t fsync_req;
static uv_fs_t fdatasync_req;
static uv_fs_t ftruncate_req;
static uv_fs_t sendfile_req;

static char buf[32];
static char test_buf[] = "test-buffer\n";


void check_permission(const char* filename, int mode) {
  int r;
  uv_fs_t req;
  struct stat* s;

  r = uv_fs_stat(uv_default_loop(), &req, filename, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);

  s = req.ptr;
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


static void fchmod_cb(uv_fs_t* req) {
  ASSERT(req->fs_type == UV_FS_FCHMOD);
  ASSERT(req->result == 0);
  fchmod_cb_count++;
  uv_fs_req_cleanup(req);
  check_permission("test_file", (int)req->data);
}


static void chmod_cb(uv_fs_t* req) {
  ASSERT(req->fs_type == UV_FS_CHMOD);
  ASSERT(req->result == 0);
  chmod_cb_count++;
  uv_fs_req_cleanup(req);
  check_permission("test_file", (int)req->data);
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


static void unlink_cb(uv_fs_t* req) {
  ASSERT(req == &unlink_req);
  ASSERT(req->fs_type == UV_FS_UNLINK);
  ASSERT(req->result != -1);
  unlink_cb_count++;
  uv_fs_req_cleanup(req);
}

static void fstat_cb(uv_fs_t* req) {
  struct stat* s = req->ptr;
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
  ASSERT(req->result != -1);
  close_cb_count++;
  uv_fs_req_cleanup(req);
  if (close_cb_count == 3) {
    r = uv_fs_unlink(loop, &unlink_req, "test_file2", unlink_cb);
  }
}


static void ftruncate_cb(uv_fs_t* req) {
  int r;
  ASSERT(req == &ftruncate_req);
  ASSERT(req->fs_type == UV_FS_FTRUNCATE);
  ASSERT(req->result != -1);
  ftruncate_cb_count++;
  uv_fs_req_cleanup(req);
  r = uv_fs_close(loop, &close_req, open_req1.result, close_cb);
}


static void read_cb(uv_fs_t* req) {
  int r;
  ASSERT(req == &read_req);
  ASSERT(req->fs_type == UV_FS_READ);
  ASSERT(req->result != -1);
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
}


static void open_cb(uv_fs_t* req) {
  int r;
  ASSERT(req == &open_req1);
  ASSERT(req->fs_type == UV_FS_OPEN);
  if (req->result < 0) {
    /* TODO get error with uv_last_error() */
    fprintf(stderr, "async open error: %d\n", req->errorno);
    ASSERT(0);
  }
  open_cb_count++;
  uv_fs_req_cleanup(req);
  memset(buf, 0, sizeof(buf));
  r = uv_fs_read(loop, &read_req, open_req1.result, buf, sizeof(buf), -1,
      read_cb);
}


static void fsync_cb(uv_fs_t* req) {
  int r;
  ASSERT(req == &fsync_req);
  ASSERT(req->fs_type == UV_FS_FSYNC);
  ASSERT(req->result != -1);
  fsync_cb_count++;
  uv_fs_req_cleanup(req);
  r = uv_fs_close(loop, &close_req, open_req1.result, close_cb);
}


static void fdatasync_cb(uv_fs_t* req) {
  int r;
  ASSERT(req == &fdatasync_req);
  ASSERT(req->fs_type == UV_FS_FDATASYNC);
  ASSERT(req->result != -1);
  fdatasync_cb_count++;
  uv_fs_req_cleanup(req);
  r = uv_fs_fsync(loop, &fsync_req, open_req1.result, fsync_cb);
}


static void write_cb(uv_fs_t* req) {
  int r;
  ASSERT(req == &write_req);
  ASSERT(req->fs_type == UV_FS_WRITE);
  ASSERT(req->result != -1);
  write_cb_count++;
  uv_fs_req_cleanup(req);
  r = uv_fs_fdatasync(loop, &fdatasync_req, open_req1.result, fdatasync_cb);
}


static void create_cb(uv_fs_t* req) {
  int r;
  ASSERT(req == &open_req1);
  ASSERT(req->fs_type == UV_FS_OPEN);
  ASSERT(req->result != -1);
  create_cb_count++;
  uv_fs_req_cleanup(req);
  r = uv_fs_write(loop, &write_req, req->result, test_buf, sizeof(test_buf),
      -1, write_cb);
}


static void rename_cb(uv_fs_t* req) {
  ASSERT(req == &rename_req);
  ASSERT(req->fs_type == UV_FS_RENAME);
  ASSERT(req->result != -1);
  rename_cb_count++;
  uv_fs_req_cleanup(req);
}


static void mkdir_cb(uv_fs_t* req) {
  ASSERT(req == &mkdir_req);
  ASSERT(req->fs_type == UV_FS_MKDIR);
  ASSERT(req->result != -1);
  mkdir_cb_count++;
  uv_fs_req_cleanup(req);
}


static void rmdir_cb(uv_fs_t* req) {
  ASSERT(req == &rmdir_req);
  ASSERT(req->fs_type == UV_FS_RMDIR);
  ASSERT(req->result != -1);
  rmdir_cb_count++;
  uv_fs_req_cleanup(req);
}


static void readdir_cb(uv_fs_t* req) {
  ASSERT(req == &readdir_req);
  ASSERT(req->fs_type == UV_FS_READDIR);
  ASSERT(req->result == 2);
  ASSERT(req->ptr);
  ASSERT(strcmp((const char*)req->ptr, "file1") == 0);
  ASSERT(strcmp((char*)req->ptr + strlen((const char*)req->ptr) + 1,
                "file2") == 0);
  readdir_cb_count++;
  uv_fs_req_cleanup(req);
  ASSERT(!req->ptr);
}


static void stat_cb(uv_fs_t* req) {
  ASSERT(req == &stat_req);
  ASSERT(req->fs_type == UV_FS_STAT || req->fs_type == UV_FS_LSTAT);
  ASSERT(req->result != -1);
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


TEST_IMPL(fs_file_async) {
  int r;

  /* Setup. */
  unlink("test_file");
  unlink("test_file2");

  uv_init();
  loop = uv_default_loop();

  r = uv_fs_open(loop, &open_req1, "test_file", O_WRONLY | O_CREAT,
      S_IREAD | S_IWRITE, create_cb);
  ASSERT(r == 0);
  uv_run(loop);

  ASSERT(create_cb_count == 1);
  ASSERT(write_cb_count == 1);
  ASSERT(fsync_cb_count == 1);
  ASSERT(fdatasync_cb_count == 1);
  ASSERT(close_cb_count == 1);

  r = uv_fs_rename(loop, &rename_req, "test_file", "test_file2", rename_cb);
  ASSERT(r == 0);

  uv_run(loop);
  ASSERT(create_cb_count == 1);
  ASSERT(write_cb_count == 1);
  ASSERT(close_cb_count == 1);
  ASSERT(rename_cb_count == 1);

  r = uv_fs_open(loop, &open_req1, "test_file2", O_RDWR, 0, open_cb);
  ASSERT(r == 0);

  uv_run(loop);
  ASSERT(open_cb_count == 1);
  ASSERT(read_cb_count == 1);
  ASSERT(close_cb_count == 2);
  ASSERT(rename_cb_count == 1);
  ASSERT(create_cb_count == 1);
  ASSERT(write_cb_count == 1);
  ASSERT(ftruncate_cb_count == 1);

  r = uv_fs_open(loop, &open_req1, "test_file2", O_RDONLY, 0, open_cb);
  ASSERT(r == 0);

  uv_run(loop);
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

  return 0;
}


TEST_IMPL(fs_file_sync) {
  int r;

  /* Setup. */
  unlink("test_file");
  unlink("test_file2");

  uv_init();
  loop = uv_default_loop();

  r = uv_fs_open(loop, &open_req1, "test_file", O_WRONLY | O_CREAT,
      S_IWRITE | S_IREAD, NULL);
  ASSERT(r == 0);
  ASSERT(open_req1.result != -1);
  uv_fs_req_cleanup(&open_req1);

  r = uv_fs_write(loop, &write_req, open_req1.result, test_buf,
      sizeof(test_buf), -1, NULL);
  ASSERT(r == 0);
  ASSERT(write_req.result != -1);
  uv_fs_req_cleanup(&write_req);

  r = uv_fs_close(loop, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result != -1);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_open(loop, &open_req1, "test_file", O_RDWR, 0, NULL);
  ASSERT(r == 0);
  ASSERT(open_req1.result != -1);
  uv_fs_req_cleanup(&open_req1);

  r = uv_fs_read(loop, &read_req, open_req1.result, buf, sizeof(buf), -1,
      NULL);
  ASSERT(r == 0);
  ASSERT(read_req.result != -1);
  ASSERT(strcmp(buf, test_buf) == 0);
  uv_fs_req_cleanup(&read_req);

  r = uv_fs_ftruncate(loop, &ftruncate_req, open_req1.result, 7, NULL);
  ASSERT(r == 0);
  ASSERT(ftruncate_req.result != -1);
  uv_fs_req_cleanup(&ftruncate_req);

  r = uv_fs_close(loop, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result != -1);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_rename(loop, &rename_req, "test_file", "test_file2", NULL);
  ASSERT(r == 0);
  ASSERT(rename_req.result != -1);
  uv_fs_req_cleanup(&rename_req);

  r = uv_fs_open(loop, &open_req1, "test_file2", O_RDONLY, 0, NULL);
  ASSERT(r == 0);
  ASSERT(open_req1.result != -1);
  uv_fs_req_cleanup(&open_req1);

  memset(buf, 0, sizeof(buf));
  r = uv_fs_read(loop, &read_req, open_req1.result, buf, sizeof(buf), -1,
      NULL);
  ASSERT(r == 0);
  ASSERT(read_req.result != -1);
  ASSERT(strcmp(buf, "test-bu") == 0);
  uv_fs_req_cleanup(&read_req);

  r = uv_fs_close(loop, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  ASSERT(close_req.result != -1);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_unlink(loop, &unlink_req, "test_file2", NULL);
  ASSERT(r == 0);
  ASSERT(unlink_req.result != -1);
  uv_fs_req_cleanup(&unlink_req);

  /* Cleanup */
  unlink("test_file");
  unlink("test_file2");

  return 0;
}


TEST_IMPL(fs_async_dir) {
  int r;

  /* Setup */
  unlink("test_dir/file1");
  unlink("test_dir/file2");
  rmdir("test_dir");

  uv_init();
  loop = uv_default_loop();

  r = uv_fs_mkdir(loop, &mkdir_req, "test_dir", 0755, mkdir_cb);
  ASSERT(r == 0);

  uv_run(loop);
  ASSERT(mkdir_cb_count == 1);

  /* Create 2 files synchronously. */
  r = uv_fs_open(loop, &open_req1, "test_dir/file1", O_WRONLY | O_CREAT,
      S_IWRITE | S_IREAD, NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&open_req1);
  r = uv_fs_close(loop, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_open(loop, &open_req1, "test_dir/file2", O_WRONLY | O_CREAT,
      S_IWRITE | S_IREAD, NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&open_req1);
  r = uv_fs_close(loop, &close_req, open_req1.result, NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_readdir(loop, &readdir_req, "test_dir", 0, readdir_cb);
  ASSERT(r == 0);

  uv_run(loop);
  ASSERT(readdir_cb_count == 1);

  r = uv_fs_stat(loop, &stat_req, "test_dir", stat_cb);
  ASSERT(r == 0);
  uv_run(loop);

  r = uv_fs_stat(loop, &stat_req, "test_dir\\", stat_cb);
  ASSERT(r == 0);
  uv_run(loop);

  r = uv_fs_lstat(loop, &stat_req, "test_dir", stat_cb);
  ASSERT(r == 0);
  uv_run(loop);

  r = uv_fs_lstat(loop, &stat_req, "test_dir\\", stat_cb);
  ASSERT(r == 0);
  uv_run(loop);

  ASSERT(stat_cb_count == 4);

  r = uv_fs_unlink(loop, &unlink_req, "test_dir/file1", unlink_cb);
  ASSERT(r == 0);
  uv_run(loop);
  ASSERT(unlink_cb_count == 1);

  r = uv_fs_unlink(loop, &unlink_req, "test_dir/file2", unlink_cb);
  ASSERT(r == 0);
  uv_run(loop);
  ASSERT(unlink_cb_count == 2);

  r = uv_fs_rmdir(loop, &rmdir_req, "test_dir", rmdir_cb);
  ASSERT(r == 0);
  uv_run(loop);
  ASSERT(rmdir_cb_count == 1);

  /* Cleanup */
  unlink("test_dir/file1");
  unlink("test_dir/file2");
  rmdir("test_dir");

  return 0;
}


TEST_IMPL(fs_async_sendfile) {
  int f, r;
  struct stat s1, s2;

  /* Setup. */
  uv_init();
  unlink("test_file");
  unlink("test_file2");

  f = open("test_file", O_WRONLY | O_CREAT, S_IWRITE | S_IREAD);
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
  loop = uv_default_loop();

  r = uv_fs_open(loop, &open_req1, "test_file", O_RDWR, 0, NULL);
  ASSERT(r == 0);
  ASSERT(open_req1.result != -1);
  uv_fs_req_cleanup(&open_req1);

  r = uv_fs_open(loop, &open_req2, "test_file2", O_WRONLY | O_CREAT,
      S_IWRITE | S_IREAD, NULL);
  ASSERT(r == 0);
  ASSERT(open_req2.result != -1);
  uv_fs_req_cleanup(&open_req2);

  r = uv_fs_sendfile(loop, &sendfile_req, open_req2.result, open_req1.result,
      0, 131072, sendfile_cb);
  ASSERT(r == 0);
  uv_run(loop);

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

  return 0;
}


TEST_IMPL(fs_fstat) {
  int r;
  uv_fs_t req;
  uv_file file;
  struct stat* s;

  /* Setup. */
  unlink("test_file");

  uv_init();

  loop = uv_default_loop();

  r = uv_fs_open(loop, &req, "test_file", O_RDWR | O_CREAT,
      S_IWRITE | S_IREAD, NULL);
  ASSERT(r == 0);
  ASSERT(req.result != -1);
  file = req.result;
  uv_fs_req_cleanup(&req);

  r = uv_fs_write(loop, &req, file, test_buf, sizeof(test_buf), -1, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == sizeof(test_buf));
  uv_fs_req_cleanup(&req);

  r = uv_fs_fstat(loop, &req, file, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  s = req.ptr;
  ASSERT(s->st_size == sizeof(test_buf));
  uv_fs_req_cleanup(&req);

  /* Now do the uv_fs_fstat call asynchronously */
  r = uv_fs_fstat(loop, &req, file, fstat_cb);
  ASSERT(r == 0);
  uv_run(loop);
  ASSERT(fstat_cb_count == 1);


  r = uv_fs_close(loop, &req, file, NULL);
  ASSERT(r == 0);
  ASSERT(req.result == 0);
  uv_fs_req_cleanup(&req);

  /*
   * Run the loop just to check we don't have make any extraneous uv_ref()
   * calls. This should drop out immediately.
   */
  uv_run(loop);

  /* Cleanup. */
  unlink("test_file");

  return 0;
}


TEST_IMPL(fs_chmod) {
  int r;
  uv_fs_t req;
  uv_file file;

  /* Setup. */
  unlink("test_file");

  uv_init();

  loop = uv_default_loop();

  r = uv_fs_open(loop, &req, "test_file", O_RDWR | O_CREAT,
      S_IWRITE | S_IREAD, NULL);
  ASSERT(r == 0);
  ASSERT(req.result != -1);
  file = req.result;
  uv_fs_req_cleanup(&req);

  r = uv_fs_write(loop, &req, file, test_buf, sizeof(test_buf), -1, NULL);
  ASSERT(r == 0);
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
  req.data = (void*)0200;
  r = uv_fs_chmod(loop, &req, "test_file", 0200, chmod_cb);
  ASSERT(r == 0);
  uv_run(loop);
  ASSERT(chmod_cb_count == 1);
#endif

  /* async chmod */
  req.data = (void*)0400;
  r = uv_fs_chmod(loop, &req, "test_file", 0400, chmod_cb);
  ASSERT(r == 0);
  uv_run(loop);
  ASSERT(chmod_cb_count == 1);

  /* async fchmod */
  req.data = (void*)0600;
  r = uv_fs_fchmod(loop, &req, file, 0600, fchmod_cb);
  ASSERT(r == 0);
  uv_run(loop);
  ASSERT(fchmod_cb_count == 1);

  close(file);

  /*
   * Run the loop just to check we don't have make any extraneous uv_ref()
   * calls. This should drop out immediately.
   */
  uv_run(loop);

  /* Cleanup. */
  unlink("test_file");

  return 0;
}


TEST_IMPL(fs_chown) {
  int r;
  uv_fs_t req;
  uv_file file;

  /* Setup. */
  unlink("test_file");

  uv_init();

  loop = uv_default_loop();

  r = uv_fs_open(loop, &req, "test_file", O_RDWR | O_CREAT,
      S_IWRITE | S_IREAD, NULL);
  ASSERT(r == 0);
  ASSERT(req.result != -1);
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
  uv_run(loop);
  ASSERT(chown_cb_count == 1);

  /* async fchown */
  r = uv_fs_fchown(loop, &req, file, -1, -1, fchown_cb);
  ASSERT(r == 0);
  uv_run(loop);
  ASSERT(fchown_cb_count == 1);

  close(file);

  /*
   * Run the loop just to check we don't have make any extraneous uv_ref()
   * calls. This should drop out immediately.
   */
  uv_run(loop);

  /* Cleanup. */
  unlink("test_file");

  return 0;
}