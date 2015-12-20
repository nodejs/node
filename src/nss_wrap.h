#ifndef SRC_NSS_WRAP_H_
#define SRC_NSS_WRAP_H_

#include "env.h"
#include "env-inl.h"
#include "node.h"
#include "req-wrap.h"
#include "req-wrap-inl.h"
#include "async-wrap.h"
#include "nss_module.h"  // NOLINT(build/include_order)

#if defined(__ANDROID__) || \
    defined(__MINGW32__) || \
    defined(__OpenBSD__) || \
    defined(_MSC_VER)
# include <nameser.h>
#else
# include <arpa/nameser.h>
#endif

namespace node {

// Forward declaration
namespace nss_module { class NSSModule; }

namespace nss_wrap {

#define NSS_REQ_ERR_SUCCESS 0
#define NSS_REQ_ERR_NOTFOUND 1
#define NSS_REQ_ERR_UNAVAIL 2
#define NSS_REQ_ERR_TRYAGAIN 3
#define NSS_REQ_ERR_INTERNAL 4
#define NSS_REQ_ERR_MALLOC 5

class NSSReqWrap : public ReqWrap<uv_work_t> {
 private:
  unsigned int refs_;

 public:
  char* host;
  int8_t host_type;

  nss_module::NSSModule* module;
  uint8_t family;

  int nresults;
  char* results;
  char* families;
  int error;

  NSSReqWrap(Environment* env,
             v8::Local<v8::Object> req_wrap_obj,
             char* host_val,
             int8_t host_type_val);

  ~NSSReqWrap();

  void Ref();

  void Unref();

  size_t self_size() const override;

  static void NameWork(uv_work_t* req);

  static void NameAfter(uv_work_t* req, int status);

  static void AddrWork(uv_work_t* req);

  static void AddrAfter(uv_work_t* req, int status);

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);

  static void Initialize(v8::Handle<v8::Object> target,
                         v8::Handle<v8::Value> unused,
                         v8::Handle<v8::Context> context);
};
}
}

#endif  // SRC_NSS_WRAP_H_
