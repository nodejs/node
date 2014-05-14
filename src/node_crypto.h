// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef SRC_NODE_CRYPTO_H_
#define SRC_NODE_CRYPTO_H_

#include "node.h"

#include "node_object_wrap.h"
#include "v8.h"

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/pkcs12.h>

#ifdef OPENSSL_NPN_NEGOTIATED
#include "node_buffer.h"
#endif

#define EVP_F_EVP_DECRYPTFINAL 101

namespace node {
namespace crypto {

static X509_STORE* root_cert_store;

// Forward declaration
class Connection;

class SecureContext : ObjectWrap {
 public:
  static void Initialize(v8::Handle<v8::Object> target);

  SSL_CTX *ctx_;
  // TODO: ca_store_ should probably be removed, it's not used anywhere.
  X509_STORE *ca_store_;

 protected:
  static const int kMaxSessionSize = 10 * 1024;

  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> Init(const v8::Arguments& args);
  static v8::Handle<v8::Value> SetKey(const v8::Arguments& args);
  static v8::Handle<v8::Value> SetCert(const v8::Arguments& args);
  static v8::Handle<v8::Value> AddCACert(const v8::Arguments& args);
  static v8::Handle<v8::Value> AddCRL(const v8::Arguments& args);
  static v8::Handle<v8::Value> AddRootCerts(const v8::Arguments& args);
  static v8::Handle<v8::Value> SetCiphers(const v8::Arguments& args);
  static v8::Handle<v8::Value> SetOptions(const v8::Arguments& args);
  static v8::Handle<v8::Value> SetSessionIdContext(const v8::Arguments& args);
  static v8::Handle<v8::Value> Close(const v8::Arguments& args);
  static v8::Handle<v8::Value> LoadPKCS12(const v8::Arguments& args);

  static SSL_SESSION* GetSessionCallback(SSL* s,
                                         unsigned char* key,
                                         int len,
                                         int* copy);
  static int NewSessionCallback(SSL* s, SSL_SESSION* sess);

  SecureContext() : ObjectWrap() {
    ctx_ = NULL;
    ca_store_ = NULL;
  }

  void FreeCTXMem() {
    if (ctx_) {
      if (ctx_->cert_store == root_cert_store) {
        // SSL_CTX_free() will attempt to free the cert_store as well.
        // Since we want our root_cert_store to stay around forever
        // we just clear the field. Hopefully OpenSSL will not modify this
        // struct in future versions.
        ctx_->cert_store = NULL;
      }
      SSL_CTX_free(ctx_);
      ctx_ = NULL;
      ca_store_ = NULL;
    } else {
      assert(ca_store_ == NULL);
    }
  }

  ~SecureContext() {
    FreeCTXMem();
  }

 private:
};

class ClientHelloParser {
 public:
  enum FrameType {
    kChangeCipherSpec = 20,
    kAlert = 21,
    kHandshake = 22,
    kApplicationData = 23,
    kOther = 255
  };

  enum HandshakeType {
    kClientHello = 1
  };

  enum ParseState {
    kWaiting,
    kTLSHeader,
    kSSLHeader,
    kPaused,
    kEnded
  };

  ClientHelloParser(Connection* c) : conn_(c),
                                     state_(kWaiting),
                                     offset_(0),
                                     body_offset_(0) {
    data_ = new uint8_t[kBufferSize];
    if (!data_)
      abort();
  }

  ~ClientHelloParser() {
    if (data_) {
      delete[] data_;
      data_ = NULL;
    }
  }

  size_t Write(const uint8_t* data, size_t len);
  void Finish();

  inline bool ended() { return state_ == kEnded; }

 private:
  static const int kBufferSize = 18432;
  Connection* conn_;
  ParseState state_;
  size_t frame_len_;

  uint8_t* data_;
  size_t offset_;
  size_t body_offset_;
};

class Connection : ObjectWrap {
 public:
  static void Initialize(v8::Handle<v8::Object> target);

#ifdef OPENSSL_NPN_NEGOTIATED
  v8::Persistent<v8::Object> npnProtos_;
  v8::Persistent<v8::Value> selectedNPNProto_;
#endif

#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
  v8::Persistent<v8::Object> sniObject_;
  v8::Persistent<v8::Value> sniContext_;
  v8::Persistent<v8::String> servername_;
#endif

