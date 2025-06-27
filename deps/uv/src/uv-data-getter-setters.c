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

const char* uv_handle_type_name(uv_handle_type type) {
  switch (type) {
#define XX(uc,lc) case UV_##uc: return #lc;
  UV_HANDLE_TYPE_MAP(XX)
#undef XX
  case UV_FILE: return "file";
  case UV_HANDLE_TYPE_MAX:
  case UV_UNKNOWN_HANDLE: return NULL;
  }
  return NULL;
}

uv_handle_type uv_handle_get_type(const uv_handle_t* handle) {
  return handle->type;
}

void* uv_handle_get_data(const uv_handle_t* handle) {
  return handle->data;
}

uv_loop_t* uv_handle_get_loop(const uv_handle_t* handle) {
  return handle->loop;
}

void uv_handle_set_data(uv_handle_t* handle, void* data) {
  handle->data = data;
}

const char* uv_req_type_name(uv_req_type type) {
  switch (type) {
#define XX(uc,lc) case UV_##uc: return #lc;
  UV_REQ_TYPE_MAP(XX)
#undef XX
  case UV_REQ_TYPE_MAX:
  case UV_UNKNOWN_REQ:
  default: /* UV_REQ_TYPE_PRIVATE */
    break;
  }
  return NULL;
}

uv_req_type uv_req_get_type(const uv_req_t* req) {
  return req->type;
}

void* uv_req_get_data(const uv_req_t* req) {
  return req->data;
}

void uv_req_set_data(uv_req_t* req, void* data) {
  req->data = data;
}

size_t uv_stream_get_write_queue_size(const uv_stream_t* stream) {
  return stream->write_queue_size;
}

size_t uv_udp_get_send_queue_size(const uv_udp_t* handle) {
  return handle->send_queue_size;
}

size_t uv_udp_get_send_queue_count(const uv_udp_t* handle) {
  return handle->send_queue_count;
}

uv_pid_t uv_process_get_pid(const uv_process_t* proc) {
  return proc->pid;
}

uv_fs_type uv_fs_get_type(const uv_fs_t* req) {
  return req->fs_type;
}

ssize_t uv_fs_get_result(const uv_fs_t* req) {
  return req->result;
}

void* uv_fs_get_ptr(const uv_fs_t* req) {
  return req->ptr;
}

const char* uv_fs_get_path(const uv_fs_t* req) {
  return req->path;
}

uv_stat_t* uv_fs_get_statbuf(uv_fs_t* req) {
  return &req->statbuf;
}

void* uv_loop_get_data(const uv_loop_t* loop) {
  return loop->data;
}

void uv_loop_set_data(uv_loop_t* loop, void* data) {
  loop->data = data;
}
