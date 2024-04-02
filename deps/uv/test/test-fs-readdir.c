/* Copyright libuv project contributors. All rights reserved.
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
#include <fcntl.h>
#include <string.h>

static uv_fs_t opendir_req;
static uv_fs_t readdir_req;
static uv_fs_t closedir_req;

static uv_dirent_t dirents[1];

static int empty_opendir_cb_count;
static int empty_closedir_cb_count;

static void cleanup_test_files(void) {
  uv_fs_t req;

  uv_fs_unlink(NULL, &req, "test_dir/file1", NULL);
  uv_fs_req_cleanup(&req);
  uv_fs_unlink(NULL, &req, "test_dir/file2", NULL);
  uv_fs_req_cleanup(&req);
  uv_fs_rmdir(NULL, &req, "test_dir/test_subdir", NULL);
  uv_fs_req_cleanup(&req);
  uv_fs_rmdir(NULL, &req, "test_dir", NULL);
  uv_fs_req_cleanup(&req);
}

static void empty_closedir_cb(uv_fs_t* req) {
  ASSERT(req == &closedir_req);
  ASSERT(req->fs_type == UV_FS_CLOSEDIR);
  ASSERT(req->result == 0);
  ++empty_closedir_cb_count;
  uv_fs_req_cleanup(req);
}

static void empty_readdir_cb(uv_fs_t* req) {
  uv_dir_t* dir;
  int r;

  ASSERT(req == &readdir_req);
  ASSERT(req->fs_type == UV_FS_READDIR);
  ASSERT(req->result == 0);
  dir = req->ptr;
  uv_fs_req_cleanup(req);
  r = uv_fs_closedir(uv_default_loop(),
                     &closedir_req,
                     dir,
                     empty_closedir_cb);
  ASSERT(r == 0);
}

static void empty_opendir_cb(uv_fs_t* req) {
  uv_dir_t* dir;
  int r;

  ASSERT(req == &opendir_req);
  ASSERT(req->fs_type == UV_FS_OPENDIR);
  ASSERT(req->result == 0);
  ASSERT_NOT_NULL(req->ptr);
  dir = req->ptr;
  dir->dirents = dirents;
  dir->nentries = ARRAY_SIZE(dirents);
  r = uv_fs_readdir(uv_default_loop(),
                    &readdir_req,
                    dir,
                    empty_readdir_cb);
  ASSERT(r == 0);
  uv_fs_req_cleanup(req);
  ++empty_opendir_cb_count;
}

/*
 * This test makes sure that both synchronous and asynchronous flavors
 * of the uv_fs_opendir() -> uv_fs_readdir() -> uv_fs_closedir() sequence work
 * as expected when processing an empty directory.
 */