 protected:
  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> EncIn(const v8::Arguments& args);
  static v8::Handle<v8::Value> ClearOut(const v8::Arguments& args);
  static v8::Handle<v8::Value> ClearPending(const v8::Arguments& args);
  static v8::Handle<v8::Value> EncPending(const v8::Arguments& args);
  static v8::Handle<v8::Value> EncOut(const v8::Arguments& args);
  static v8::Handle<v8::Value> ClearIn(const v8::Arguments& args);
  static v8::Handle<v8::Value> GetPeerCertificate(const v8::Arguments& args);
  static v8::Handle<v8::Value> GetSession(const v8::Arguments& args);
  static v8::Handle<v8::Value> SetSession(const v8::Arguments& args);
  static v8::Handle<v8::Value> LoadSession(const v8::Arguments& args);
  static v8::Handle<v8::Value> IsSessionReused(const v8::Arguments& args);
  static v8::Handle<v8::Value> IsInitFinished(const v8::Arguments& args);
  static v8::Handle<v8::Value> VerifyError(const v8::Arguments& args);
  static v8::Handle<v8::Value> GetCurrentCipher(const v8::Arguments& args);
  static v8::Handle<v8::Value> Shutdown(const v8::Arguments& args);
  static v8::Handle<v8::Value> Start(const v8::Arguments& args);
  static v8::Handle<v8::Value> Close(const v8::Arguments& args);

  static void InitNPN(SecureContext* sc, bool is_server);

#ifdef OPENSSL_NPN_NEGOTIATED
  // NPN
  static v8::Handle<v8::Value> GetNegotiatedProto(const v8::Arguments& args);
  static v8::Handle<v8::Value> SetNPNProtocols(const v8::Arguments& args);
  static int AdvertiseNextProtoCallback_(SSL *s,
                                         const unsigned char **data,
                                         unsigned int *len,
                                         void *arg);
  static int SelectNextProtoCallback_(SSL *s,
                                      unsigned char **out, unsigned char *outlen,
                                      const unsigned char* in,
                                      unsigned int inlen, void *arg);
#endif

#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
  // SNI
  static v8::Handle<v8::Value> GetServername(const v8::Arguments& args);
  static v8::Handle<v8::Value> SetSNICallback(const v8::Arguments& args);
  static int SelectSNIContextCallback_(SSL *s, int *ad, void* arg);
#endif

  int HandleBIOError(BIO *bio, const char* func, int rv);

  enum ZeroStatus {
    kZeroIsNotAnError,
    kZeroIsAnError
  };

  enum SyscallStatus {
    kIgnoreSyscall,
    kSyscallError
  };

  int HandleSSLError(const char* func, int rv, ZeroStatus zs, SyscallStatus ss);

  void ClearError();
  void SetShutdownFlags();

  static Connection* Unwrap(const v8::Arguments& args) {
    Connection* ss = ObjectWrap::Unwrap<Connection>(args.Holder());
    ss->ClearError();
    return ss;
  }

  Connection() : ObjectWrap(), hello_parser_(this) {
    bio_read_ = bio_write_ = NULL;
    ssl_ = NULL;
    next_sess_ = NULL;
  }

  ~Connection() {
    if (ssl_ != NULL) {
      SSL_free(ssl_);
      ssl_ = NULL;
    }

    if (next_sess_ != NULL) {
      SSL_SESSION_free(next_sess_);
      next_sess_ = NULL;
    }

#ifdef OPENSSL_NPN_NEGOTIATED
    if (!npnProtos_.IsEmpty()) npnProtos_.Dispose();
    if (!selectedNPNProto_.IsEmpty()) selectedNPNProto_.Dispose();
#endif

#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
   if (!sniObject_.IsEmpty()) sniObject_.Dispose();
   if (!sniContext_.IsEmpty()) sniContext_.Dispose();
   if (!servername_.IsEmpty()) servername_.Dispose();
#endif
  }

 private:
  static void SSLInfoCallback(const SSL *ssl, int where, int ret);

  BIO *bio_read_;
  BIO *bio_write_;
  SSL *ssl_;

  ClientHelloParser hello_parser_;

  bool is_server_; /* coverity[member_decl] */
  SSL_SESSION* next_sess_;

  friend class ClientHelloParser;
  friend class SecureContext;
};

bool EntropySource(unsigned char* buffer, size_t length);
void InitCrypto(v8::Handle<v8::Object> target);

}  // namespace crypto
}  // namespace node

#endif  // SRC_NODE_CRYPTO_H_
