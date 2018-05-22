#ifndef SRC_NODE_PROCESS_H_
#define SRC_NODE_PROCESS_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"

namespace node {

using v8::Array;
using v8::Boolean;
using v8::FunctionCallbackInfo;
using v8::Integer;
using v8::Local;
using v8::Name;
using v8::Object;
using v8::PropertyCallbackInfo;
using v8::Value;

void Abort(const FunctionCallbackInfo<Value>& args);
void Chdir(const FunctionCallbackInfo<Value>& args);
void CPUUsage(const FunctionCallbackInfo<Value>& args);
void Cwd(const FunctionCallbackInfo<Value>& args);
void DLOpen(const FunctionCallbackInfo<Value>& args);
void Exit(const FunctionCallbackInfo<Value>& args);
void GetActiveRequests(const FunctionCallbackInfo<Value>& args);
void GetActiveHandles(const FunctionCallbackInfo<Value>& args);
void Hrtime(const FunctionCallbackInfo<Value>& args);
void Kill(const FunctionCallbackInfo<Value>& args);
void MemoryUsage(const FunctionCallbackInfo<Value>& args);
void RawDebug(const FunctionCallbackInfo<Value>& args);
void StartProfilerIdleNotifier(const FunctionCallbackInfo<Value>& args);
void StopProfilerIdleNotifier(const FunctionCallbackInfo<Value>& args);
void Umask(const FunctionCallbackInfo<Value>& args);
void Uptime(const FunctionCallbackInfo<Value>& args);

#if defined(__POSIX__) && !defined(__ANDROID__) && !defined(__CloudABI__)
void GetUid(const FunctionCallbackInfo<Value>& args);
void GetGid(const FunctionCallbackInfo<Value>& args);
void GetEUid(const FunctionCallbackInfo<Value>& args);
void GetEGid(const FunctionCallbackInfo<Value>& args);
void SetGid(const FunctionCallbackInfo<Value>& args);
void SetEGid(const FunctionCallbackInfo<Value>& args);
void SetUid(const FunctionCallbackInfo<Value>& args);
void SetEUid(const FunctionCallbackInfo<Value>& args);
void GetGroups(const FunctionCallbackInfo<Value>& args);
void SetGroups(const FunctionCallbackInfo<Value>& args);
void InitGroups(const FunctionCallbackInfo<Value>& args);
#endif  // __POSIX__ && !defined(__ANDROID__) && !defined(__CloudABI__)

void DebugPortGetter(Local<Name> property,
                     const PropertyCallbackInfo<Value>& info);

void DebugPortSetter(Local<Name> property,
                     Local<Value> value,
                     const PropertyCallbackInfo<void>& info);

void GetParentProcessId(Local<Name> property,
                        const PropertyCallbackInfo<Value>& info);

void EnvGetter(Local<Name> property,
               const PropertyCallbackInfo<Value>& info);
void EnvSetter(Local<Name> property,
               Local<Value> value,
               const PropertyCallbackInfo<Value>& info);
void EnvQuery(Local<Name> property, const PropertyCallbackInfo<Integer>& info);
void EnvDeleter(Local<Name> property,
                const PropertyCallbackInfo<Boolean>& info);
void EnvEnumerator(const PropertyCallbackInfo<Array>& info);

void ProcessTitleGetter(Local<Name> property,
                        const PropertyCallbackInfo<Value>& info);
void ProcessTitleSetter(Local<Name> property,
                        Local<Value> value,
                        const PropertyCallbackInfo<void>& info);

Local<Object> GetFeatures(Environment* env);

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_PROCESS_H_