TEST_IMPL(fs_readdir_empty_dir) {
  const char* path;
  uv_fs_t mkdir_req;
  uv_fs_t rmdir_req;
  int r;
  int nb_entries_read;
  uv_dir_t* dir;

  path = "./empty_dir/";
  uv_fs_mkdir(uv_default_loop(), &mkdir_req, path, 0777, NULL);
  uv_fs_req_cleanup(&mkdir_req);

  /* Fill the req to ensure that required fields are cleaned up. */
  memset(&opendir_req, 0xdb, sizeof(opendir_req));

  /* Testing the synchronous flavor. */
  r = uv_fs_opendir(uv_default_loop(),
                    &opendir_req,
                    path,
                    NULL);
  ASSERT(r == 0);
  ASSERT(opendir_req.fs_type == UV_FS_OPENDIR);
  ASSERT(opendir_req.result == 0);
  ASSERT_NOT_NULL(opendir_req.ptr);
  dir = opendir_req.ptr;
  uv_fs_req_cleanup(&opendir_req);

  /* Fill the req to ensure that required fields are cleaned up. */
  memset(&readdir_req, 0xdb, sizeof(readdir_req));
  dir->dirents = dirents;
  dir->nentries = ARRAY_SIZE(dirents);
  nb_entries_read = uv_fs_readdir(uv_default_loop(),
                                  &readdir_req,
                                  dir,
                                  NULL);
  ASSERT(nb_entries_read == 0);
  uv_fs_req_cleanup(&readdir_req);

  /* Fill the req to ensure that required fields are cleaned up. */
  memset(&closedir_req, 0xdb, sizeof(closedir_req));
  uv_fs_closedir(uv_default_loop(), &closedir_req, dir, NULL);
  ASSERT(closedir_req.result == 0);
  uv_fs_req_cleanup(&closedir_req);

  /* Testing the asynchronous flavor. */

  /* Fill the req to ensure that required fields are cleaned up. */
  memset(&opendir_req, 0xdb, sizeof(opendir_req));
  memset(&readdir_req, 0xdb, sizeof(readdir_req));
  memset(&closedir_req, 0xdb, sizeof(closedir_req));

  r = uv_fs_opendir(uv_default_loop(), &opendir_req, path, empty_opendir_cb);
  ASSERT(r == 0);
  ASSERT(empty_opendir_cb_count == 0);
  ASSERT(empty_closedir_cb_count == 0);
  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  ASSERT(r == 0);
  ASSERT(empty_opendir_cb_count == 1);
  ASSERT(empty_closedir_cb_count == 1);
  uv_fs_rmdir(uv_default_loop(), &rmdir_req, path, NULL);
  uv_fs_req_cleanup(&rmdir_req);
  MAKE_VALGRIND_HAPPY();
  return 0;
}

/*
 * This test makes sure that reading a non-existing directory with
 * uv_fs_{open,read}_dir() returns proper error codes.
 */

static int non_existing_opendir_cb_count;

static void non_existing_opendir_cb(uv_fs_t* req) {
  ASSERT(req == &opendir_req);
  ASSERT(req->fs_type == UV_FS_OPENDIR);
  ASSERT(req->result == UV_ENOENT);
  ASSERT_NULL(req->ptr);

  uv_fs_req_cleanup(req);
  ++non_existing_opendir_cb_count;
}

TEST_IMPL(fs_readdir_non_existing_dir) {
  const char* path;
  int r;

  path = "./non-existing-dir/";

  /* Fill the req to ensure that required fields are cleaned up. */
  memset(&opendir_req, 0xdb, sizeof(opendir_req));

  /* Testing the synchronous flavor. */
  r = uv_fs_opendir(uv_default_loop(), &opendir_req, path, NULL);
  ASSERT(r == UV_ENOENT);
  ASSERT(opendir_req.fs_type == UV_FS_OPENDIR);
  ASSERT(opendir_req.result == UV_ENOENT);
  ASSERT_NULL(opendir_req.ptr);
  uv_fs_req_cleanup(&opendir_req);

  /* Fill the req to ensure that required fields are cleaned up. */
  memset(&opendir_req, 0xdb, sizeof(opendir_req));

  /* Testing the async flavor. */
  r = uv_fs_opendir(uv_default_loop(),
                    &opendir_req,
                    path,
                    non_existing_opendir_cb);
  ASSERT(r == 0);
  ASSERT(non_existing_opendir_cb_count == 0);
  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  ASSERT(r == 0);
  ASSERT(non_existing_opendir_cb_count == 1);

  MAKE_VALGRIND_HAPPY();
  return 0;
}

/*
 * This test makes sure that reading a file as a directory reports correct
 * error codes.
 */

static int file_opendir_cb_count;

static void file_opendir_cb(uv_fs_t* req) {
  ASSERT(req == &opendir_req);
  ASSERT(req->fs_type == UV_FS_OPENDIR);
  ASSERT(req->result == UV_ENOTDIR);
  ASSERT_NULL(req->ptr);

  uv_fs_req_cleanup(req);
  ++file_opendir_cb_count;
}

