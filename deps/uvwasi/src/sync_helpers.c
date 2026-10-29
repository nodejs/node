#include "uv.h"
#include "sync_helpers.h"
#include "uv_mapping.h"
#include "uvwasi_alloc.h"

typedef struct free_handle_data_s {
  uvwasi_t* uvwasi;
  int done;
} free_handle_data_t;

static void free_handle_cb(uv_handle_t* handle) {
  free_handle_data_t* free_handle_data = uv_handle_get_data((uv_handle_t*) handle);
  uvwasi__free(free_handle_data->uvwasi, handle);
  free_handle_data->done = 1;
}

int free_handle_sync(struct uvwasi_s* uvwasi, uv_handle_t* handle) {
  free_handle_data_t free_handle_data = { uvwasi, 0 };
  uv_handle_set_data(handle, (void*) &free_handle_data);
  uv_close(handle, free_handle_cb);
  uv_loop_t* handle_loop = uv_handle_get_loop(handle);
  while(!free_handle_data.done) {
    if (uv_run(handle_loop, UV_RUN_ONCE) == 0) {
      break;
    }
  }
  return UVWASI_ESUCCESS;
}

static void do_stream_shutdown(uv_shutdown_t* req, int status) {
  shutdown_data_t* shutdown_data;
  shutdown_data = uv_handle_get_data((uv_handle_t*) req->handle);
  shutdown_data->status = status;
  shutdown_data->done = 1;
 }

int shutdown_stream_sync(struct uvwasi_s* uvwasi,
                          uv_stream_t* stream,
		          shutdown_data_t* shutdown_data) {
  uv_shutdown_t req; 
  uv_loop_t* stream_loop;

  shutdown_data->done = 0;
  shutdown_data->status = 0;
  stream_loop = uv_handle_get_loop((uv_handle_t*) stream);

  uv_handle_set_data((uv_handle_t*) stream, (void*) shutdown_data);
  uv_shutdown(&req, stream, do_stream_shutdown);
  while (!shutdown_data->done) {
    if (uv_run(stream_loop, UV_RUN_ONCE) == 0) {
      return UVWASI_ECANCELED;
    }
  }
  return UVWASI_ESUCCESS;
}

static void recv_alloc_cb(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  recv_data_t* recv_data;
  recv_data = uv_handle_get_data(handle);
  buf->base = recv_data->base;
  buf->len = recv_data->len;
}

void do_stream_recv(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
  recv_data_t* recv_data;
  recv_data = uv_handle_get_data((uv_handle_t*) stream);
  uv_read_stop(stream);
  recv_data->nread = nread;
  recv_data->done = 1;
}

int read_stream_sync(struct uvwasi_s* uvwasi,
                     uv_stream_t* stream,
	             recv_data_t* recv_data) {
  uv_loop_t* recv_loop;
  int r;

  recv_data->nread = 0;
  recv_data->done = 0;
  recv_loop = uv_handle_get_loop((uv_handle_t*) stream);

  uv_handle_set_data((uv_handle_t*) stream, (void*) recv_data);
  r = uv_read_start(stream, recv_alloc_cb, do_stream_recv);
  if (r != 0) {
    return uvwasi__translate_uv_error(r);
  }

  while (!recv_data->done) {
    if (uv_run(recv_loop, UV_RUN_ONCE) == 0) {
      return UVWASI_ECANCELED;
    }
 }

  return UVWASI_ESUCCESS;
}
