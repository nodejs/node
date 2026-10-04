/* Copyright libuv contributors. All rights reserved.
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

/* Verify that passing INT32_MAX as a buffer length is rejected with UV_EINVAL
 * at the various I/O entry points that enforce UV__IO_MAX_BYTES.
 */

#include "uv.h"
#include "task.h"

#include <fcntl.h>
#include <stdint.h>
#include <sys/stat.h>

#define TEST_FILE "tmp_io_64_safe"

static void on_udp_send(uv_udp_send_t* req, int status) {
  /* Should never be called: uv_udp_send must reject synchronously. */
  ASSERT(0 && "on_udp_send callback must not be invoked");
  (void) req;
  (void) status;
}

TEST_IMPL(io_64_safe) {
  uv_loop_t* loop;
  uv_fs_t open_req;
  uv_fs_t fs_req;
  uv_write_t write_req;
  uv_udp_t udp;
  uv_tcp_t tcp;
  uv_udp_send_t send_req;
  struct sockaddr_in addr;
  uv_buf_t* t2_bufs[1];
  unsigned int t2_nbufs[1];
  struct sockaddr* t2_addrs[1];
  uv_buf_t buf;
  uv_buf_t bufs2[2];
  uv_file fd;
  uv_file in_fd;
  uv_file out_fd;

  loop = uv_default_loop();

  /* A buf whose length just exceeds UV__IO_MAX_BYTES (0x7ffff000). */
  buf = uv_buf_init(NULL, INT32_MAX);

  /* Two buffers whose individual sizes are reasonable but whose sum exceeds
   * UV__IO_MAX_BYTES (0x7ffff000).  Each is 1 GiB + 1 byte.
   */
  bufs2[0] = uv_buf_init(NULL, 0x40000001u);
  bufs2[1] = uv_buf_init(NULL, 0x40000001u);

  /* ------------------------------------------------------------------ */
  /* uv_fs_write: reject synchronous filesystem write > UV__IO_MAX_BYTES.   */
  /* ------------------------------------------------------------------ */
  {
    fd = uv_fs_open(NULL, &open_req, TEST_FILE,
                    UV_FS_O_WRONLY | UV_FS_O_CREAT | UV_FS_O_TRUNC, S_IRUSR | S_IWUSR, NULL);
    ASSERT_GE(fd, 0);
    uv_fs_req_cleanup(&open_req);

    ASSERT_EQ(UV_EINVAL, uv_fs_write(NULL, &fs_req, fd, &buf, 1, 0, NULL));
    uv_fs_req_cleanup(&fs_req);

    /* nbufs > 1 where sum > UV__IO_MAX_BYTES */
    ASSERT_EQ(UV_EINVAL, uv_fs_write(NULL, &fs_req, fd, bufs2, 2, 0, NULL));
    uv_fs_req_cleanup(&fs_req);

    uv_fs_close(NULL, &fs_req, fd, NULL);
    uv_fs_req_cleanup(&fs_req);
  }

  /* ------------------------------------------------------------------ */
  /* uv_fs_sendfile: reject len > UV__IO_MAX_BYTES.                         */
  /* ------------------------------------------------------------------ */
  {
    in_fd = uv_fs_open(NULL, &open_req, TEST_FILE,
                       UV_FS_O_RDONLY | UV_FS_O_CREAT, S_IRUSR | S_IWUSR, NULL);
    ASSERT_GE(in_fd, 0);
    uv_fs_req_cleanup(&open_req);

    out_fd = uv_fs_open(NULL, &open_req, TEST_FILE,
                        UV_FS_O_WRONLY | UV_FS_O_CREAT | UV_FS_O_APPEND, S_IRUSR | S_IWUSR,
                        NULL);
    ASSERT_GE(out_fd, 0);
    uv_fs_req_cleanup(&open_req);

    ASSERT_EQ(UV_EINVAL,
              uv_fs_sendfile(NULL, &fs_req, out_fd, in_fd, 0,
                             (size_t) INT32_MAX, NULL));
    uv_fs_req_cleanup(&fs_req);

    uv_fs_close(NULL, &fs_req, in_fd, NULL);
    uv_fs_req_cleanup(&fs_req);
    uv_fs_close(NULL, &fs_req, out_fd, NULL);
    uv_fs_req_cleanup(&fs_req);
  }

  uv_fs_unlink(NULL, &fs_req, TEST_FILE, NULL);
  uv_fs_req_cleanup(&fs_req);

  {
    /* uv_write: reject stream write > UV__IO_MAX_BYTES before queuing. */
    ASSERT_OK(uv_tcp_init(loop, &tcp));
    ASSERT_EQ(UV_EINVAL,
              uv_write(&write_req, (uv_stream_t*) &tcp, &buf, 1, NULL));

    /* nbufs > 1 where sum > UV__IO_MAX_BYTES */
    ASSERT_EQ(UV_EINVAL,
              uv_write(&write_req, (uv_stream_t*) &tcp, bufs2, 2, NULL));

    /* uv_try_write: same check via the synchronous path. */
    ASSERT_EQ(UV_EINVAL, uv_try_write((uv_stream_t*) &tcp, &buf, 1));

    /* nbufs > 1 via try_write */
    ASSERT_EQ(UV_EINVAL, uv_try_write((uv_stream_t*) &tcp, bufs2, 2));

    uv_close((uv_handle_t*) &tcp, NULL);
  }

  {
    ASSERT_OK(uv_udp_init(loop, &udp));
    ASSERT_OK(uv_ip4_addr("127.0.0.1", TEST_PORT, &addr));

    /* uv_udp_try_send: reject UDP send > UV__IO_MAX_BYTES. */
    ASSERT_EQ(UV_EINVAL,
              uv_udp_try_send(&udp, &buf, 1,
                              (const struct sockaddr*) &addr));

    /* nbufs > 1 via try_send */
    ASSERT_EQ(UV_EINVAL,
              uv_udp_try_send(&udp, bufs2, 2,
                              (const struct sockaddr*) &addr));

    /* uv_udp_send (async): reject synchronously before queuing. */
    ASSERT_EQ(UV_EINVAL,
              uv_udp_send(&send_req, &udp, &buf, 1,
                          (const struct sockaddr*) &addr, on_udp_send));

    /* nbufs > 1 via async send */
    ASSERT_EQ(UV_EINVAL,
              uv_udp_send(&send_req, &udp, bufs2, 2,
                          (const struct sockaddr*) &addr, on_udp_send));

    /* uv_udp_try_send2: reject per-batch size > UV__IO_MAX_BYTES. */
    t2_bufs[0]  = &buf;
    t2_nbufs[0] = 1;
    t2_addrs[0] = (struct sockaddr*) &addr;
    ASSERT_EQ(UV_EINVAL,
              uv_udp_try_send2(&udp, 1, t2_bufs, t2_nbufs, t2_addrs, 0));

    /* nbufs > 1 per batch via try_send2 */
    t2_bufs[0]  = bufs2;
    t2_nbufs[0] = 2;
    ASSERT_EQ(UV_EINVAL,
              uv_udp_try_send2(&udp, 1, t2_bufs, t2_nbufs, t2_addrs, 0));

    uv_close((uv_handle_t*) &udp, NULL);
  }

  uv_run(loop, UV_RUN_DEFAULT);
  MAKE_VALGRIND_HAPPY(loop);
  return 0;
}