TEST_IMPL(fs_readdir_file) {
  const char* path;
  int r;

  path = "test/fixtures/empty_file";

  /* Fill the req to ensure that required fields are cleaned up. */
  memset(&opendir_req, 0xdb, sizeof(opendir_req));

  /* Testing the synchronous flavor. */
  r = uv_fs_opendir(uv_default_loop(), &opendir_req, path, NULL);

  ASSERT(r == UV_ENOTDIR);
  ASSERT(opendir_req.fs_type == UV_FS_OPENDIR);
  ASSERT(opendir_req.result == UV_ENOTDIR);
  ASSERT_NULL(opendir_req.ptr);

  uv_fs_req_cleanup(&opendir_req);

  /* Fill the req to ensure that required fields are cleaned up. */
  memset(&opendir_req, 0xdb, sizeof(opendir_req));

  /* Testing the async flavor. */
  r = uv_fs_opendir(uv_default_loop(), &opendir_req, path, file_opendir_cb);
  ASSERT(r == 0);
  ASSERT(file_opendir_cb_count == 0);
  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  ASSERT(r == 0);
  ASSERT(file_opendir_cb_count == 1);
  MAKE_VALGRIND_HAPPY();
  return 0;
}

/*
 * This test makes sure that reading a non-empty directory with
 * uv_fs_{open,read}_dir() returns proper directory entries, including the
 * correct entry types.
 */

static int non_empty_opendir_cb_count;
static int non_empty_readdir_cb_count;
static int non_empty_closedir_cb_count;

static void non_empty_closedir_cb(uv_fs_t* req) {
  ASSERT(req == &closedir_req);
  ASSERT(req->result == 0);
  uv_fs_req_cleanup(req);
  ++non_empty_closedir_cb_count;
}

static void non_empty_readdir_cb(uv_fs_t* req) {
  uv_dir_t* dir;

  ASSERT(req == &readdir_req);
  ASSERT(req->fs_type == UV_FS_READDIR);
  dir = req->ptr;

  if (req->result == 0) {
    uv_fs_req_cleanup(req);
    ASSERT(non_empty_readdir_cb_count == 3);
    uv_fs_closedir(uv_default_loop(),
                   &closedir_req,
                   dir,
                   non_empty_closedir_cb);
  } else {
    ASSERT(req->result == 1);
    ASSERT(dir->dirents == dirents);
    ASSERT(strcmp(dirents[0].name, "file1") == 0 ||
           strcmp(dirents[0].name, "file2") == 0 ||
           strcmp(dirents[0].name, "test_subdir") == 0);
#ifdef HAVE_DIRENT_TYPES
    if (!strcmp(dirents[0].name, "test_subdir"))
      ASSERT(dirents[0].type == UV_DIRENT_DIR);
    else
      ASSERT(dirents[0].type == UV_DIRENT_FILE);
#else
    ASSERT(dirents[0].type == UV_DIRENT_UNKNOWN);
#endif /* HAVE_DIRENT_TYPES */

    ++non_empty_readdir_cb_count;
    uv_fs_req_cleanup(req);
    dir->dirents = dirents;
    dir->nentries = ARRAY_SIZE(dirents);
    uv_fs_readdir(uv_default_loop(),
                  &readdir_req,
                  dir,
                  non_empty_readdir_cb);
  }
}

static void non_empty_opendir_cb(uv_fs_t* req) {
  uv_dir_t* dir;
  int r;

  ASSERT(req == &opendir_req);
  ASSERT(req->fs_type == UV_FS_OPENDIR);
  ASSERT(req->result == 0);
  ASSERT_NOT_NULL(req->ptr);

  dir = req->ptr;
  dir->dirents = dirents;
  dir->nentries = ARRAY_SIZE(dirents);

  r = uv_fs_readdir(uv_default_loop(),
                    &readdir_req,
                    dir,
                    non_empty_readdir_cb);
  ASSERT(r == 0);
  uv_fs_req_cleanup(req);
  ++non_empty_opendir_cb_count;
}

