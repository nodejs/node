#ifndef SRC_TLS_WRAP_H_
#define SRC_TLS_WRAP_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node.h"
#include "node_crypto.h"  // SSLWrap

#include "async-wrap.h"
#include "env.h"
#include "stream_wrap.h"
#include "util.h"
#include "v8.h"

#include <openssl/ssl.h>

namespace node {

// Forward-declarations
class NodeBIO;
class WriteWrap;
namespace crypto {
class SecureContext;
}

class TLSWrap : public AsyncWrap,
                public crypto::SSLWrap<TLSWrap>,
                public StreamBase {
 public:
  ~TLSWrap() override;

  static void Initialize(v8::Local<v8::Object> target,
                         v8::Local<v8::Value> unused,
                         v8::Local<v8::Context> context);

  void* Cast() override;
  int GetFD() override;
  bool IsAlive() override;
  bool IsClosing() override;

  // JavaScript functions
  int ReadStart() override;
  int ReadStop() override;

  int DoShutdown(ShutdownWrap* req_wrap) override;
  int DoWrite(WriteWrap* w,
              uv_buf_t* bufs,
              size_t count,
              uv_stream_t* send_handle) override;
  const char* Error() const override;
  void ClearError() override;

  void NewSessionDoneCb();

  size_t self_size() const override { return sizeof(*this); }

 protected:
  static const int kClearOutChunkSize = 16384;

  // Maximum number of bytes for hello parser
  static const int kMaxHelloLength = 16384;

  // Usual ServerHello + Certificate size
  static const int kInitialClientBufferLength = 4096;

  // Maximum number of buffers passed to uv_write()
  static const int kSimultaneousBufferCount = 10;

  // Write callback queue's item
  class WriteItem {
   public:
    explicit WriteItem(WriteWrap* w) : w_(w) {
    }
    ~WriteItem() {
      w_ = nullptr;
    }

    WriteWrap* w_;
    ListNode<WriteItem> member_;
  };

  TLSWrap(Environment* env,
          Kind kind,
          StreamBase* stream,
          crypto::SecureContext* sc);

  static void SSLInfoCallback(const SSL* ssl_, int where, int ret);
  void InitSSL();
  void EncOut();
  static void EncOutCb(WriteWrap* req_wrap, int status);
  bool ClearIn();
  void ClearOut();
  void MakePending();
  bool InvokeQueued(int status, const char* error_str = nullptr);

  inline void Cycle() {
    // Prevent recursion
    if (++cycle_depth_ > 1)
      return;

    for (; cycle_depth_ > 0; cycle_depth_--) {
      ClearIn();
      ClearOut();
      EncOut();
    }
  }

  AsyncWrap* GetAsyncWrap() override;
  bool IsIPCPipe() override;

  // Resource implementation
  static void OnAfterWriteImpl(WriteWrap* w, void* ctx);
  static void OnAllocImpl(size_t size, uv_buf_t* buf, void* ctx);
  static void OnReadImpl(ssize_t nread,
                         const uv_buf_t* buf,
                         uv_handle_type pending,
                         void* ctx);
  static void OnAfterWriteSelf(WriteWrap* w, void* ctx);
  static void OnAllocSelf(size_t size, uv_buf_t* buf, void* ctx);
  static void OnReadSelf(ssize_t nread,
                         const uv_buf_t* buf,
                         uv_handle_type pending,
                         void* ctx);

  void DoRead(ssize_t nread, const uv_buf_t* buf, uv_handle_type pending);

  // If |msg| is not nullptr, caller is responsible for calling `delete[] *msg`.
  v8::Local<v8::Value> GetSSLError(int status, int* err, const char** msg);

  static void OnClientHelloParseEnd(void* arg);
  static void Wrap(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Receive(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Start(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetVerifyMode(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void EnableSessionCallbacks(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void EnableCertCb(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void DestroySSL(const v8::FunctionCallbackInfo<v8::Value>& args);

#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
  static void GetServername(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetServername(const v8::FunctionCallbackInfo<v8::Value>& args);
  static int SelectSNIContextCallback(SSL* s, int* ad, void* arg);
#endif  // SSL_CTRL_SET_TLSEXT_SERVERNAME_CB

  crypto::SecureContext* sc_;
  StreamBase* stream_;
  BIO* enc_in_;
  BIO* enc_out_;
  NodeBIO* clear_in_;
  size_t write_size_;
  typedef ListHead<WriteItem, &WriteItem::member_> WriteItemList;
  WriteItemList write_item_queue_;
  WriteItemList pending_write_items_;
  bool started_;
  bool established_;
  bool shutdown_;
  const char* error_;
  int cycle_depth_;

  // If true - delivered EOF to the js-land, either after `close_notify`, or
  // after the `UV_EOF` on socket.
  bool eof_;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_TLS_WRAP_H_
