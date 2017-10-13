#include "node.h"
#include "node_io.h"
#include "util.h"

namespace node {

namespace io {

inline void io_bind_set_user_data(io_bind_t* handle, void* user_data) {
  CHECK_NE(handle, nullptr);
  handle->user_data = user_data;
}

inline void io_pull_set_user_data(io_pull_t* handle, void* user_data) {
  CHECK_NE(handle, nullptr);
  handle->user_data = user_data;
}

inline void io_push_set_user_data(io_push_t* handle, void* user_data) {
  CHECK_NE(handle, nullptr);
  handle->user_data = user_data;
}

inline void* io_bind_get_user_data(io_bind_t* handle) {
  if (handle == nullptr)
    return nullptr;
  return handle->user_data;
}

inline void* io_pull_get_user_data(io_pull_t* handle) {
  if (handle == nullptr)
    return nullptr;
  return handle->user_data;
}

inline void* io_push_get_user_data(io_push_t* handle) {
  if (handle == nullptr)
    return nullptr;
  return handle->user_data;
}

inline void io_bind_set_alloc_cb(io_bind_t* handle, alloc_cb cb) {
  CHECK_NE(handle, nullptr);
  handle->alloc = cb;
}

inline void io_bind_set_free_cb(io_bind_t* handle, free_cb cb) {
  CHECK_NE(handle, nullptr);
  handle->free = cb;
}

inline void io_bind_set_bind_cb(io_bind_t* handle, bind_cb cb) {
  CHECK_NE(handle, nullptr);
  handle->bind = cb;
}

inline void io_bind_set_unbind_cb(io_bind_t* handle, unbind_cb cb) {
  CHECK_NE(handle, nullptr);
  handle->unbind = cb;
}

inline void io_pull_set_pull_cb(io_pull_t* handle, pull_cb cb) {
  CHECK_NE(handle, nullptr);
  handle->pull = cb;
}

inline void io_push_set_push_cb(io_push_t* handle, push_cb cb) {
  CHECK_NE(handle, nullptr);
  handle->push = cb;
}

inline alloc_cb io_bind_get_alloc_cb(io_bind_t* handle) {
  if (handle == nullptr)
    return nullptr;
  return handle->alloc;
}

inline free_cb io_bind_get_free_cb(io_bind_t* handle) {
  if (handle == nullptr)
    return nullptr;
  return handle->free;
}

inline bind_cb io_bind_get_bind_cb(io_bind_t* handle) {
  if (handle == nullptr)
    return nullptr;
  return handle->bind;
}

inline unbind_cb io_bind_get_unbind_cb(io_bind_t* handle) {
  if (handle == nullptr)
    return nullptr;
  return handle->unbind;
}

inline pull_cb io_pull_get_pull_cb(io_pull_t* handle) {
  if (handle == nullptr)
    return nullptr;
  return handle->pull;
}

inline push_cb io_push_get_push_cb(io_push_t* handle) {
  if (handle == nullptr)
    return nullptr;
  return handle->push;
}

inline int io_get_error(int status) {
  if (!(status & IO_PUSH_STATUS_ERROR))
    return 0;
  return status & ~0xFF;
}

}  // namespace io
}  // namespace node
