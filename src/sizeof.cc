
#include "env.h"
#include "env-inl.h"
#include "node.h"
#include "node_internals.h"
#include "v8-profiler.h"
#include "v8.h"

namespace node {
namespace _sizeof {

using v8::Context;
using v8::Handle;
using v8::Object;
using v8::Uint32;
using v8::Value;


void Initialize(Handle<Object> exports,
                Handle<Value> unused,
                Handle<Context> context) {
  Environment* env = Environment::GetCurrent(context);

  Handle<Object> sizeof_obj = Object::New(env->isolate());
  Handle<Object> alignof_obj = Object::New(env->isolate());

#define SET_SIZEOF(name, type) \
  sizeof_obj->Set(FIXED_ONE_BYTE_STRING(env->isolate(), #name), \
               Uint32::NewFromUnsigned(env->isolate(), \
                                       static_cast<uint32_t>(sizeof(type))));

#define SET_ALIGNOF(name, type) \
  alignof_obj->Set(FIXED_ONE_BYTE_STRING(env->isolate(), #name), \
               Uint32::NewFromUnsigned(env->isolate(), \
                                       static_cast<uint32_t>(alignof(type))));

#define SET_TYPE(name, type) \
  SET_SIZEOF(name, type) \
  SET_ALIGNOF(name, type)

  // fixed sizes
  SET_TYPE(int8, int8_t);
  SET_TYPE(uint8, uint8_t);
  SET_TYPE(int16, int16_t);
  SET_TYPE(uint16, uint16_t);
  SET_TYPE(int32, int32_t);
  SET_TYPE(uint32, uint32_t);
  SET_TYPE(int64, int64_t);
  SET_TYPE(uint64, uint64_t);
  SET_TYPE(float, float);
  SET_TYPE(double, double);

  // (potentially) variable sizes
  SET_TYPE(bool, bool);
  SET_TYPE(byte, unsigned char);
  SET_TYPE(char, char);
  SET_TYPE(uchar, unsigned char);
  SET_TYPE(short, short);
  SET_TYPE(ushort, unsigned short);
  SET_TYPE(int, int);
  SET_TYPE(uint, unsigned int);
  SET_TYPE(long, long);
  SET_TYPE(ulong, unsigned long);
  SET_TYPE(longlong, long long);
  SET_TYPE(ulonglong, unsigned long long);
  SET_TYPE(longdouble, long double);
  SET_TYPE(pointer, char*);
  SET_TYPE(size_t, size_t);

#undef SET_SIZEOF
#undef SET_ALIGNOF
#undef SET_TYPE

  exports->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "sizeof"), sizeof_obj);
  exports->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "alignof"), alignof_obj);
}


}  // namespace _sizeof
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(sizeof, node::_sizeof::Initialize)