TEST_IMPL(fs_readdir_non_empty_dir) {
  size_t entries_count;
  uv_fs_t mkdir_req;
  uv_fs_t rmdir_req;
  uv_fs_t create_req;
  uv_fs_t close_req;
  uv_dir_t* dir;
  int r;

  cleanup_test_files();

  r = uv_fs_mkdir(uv_default_loop(), &mkdir_req, "test_dir", 0755, NULL);
  ASSERT(r == 0);

  /* Create two files synchronously. */
  r = uv_fs_open(uv_default_loop(),
                 &create_req,
                 "test_dir/file1",
                 O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR,
                 NULL);
  ASSERT(r >= 0);
  uv_fs_req_cleanup(&create_req);
  r = uv_fs_close(uv_default_loop(),
                  &close_req,
                  create_req.result,
                  NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_open(uv_default_loop(),
                 &create_req,
                 "test_dir/file2",
                 O_WRONLY | O_CREAT, S_IWUSR | S_IRUSR,
                 NULL);
  ASSERT(r >= 0);
  uv_fs_req_cleanup(&create_req);
  r = uv_fs_close(uv_default_loop(),
                  &close_req,
                  create_req.result,
                  NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&close_req);

  r = uv_fs_mkdir(uv_default_loop(),
                  &mkdir_req,
                  "test_dir/test_subdir",
                  0755,
                  NULL);
  ASSERT(r == 0);
  uv_fs_req_cleanup(&mkdir_req);

  /* Fill the req to ensure that required fields are cleaned up. */
  memset(&opendir_req, 0xdb, sizeof(opendir_req));

  /* Testing the synchronous flavor. */
  r = uv_fs_opendir(uv_default_loop(), &opendir_req, "test_dir", NULL);
  ASSERT(r == 0);
  ASSERT(opendir_req.fs_type == UV_FS_OPENDIR);
  ASSERT(opendir_req.result == 0);
  ASSERT_NOT_NULL(opendir_req.ptr);

  entries_count = 0;
  dir = opendir_req.ptr;
  dir->dirents = dirents;
  dir->nentries = ARRAY_SIZE(dirents);
  uv_fs_req_cleanup(&opendir_req);

  while (uv_fs_readdir(uv_default_loop(),
                       &readdir_req,
                       dir,
                       NULL) != 0) {
  ASSERT(strcmp(dirents[0].name, "file1") == 0 ||
         strcmp(dirents[0].name, "file2") == 0 ||
         strcmp(dirents[0].name, "test_subdir") == 0);
#ifdef HAVE_DIRENT_TYPES
    if (!strcmp(dirents[0].name, "test_subdir"))
      ASSERT(dirents[0].type == UV_DIRENT_DIR);
    else
      ASSERT(dirents[0].type == UV_DIRENT_FILE);
#else
    ASSERT(dirents[0].type == UV_DIRENT_UNKNOWN);
#endif /* HAVE_DIRENT_TYPES */
    uv_fs_req_cleanup(&readdir_req);
    ++entries_count;
  }

  ASSERT(entries_count == 3);
  uv_fs_req_cleanup(&readdir_req);

  /* Fill the req to ensure that required fields are cleaned up. */
  memset(&closedir_req, 0xdb, sizeof(closedir_req));
  uv_fs_closedir(uv_default_loop(), &closedir_req, dir, NULL);
  ASSERT(closedir_req.result == 0);
  uv_fs_req_cleanup(&closedir_req);

  /* Testing the asynchronous flavor. */

  /* Fill the req to ensure that required fields are cleaned up. */
  memset(&opendir_req, 0xdb, sizeof(opendir_req));

  r = uv_fs_opendir(uv_default_loop(),
                    &opendir_req,
                    "test_dir",
                    non_empty_opendir_cb);
  ASSERT(r == 0);
  ASSERT(non_empty_opendir_cb_count == 0);
  ASSERT(non_empty_closedir_cb_count == 0);
  r = uv_run(uv_default_loop(), UV_RUN_DEFAULT);
  ASSERT(r == 0);
  ASSERT(non_empty_opendir_cb_count == 1);
  ASSERT(non_empty_closedir_cb_count == 1);

  uv_fs_rmdir(uv_default_loop(), &rmdir_req, "test_subdir", NULL);
  uv_fs_req_cleanup(&rmdir_req);

  cleanup_test_files();
  MAKE_VALGRIND_HAPPY();
  return 0;
 }
