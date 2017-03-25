#include "node_api_async.h"
#include "uv.h"

typedef struct napi_work_impl__ {
  uv_work_t* work;
  void* data;
  void (*execute)(void* data);
  void (*complete)(void* data);
  void (*destroy)(void* data);
} napi_work_impl;

napi_work napi_create_async_work() {
  napi_work_impl* worker =
      reinterpret_cast<napi_work_impl*>(malloc(sizeof(napi_work_impl)));
  uv_work_t* req = reinterpret_cast<uv_work_t*>(malloc(sizeof(uv_work_t)));
  req->data = worker;
  worker->work = req;
  return reinterpret_cast<napi_work>(worker);
}

void napi_delete_async_work(napi_work w) {
  napi_work_impl* worker = reinterpret_cast<napi_work_impl*>(w);
  if (worker != NULL) {
    if (worker->work != NULL) {
      delete reinterpret_cast<uv_work_t*>(worker->work);
    }
    delete worker;
    worker = NULL;
  }
}

void napi_async_set_data(napi_work w, void* data) {
  napi_work_impl* worker = reinterpret_cast<napi_work_impl*>(w);
  worker->data = data;
}

void napi_async_set_execute(napi_work w, void (*execute)(void* data)) {
  napi_work_impl* worker = reinterpret_cast<napi_work_impl*>(w);
  worker->execute = execute;
}

void napi_async_set_complete(napi_work w, void (*complete)(void* data)) {
  napi_work_impl* worker = reinterpret_cast<napi_work_impl*>(w);
  worker->complete = complete;
}

void napi_async_set_destroy(napi_work w, void (*destroy)(void* data)) {
  napi_work_impl* worker = reinterpret_cast<napi_work_impl*>(w);
  worker->destroy = destroy;
}

void napi_async_execute(uv_work_t* req) {
  napi_work_impl* worker = static_cast<napi_work_impl*>(req->data);
  worker->execute(worker->data);
}

void napi_async_complete(uv_work_t* req) {
  napi_work_impl* worker = static_cast<napi_work_impl*>(req->data);
  worker->complete(worker->data);
  worker->destroy(worker->data);
}

void napi_async_queue_worker(napi_work w) {
  napi_work_impl* worker = reinterpret_cast<napi_work_impl*>(w);
  uv_queue_work(uv_default_loop(),
                worker->work,
                napi_async_execute,
                reinterpret_cast<uv_after_work_cb>(napi_async_complete));
}
