#ifndef SRC_STREAM_BASE_H_
#define SRC_STREAM_BASE_H_

#include "node.h"
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

  virtual int ReadStart() = 0;
  virtual int ReadStop() = 0;
  virtual int Shutdown(v8::Local<v8::Object> req) = 0;
  virtual int Writev(v8::Local<v8::Object> req, v8::Local<v8::Array> bufs) = 0;
  virtual int WriteBuffer(v8::Local<v8::Object> req,
                          const char* buf,
                          size_t len) = 0;
  virtual int WriteString(v8::Local<v8::Object> req,
                          v8::Local<v8::String> str,
                          enum encoding enc,
                          v8::Local<v8::Object> handle) = 0;
  virtual int SetBlocking(bool enable) = 0;

 protected:
  StreamBase(Environment* env, v8::Local<v8::Object> object);
  virtual ~StreamBase() = default;

  int ReadStart(const v8::FunctionCallbackInfo<v8::Value>& args);
  int ReadStop(const v8::FunctionCallbackInfo<v8::Value>& args);
  int Shutdown(const v8::FunctionCallbackInfo<v8::Value>& args);
  int Writev(const v8::FunctionCallbackInfo<v8::Value>& args);
  int WriteBuffer(const v8::FunctionCallbackInfo<v8::Value>& args);
  template <enum encoding Encoding>
  int WriteString(const v8::FunctionCallbackInfo<v8::Value>& args);
  int SetBlocking(const v8::FunctionCallbackInfo<v8::Value>& args);

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
