
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
using v8::Isolate;
using v8::Object;
using v8::Persistent;
using v8::Uint32;
using v8::Value;


void Initialize(Handle<Object> exports,
                Handle<Value> unused,
                Handle<Context> context) {
  Environment* env = Environment::GetCurrent(context);

  // fixed sizes
#define SET_SIZEOF(name, type) \
  exports->Set(FIXED_ONE_BYTE_STRING(env->isolate(), #name ), \
               Uint32::NewFromUnsigned(env->isolate(), static_cast<uint32_t>(sizeof( type ))));

  SET_SIZEOF(int8, int8_t);
  SET_SIZEOF(uint8, uint8_t);
  SET_SIZEOF(int16, int16_t);
  SET_SIZEOF(uint16, uint16_t);
  SET_SIZEOF(int32, int32_t);
  SET_SIZEOF(uint32, uint32_t);
  SET_SIZEOF(int64, int64_t);
  SET_SIZEOF(uint64, uint64_t);
  SET_SIZEOF(float, float);
  SET_SIZEOF(double, double);
  SET_SIZEOF(longdouble, long double);

  // (potentially) variable sizes
  SET_SIZEOF(bool, bool);
  SET_SIZEOF(byte, unsigned char);
  SET_SIZEOF(char, char);
  SET_SIZEOF(uchar, unsigned char);
  SET_SIZEOF(short, short);
  SET_SIZEOF(ushort, unsigned short);
  SET_SIZEOF(int, int);
  SET_SIZEOF(uint, unsigned int);
  SET_SIZEOF(long, long);
  SET_SIZEOF(ulong, unsigned long);
  SET_SIZEOF(longlong, long long);
  SET_SIZEOF(ulonglong, unsigned long long);
  SET_SIZEOF(pointer, char *);
  SET_SIZEOF(size_t, size_t);

  // size of a Persistent handle to a JS object
  SET_SIZEOF(Object, Persistent<Object>);

#undef SET_SIZEOF
}


}  // namespace _sizeof
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(sizeof, node::_sizeof::Initialize)
