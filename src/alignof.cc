
#include "env.h"
#include "env-inl.h"
#include "node.h"
#include "node_internals.h"
#include "v8-profiler.h"
#include "v8.h"

namespace node {
namespace _alignof {

using v8::Context;
using v8::Handle;
using v8::Object;
using v8::Persistent;
using v8::Uint32;
using v8::Value;


void Initialize(Handle<Object> exports,
                Handle<Value> unused,
                Handle<Context> context) {
  Environment* env = Environment::GetCurrent(context);

#define SET_ALIGNOF(name, type) \
  struct s_##name { type a; }; \
  exports->Set(FIXED_ONE_BYTE_STRING(env->isolate(), #name ), \
               Uint32::NewFromUnsigned(env->isolate(), static_cast<uint32_t>(__alignof__(struct s_##name))));

  // fixed alignments
  SET_ALIGNOF(int8, int8_t);
  SET_ALIGNOF(uint8, uint8_t);
  SET_ALIGNOF(int16, int16_t);
  SET_ALIGNOF(uint16, uint16_t);
  SET_ALIGNOF(int32, int32_t);
  SET_ALIGNOF(uint32, uint32_t);
  SET_ALIGNOF(int64, int64_t);
  SET_ALIGNOF(uint64, uint64_t);
  SET_ALIGNOF(float, float);
  SET_ALIGNOF(double, double);
  SET_ALIGNOF(longdouble, long double);

  // (potentially) variable alignments
  SET_ALIGNOF(bool, bool);
  SET_ALIGNOF(char, char);
  SET_ALIGNOF(uchar, unsigned char);
  SET_ALIGNOF(short, short);
  SET_ALIGNOF(ushort, unsigned short);
  SET_ALIGNOF(int, int);
  SET_ALIGNOF(uint, unsigned int);
  SET_ALIGNOF(long, long);
  SET_ALIGNOF(ulong, unsigned long);
  SET_ALIGNOF(longlong, long long);
  SET_ALIGNOF(ulonglong, unsigned long long);
  SET_ALIGNOF(pointer, char *);
  SET_ALIGNOF(size_t, size_t);

  // alignment of a Persistent handle to a JS object
  SET_ALIGNOF(Object, Persistent<Object>);

#undef SET_ALIGNOF
}


}  // namespace _alignof
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(alignof, node::_alignof::Initialize)
