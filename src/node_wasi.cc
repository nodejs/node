#include <cerrno>
#ifdef _WIN32
#include <io.h>
#include <processthreadsapi.h>
#else
#include <sys/types.h>
#include <unistd.h>
#include <sched.h>
#endif
#include <uv.h>

#include "util-inl.h"
#include "node_internals.h"

#define NAPI_EXPERIMENTAL
#include "node_api.h"

namespace node {
namespace wasi {

napi_value Seek(napi_env env, napi_callback_info info) {
  napi_status status;

  napi_value args[3];
  size_t argc = 3;
  status = napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  if (status != napi_ok) {
    return nullptr;
  }

  uint32_t fd;
  status = napi_get_value_uint32(env, args[0], &fd);
  if (status != napi_ok) {
    return nullptr;
  }

  uint64_t offset;
  bool lossless;
  status = napi_get_value_bigint_uint64(env, args[1], &offset, &lossless);
  if (status != napi_ok) {
    return nullptr;
  }

  int32_t whence;
  status = napi_get_value_int32(env, args[2], &whence);
  if (status != napi_ok) {
    return nullptr;
  }

#ifdef _WIN32
  int64_t new_offset = _lseeki64(fd, offset, whence);
#else
  off_t new_offset = lseek(fd, offset, whence);
#endif  // _WIN32

  napi_value result;
  if (new_offset == -1) {
    status = napi_create_int32(env, errno, &result);
  } else {
    status = napi_create_bigint_uint64(env, new_offset, &result);
  }
  if (status != napi_ok) {
    return nullptr;
  }

  return result;
}

napi_value SchedYield(napi_env, napi_callback_info) {
#ifdef _WIN32
  SwitchToThread();
#else
  sched_yield();
#endif  // WIN32
  return nullptr;
}

napi_value Realtime(napi_env env, napi_callback_info) {
  uv_timeval64_t tv;
  uv_gettimeofday(&tv);

  uint64_t ns = tv.tv_sec * 1e9;
  ns += tv.tv_usec * 1000;

  napi_value result;
  napi_status status = napi_create_bigint_uint64(env, ns, &result);

  if (status != napi_ok) {
    return nullptr;
  }

  return result;
}

napi_value Init(napi_env env, napi_value exports) {
  napi_status status;

#define V(f, name) \
  { \
    napi_value func; \
    status = napi_create_function( \
        env, name, NAPI_AUTO_LENGTH, f, nullptr, &func); \
    if (status != napi_ok) { \
      return nullptr; \
    } \
    status = napi_set_named_property(env, exports, name, func); \
    if (status != napi_ok) { \
      return nullptr; \
    } \
  }
  V(Seek, "seek");
  V(SchedYield, "schedYield");
  V(Realtime, "realtime");
#undef V

#define V(n) \
  { \
    napi_value result; \
    napi_create_int32(env, n, &result); \
    napi_set_named_property(env, exports, #n, result); \
  }
  V(SEEK_SET);
  V(SEEK_CUR);
  V(SEEK_END);


  return exports;
}

}  // namespace wasi
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL_NAPI(wasi, node::wasi::Init)
