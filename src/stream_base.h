#ifndef SRC_STREAM_BASE_H_
#define SRC_STREAM_BASE_H_

#include "v8.h"

namespace node {

// Forward declarations
class Environment;

class StreamBase {
 public:
  template <class Base>
  static void AddMethods(Environment* env,
                         v8::Handle<v8::FunctionTemplate> target);

  virtual bool IsAlive() = 0;
  virtual int GetFD() = 0;

  virtual int ReadStart(const v8::FunctionCallbackInfo<v8::Value>& args) = 0;
  virtual int ReadStop(const v8::FunctionCallbackInfo<v8::Value>& args) = 0;
  virtual int Shutdown(const v8::FunctionCallbackInfo<v8::Value>& args) = 0;
  virtual int Writev(const v8::FunctionCallbackInfo<v8::Value>& args) = 0;
  virtual int WriteBuffer(const v8::FunctionCallbackInfo<v8::Value>& args) = 0;
  virtual int WriteAsciiString(const v8::FunctionCallbackInfo<v8::Value>& args)
      = 0;
  virtual int WriteUtf8String(const v8::FunctionCallbackInfo<v8::Value>& args)
      = 0;
  virtual int WriteUcs2String(const v8::FunctionCallbackInfo<v8::Value>& args)
      = 0;
  virtual int WriteBinaryString(const v8::FunctionCallbackInfo<v8::Value>& args)
      = 0;
  virtual int SetBlocking(const v8::FunctionCallbackInfo<v8::Value>& args) = 0;

 protected:
  StreamBase(Environment* env, v8::Local<v8::Object> object);
  virtual ~StreamBase() = default;

  template <class Base>
  static void GetFD(v8::Local<v8::String>,
                    const v8::PropertyCallbackInfo<v8::Value>&);

  template <class Base,
            int (StreamBase::*Method)(
      const v8::FunctionCallbackInfo<v8::Value>& args)>
  static void JSMethod(const v8::FunctionCallbackInfo<v8::Value>& args);
};

}  // namespace node

#endif  // SRC_STREAM_BASE_H_
