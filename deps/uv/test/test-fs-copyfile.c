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

#if defined(__unix__) || defined(__POSIX__) || \
    defined(__APPLE__) || defined(_AIX) || defined(__MVS__)
#include <unistd.h> /* unlink, etc. */
#else
# include <direct.h>
# include <io.h>
# define unlink _unlink
#endif

static const char fixture[] = "test/fixtures/load_error.node";
static const char dst[] = "test_file_dst";
static int result_check_count;


static void handle_result(uv_fs_t* req) {
  uv_fs_t stat_req;
  uint64_t size;
  uint64_t mode;
  int r;

  ASSERT(req->fs_type == UV_FS_COPYFILE);
  ASSERT(req->result == 0);

  /* Verify that the file size and mode are the same. */
  r = uv_fs_stat(NULL, &stat_req, req->path, NULL);
  ASSERT(r == 0);
  size = stat_req.statbuf.st_size;
  mode = stat_req.statbuf.st_mode;
  uv_fs_req_cleanup(&stat_req);
  r = uv_fs_stat(NULL, &stat_req, dst, NULL);
  ASSERT(r == 0);
  ASSERT(stat_req.statbuf.st_size == size);
  ASSERT(stat_req.statbuf.st_mode == mode);
  uv_fs_req_cleanup(&stat_req);
  uv_fs_req_cleanup(req);
  result_check_count++;
}


static void touch_file(const char* name, unsigned int size) {
  uv_file file;
  uv_fs_t req;
  uv_buf_t buf;
  int r;
  unsigned int i;

  r = uv_fs_open(NULL, &req, name, O_WRONLY | O_CREAT | O_TRUNC,
                 S_IWUSR | S_IRUSR, NULL);
  uv_fs_req_cleanup(&req);
  ASSERT(r >= 0);
  file = r;

  buf = uv_buf_init("a", 1);

  /* Inefficient but simple. */
  for (i = 0; i < size; i++) {
    r = uv_fs_write(NULL, &req, file, &buf, 1, i, NULL);
    uv_fs_req_cleanup(&req);
    ASSERT(r >= 0);
  }

  r = uv_fs_close(NULL, &req, file, NULL);
  uv_fs_req_cleanup(&req);
  ASSERT(r == 0);
}


TEST_IMPL(fs_copyfile) {
  const char src[] = "test_file_src";
  uv_loop_t* loop;
  uv_fs_t req;
  int r;

  loop = uv_default_loop();

  /* Fails with EINVAL if bad flags are passed. */
  r = uv_fs_copyfile(NULL, &req, src, dst, -1, NULL);
  ASSERT(r == UV_EINVAL);
  uv_fs_req_cleanup(&req);

  /* Fails with ENOENT if source does not exist. */
  unlink(src);
  unlink(dst);
  r = uv_fs_copyfile(NULL, &req, src, dst, 0, NULL);
  ASSERT(req.result == UV_ENOENT);
  ASSERT(r == UV_ENOENT);
  uv_fs_req_cleanup(&req);
  /* The destination should not exist. */
  r = uv_fs_stat(NULL, &req, dst, NULL);
  ASSERT(r != 0);
  uv_fs_req_cleanup(&req);

  /* Copies file synchronously. Creates new file. */
  unlink(dst);
  r = uv_fs_copyfile(NULL, &req, fixture, dst, 0, NULL);
  ASSERT(r == 0);
  handle_result(&req);

  /* Copies a file of size zero. */
  unlink(dst);
  touch_file(src, 0);
  r = uv_fs_copyfile(NULL, &req, src, dst, 0, NULL);
  ASSERT(r == 0);
  handle_result(&req);

  /* Copies file synchronously. Overwrites existing file. */
  r = uv_fs_copyfile(NULL, &req, fixture, dst, 0, NULL);
  ASSERT(r == 0);
  handle_result(&req);

  /* Fails to overwrites existing file. */
  r = uv_fs_copyfile(NULL, &req, fixture, dst, UV_FS_COPYFILE_EXCL, NULL);
  ASSERT(r == UV_EEXIST);
  uv_fs_req_cleanup(&req);

  /* Truncates when an existing destination is larger than the source file. */
  touch_file(src, 1);
  r = uv_fs_copyfile(NULL, &req, src, dst, 0, NULL);
  ASSERT(r == 0);
  handle_result(&req);

  /* Copies a larger file. */
  unlink(dst);
  touch_file(src, 4096 * 2);
  r = uv_fs_copyfile(NULL, &req, src, dst, 0, NULL);
  ASSERT(r == 0);
  handle_result(&req);
  unlink(src);

  /* Copies file asynchronously */
  unlink(dst);
  r = uv_fs_copyfile(loop, &req, fixture, dst, 0, handle_result);
  ASSERT(r == 0);
  ASSERT(result_check_count == 5);
  uv_run(loop, UV_RUN_DEFAULT);
  ASSERT(result_check_count == 6);
  unlink(dst); /* Cleanup */

  return 0;
}
