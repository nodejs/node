#ifndef SRC_NODE_HTTP_COMMON_INL_H_
#define SRC_NODE_HTTP_COMMON_INL_H_

#include "node_http_common.h"
#include "node.h"
#include "env-inl.h"

namespace node {

using v8::Array;
using v8::Local;
using v8::String;
using v8::Uint32;
using v8::Value;

template <typename T>
NgHeaders<T>::NgHeaders(Environment* env, Local<Array> headers) {
  Local<Value> header_string =
      headers->Get(env->context(), 0).ToLocalChecked();
  Local<Value> header_count =
      headers->Get(env->context(), 1).ToLocalChecked();
  CHECK(header_count->IsUint32());
  CHECK(header_string->IsString());
  count_ = header_count.As<Uint32>()->Value();
  int header_string_len = header_string.As<String>()->Length();

  if (count_ == 0) {
    CHECK_EQ(header_string_len, 0);
    return;
  }

  buf_.AllocateSufficientStorage((alignof(nv_t) - 1) +
                                 count_ * sizeof(nv_t) +
                                 header_string_len);

  char* start = reinterpret_cast<char*>(
      RoundUp(reinterpret_cast<uintptr_t>(*buf_), alignof(nv_t)));
  char* header_contents = start + (count_ * sizeof(nv_t));
  nv_t* const nva = reinterpret_cast<nv_t*>(start);

  CHECK_LE(header_contents + header_string_len, *buf_ + buf_.length());
  CHECK_EQ(header_string.As<String>()->WriteOneByte(
               env->isolate(),
               reinterpret_cast<uint8_t*>(header_contents),
               0,
               header_string_len,
               String::NO_NULL_TERMINATION),
           header_string_len);

  size_t n = 0;
  char* p;
  for (p = header_contents; p < header_contents + header_string_len; n++) {
    if (n >= count_) {
      static uint8_t zero = '\0';
      nva[0].name = nva[0].value = &zero;
      nva[0].namelen = nva[0].valuelen = 1;
      count_ = 1;
      return;
    }

    nva[n].flags = T::kNoneFlag;
    nva[n].name = reinterpret_cast<uint8_t*>(p);
    nva[n].namelen = strlen(p);
    p += nva[n].namelen + 1;
    nva[n].value = reinterpret_cast<uint8_t*>(p);
    nva[n].valuelen = strlen(p);
    p += nva[n].valuelen + 1;
  }
}

}  // namespace node

#endif  // SRC_NODE_HTTP_COMMON_INL_H_
