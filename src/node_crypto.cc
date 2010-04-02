#include <node_crypto.h>
#include <v8.h>

#include <node.h>
#include <node_buffer.h>

#include <string.h>
#include <stdlib.h>

#include <errno.h>

namespace node {

using namespace v8;

static Persistent<String> errno_symbol;
static Persistent<String> syscall_symbol;
static Persistent<String> subject_symbol;
static Persistent<String> issuer_symbol;
static Persistent<String> valid_from_symbol;
static Persistent<String> valid_to_symbol;
static Persistent<String> name_symbol;
static Persistent<String> version_symbol;

static int x509_verify_error;

static inline const char *errno_string(int errorno) {
#define ERRNO_CASE(e)  case e: return #e;
  switch (errorno) {

#ifdef EACCES
  ERRNO_CASE(EACCES);
#endif

#ifdef EADDRINUSE
  ERRNO_CASE(EADDRINUSE);
#endif

#ifdef EADDRNOTAVAIL
  ERRNO_CASE(EADDRNOTAVAIL);
#endif

#ifdef EAFNOSUPPORT
  ERRNO_CASE(EAFNOSUPPORT);
#endif

#ifdef EAGAIN
  ERRNO_CASE(EAGAIN);
#else
# ifdef EWOULDBLOCK
  ERRNO_CASE(EWOULDBLOCK);
# endif
#endif

#ifdef EALREADY
  ERRNO_CASE(EALREADY);
#endif

#ifdef EBADF
  ERRNO_CASE(EBADF);
#endif

#ifdef EBADMSG
  ERRNO_CASE(EBADMSG);
#endif

#ifdef EBUSY
  ERRNO_CASE(EBUSY);
#endif

#ifdef ECANCELED
  ERRNO_CASE(ECANCELED);
#endif

#ifdef ECHILD
  ERRNO_CASE(ECHILD);
#endif

#ifdef ECONNABORTED
  ERRNO_CASE(ECONNABORTED);
#endif

#ifdef ECONNREFUSED
  ERRNO_CASE(ECONNREFUSED);
#endif

#ifdef ECONNRESET
  ERRNO_CASE(ECONNRESET);
#endif

#ifdef EDEADLK
  ERRNO_CASE(EDEADLK);
#endif

#ifdef EDESTADDRREQ
  ERRNO_CASE(EDESTADDRREQ);
#endif

#ifdef EDOM
  ERRNO_CASE(EDOM);
#endif

#ifdef EDQUOT
  ERRNO_CASE(EDQUOT);
#endif

#ifdef EEXIST
  ERRNO_CASE(EEXIST);
#endif

#ifdef EFAULT
  ERRNO_CASE(EFAULT);
#endif

#ifdef EFBIG
  ERRNO_CASE(EFBIG);
#endif

#ifdef EHOSTUNREACH
  ERRNO_CASE(EHOSTUNREACH);
#endif

#ifdef EIDRM
  ERRNO_CASE(EIDRM);
#endif

#ifdef EILSEQ
  ERRNO_CASE(EILSEQ);
#endif

#ifdef EINPROGRESS
  ERRNO_CASE(EINPROGRESS);
#endif

#ifdef EINTR
  ERRNO_CASE(EINTR);
#endif

#ifdef EINVAL
  ERRNO_CASE(EINVAL);
#endif

#ifdef EIO
  ERRNO_CASE(EIO);
#endif

#ifdef EISCONN
  ERRNO_CASE(EISCONN);
#endif

#ifdef EISDIR
  ERRNO_CASE(EISDIR);
#endif

#ifdef ELOOP
  ERRNO_CASE(ELOOP);
#endif

#ifdef EMFILE
  ERRNO_CASE(EMFILE);
#endif

#ifdef EMLINK
  ERRNO_CASE(EMLINK);
#endif

#ifdef EMSGSIZE
  ERRNO_CASE(EMSGSIZE);
#endif

#ifdef EMULTIHOP
  ERRNO_CASE(EMULTIHOP);
#endif

#ifdef ENAMETOOLONG
  ERRNO_CASE(ENAMETOOLONG);
#endif

#ifdef ENETDOWN
  ERRNO_CASE(ENETDOWN);
#endif

#ifdef ENETRESET
  ERRNO_CASE(ENETRESET);
#endif

#ifdef ENETUNREACH
  ERRNO_CASE(ENETUNREACH);
#endif

#ifdef ENFILE
  ERRNO_CASE(ENFILE);
#endif

#ifdef ENOBUFS
  ERRNO_CASE(ENOBUFS);
#endif

#ifdef ENODATA
  ERRNO_CASE(ENODATA);
#endif

#ifdef ENODEV
  ERRNO_CASE(ENODEV);
#endif

#ifdef ENOENT
  ERRNO_CASE(ENOENT);
#endif

#ifdef ENOEXEC
  ERRNO_CASE(ENOEXEC);
#endif

#ifdef ENOLCK
  ERRNO_CASE(ENOLCK);
#endif

#ifdef ENOLINK
  ERRNO_CASE(ENOLINK);
#endif

#ifdef ENOMEM
  ERRNO_CASE(ENOMEM);
#endif

#ifdef ENOMSG
  ERRNO_CASE(ENOMSG);
#endif

#ifdef ENOPROTOOPT
  ERRNO_CASE(ENOPROTOOPT);
#endif

#ifdef ENOSPC
  ERRNO_CASE(ENOSPC);
#endif

#ifdef ENOSR
  ERRNO_CASE(ENOSR);
#endif

#ifdef ENOSTR
  ERRNO_CASE(ENOSTR);
#endif

#ifdef ENOSYS
  ERRNO_CASE(ENOSYS);
#endif

#ifdef ENOTCONN
  ERRNO_CASE(ENOTCONN);
#endif

#ifdef ENOTDIR
  ERRNO_CASE(ENOTDIR);
#endif

#ifdef ENOTEMPTY
  ERRNO_CASE(ENOTEMPTY);
#endif

#ifdef ENOTSOCK
  ERRNO_CASE(ENOTSOCK);
#endif

#ifdef ENOTSUP
  ERRNO_CASE(ENOTSUP);
#else
# ifdef EOPNOTSUPP
  ERRNO_CASE(EOPNOTSUPP);
# endif
#endif

#ifdef ENOTTY
  ERRNO_CASE(ENOTTY);
#endif

#ifdef ENXIO
  ERRNO_CASE(ENXIO);
#endif


#ifdef EOVERFLOW
  ERRNO_CASE(EOVERFLOW);
#endif

#ifdef EPERM
  ERRNO_CASE(EPERM);
#endif

#ifdef EPIPE
  ERRNO_CASE(EPIPE);
#endif

#ifdef EPROTO
  ERRNO_CASE(EPROTO);
#endif

#ifdef EPROTONOSUPPORT
  ERRNO_CASE(EPROTONOSUPPORT);
#endif

#ifdef EPROTOTYPE
  ERRNO_CASE(EPROTOTYPE);
#endif

#ifdef ERANGE
  ERRNO_CASE(ERANGE);
#endif

#ifdef EROFS
  ERRNO_CASE(EROFS);
#endif

#ifdef ESPIPE
  ERRNO_CASE(ESPIPE);
#endif

#ifdef ESRCH
  ERRNO_CASE(ESRCH);
#endif

#ifdef ESTALE
  ERRNO_CASE(ESTALE);
#endif

#ifdef ETIME
  ERRNO_CASE(ETIME);
#endif

#ifdef ETIMEDOUT
  ERRNO_CASE(ETIMEDOUT);
#endif

#ifdef ETXTBSY
  ERRNO_CASE(ETXTBSY);
#endif

#ifdef EXDEV
  ERRNO_CASE(EXDEV);
#endif

  default: return "";
  }
}




static int verify_callback(int ok, X509_STORE_CTX *ctx) {
  x509_verify_error = ctx->error;
  return(ok);
}

static inline Local<Value> ErrnoException(int errorno,
                                          const char *syscall,
                                          const char *msg = "") {
  Local<String> estring = String::NewSymbol(errno_string(errorno));
  if (!msg[0]) msg = strerror(errorno);
  Local<String> message = String::NewSymbol(msg);

  Local<String> cons1 = String::Concat(estring, String::NewSymbol(", "));
  Local<String> cons2 = String::Concat(cons1, message);

  Local<Value> e = Exception::Error(cons2);

  Local<Object> obj = e->ToObject();
  obj->Set(errno_symbol, Integer::New(errorno));
  obj->Set(syscall_symbol, String::NewSymbol(syscall));
  return e;
}

void SecureContext::Initialize(Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(SecureContext::New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(String::NewSymbol("SecureContext"));

  NODE_SET_PROTOTYPE_METHOD(t, "init", SecureContext::Init);
  NODE_SET_PROTOTYPE_METHOD(t, "setKey", SecureContext::SetKey);
  NODE_SET_PROTOTYPE_METHOD(t, "setCert", SecureContext::SetCert);
  NODE_SET_PROTOTYPE_METHOD(t, "addCACert", SecureContext::AddCACert);
  NODE_SET_PROTOTYPE_METHOD(t, "setCiphers", SecureContext::SetCiphers);
  NODE_SET_PROTOTYPE_METHOD(t, "close", SecureContext::Close);

  target->Set(String::NewSymbol("SecureContext"), t->GetFunction());
}


Handle<Value> SecureContext::New(const Arguments& args) {
  HandleScope scope;
  SecureContext *p = new SecureContext();
  p->Wrap(args.Holder());
  return args.This();
}

Handle<Value> SecureContext::Init(const Arguments& args) {
  HandleScope scope;

  SecureContext *sc = ObjectWrap::Unwrap<SecureContext>(args.Holder());

  SSL_METHOD *method = SSLv23_method();

  if (args.Length() == 1) {
    if (!args[0]->IsString())
    return ThrowException(Exception::TypeError(
          String::New("Bad parameter")));

    String::Utf8Value sslmethod(args[0]->ToString());
    if (strcmp(*sslmethod, "SSLv2_method") == 0)
      method = SSLv2_method();
    if (strcmp(*sslmethod, "SSLv2_server_method") == 0)
      method = SSLv2_server_method();
    if (strcmp(*sslmethod, "SSLv2_client_method") == 0)
      method = SSLv2_client_method();
    if (strcmp(*sslmethod, "SSLv3_method") == 0)
      method = SSLv3_method();
    if (strcmp(*sslmethod, "SSLv3_server_method") == 0)
      method = SSLv3_server_method();
    if (strcmp(*sslmethod, "SSLv3_client_method") == 0)
      method = SSLv3_client_method();
    if (strcmp(*sslmethod, "SSLv23_method") == 0)
      method = SSLv23_method();
    if (strcmp(*sslmethod, "SSLv23_server_method") == 0)
      method = SSLv23_server_method();
    if (strcmp(*sslmethod, "SSLv23_client_method") == 0)
      method = SSLv23_client_method();
    if (strcmp(*sslmethod, "TLSv1_method") == 0)
      method = TLSv1_method();
    if (strcmp(*sslmethod, "TLSv1_server_method") == 0)
      method = TLSv1_server_method();
    if (strcmp(*sslmethod, "TLSv1_client_method") == 0)
      method = TLSv1_client_method();
  }



  sc->pCtx = SSL_CTX_new(method);
  // Enable session caching?
  SSL_CTX_set_session_cache_mode(sc->pCtx, SSL_SESS_CACHE_SERVER);
  // SSL_CTX_set_session_cache_mode(sc->pCtx,SSL_SESS_CACHE_OFF);

  sc->caStore = X509_STORE_new();
  return True();
}

Handle<Value> SecureContext::SetKey(const Arguments& args) {
  HandleScope scope;

  SecureContext *sc = ObjectWrap::Unwrap<SecureContext>(args.Holder());

  if (args.Length() != 1 ||
      !args[0]->IsString()) {
    return ThrowException(Exception::TypeError(
          String::New("Bad parameter")));
  }
  String::Utf8Value keyPem(args[0]->ToString());

  BIO *bp = NULL;
  EVP_PKEY* pkey;
  bp = BIO_new(BIO_s_mem());
  if (!BIO_write(bp, *keyPem, strlen(*keyPem)))
    return False();

  pkey = PEM_read_bio_PrivateKey(bp, NULL, NULL, NULL);
  if (pkey == NULL)
    return False();

  SSL_CTX_use_PrivateKey(sc->pCtx, pkey);
  BIO_free(bp);

  return True();
}

Handle<Value> SecureContext::SetCert(const Arguments& args) {
  HandleScope scope;

  SecureContext *sc = ObjectWrap::Unwrap<SecureContext>(args.Holder());

  if (args.Length() != 1 ||
      !args[0]->IsString()) {
    return ThrowException(Exception::TypeError(
          String::New("Bad parameter")));
  }
  String::Utf8Value certPem(args[0]->ToString());

  BIO *bp = NULL;
  X509 *        x509;
  bp = BIO_new(BIO_s_mem());
  if (!BIO_write(bp, *certPem, strlen(*certPem)))
    return False();

  x509 = PEM_read_bio_X509(bp, NULL, NULL, NULL);
  if (x509 == NULL)
    return False();

  SSL_CTX_use_certificate(sc->pCtx, x509);
  BIO_free(bp);
  X509_free(x509);

  return True();
}

Handle<Value> SecureContext::AddCACert(const Arguments& args) {
  HandleScope scope;

  SecureContext *sc = ObjectWrap::Unwrap<SecureContext>(args.Holder());

  if (args.Length() != 1 ||
      !args[0]->IsString()) {
    return ThrowException(Exception::TypeError(
          String::New("Bad parameter")));
  }
  String::Utf8Value certPem(args[0]->ToString());

  BIO *bp = NULL;
  X509 *x509;
  bp = BIO_new(BIO_s_mem());
  if (!BIO_write(bp, *certPem, strlen(*certPem)))
    return False();

  x509 = PEM_read_bio_X509(bp, NULL, NULL, NULL);
  if (x509 == NULL)
    return False();

  X509_STORE_add_cert(sc->caStore, x509);

  BIO_free(bp);
  X509_free(x509);

  return True();
}


Handle<Value> SecureContext::SetCiphers(const Arguments& args) {
  HandleScope scope;

  SecureContext *sc = ObjectWrap::Unwrap<SecureContext>(args.Holder());

  if (args.Length() != 1 ||
      !args[0]->IsString()) {
    return ThrowException(Exception::TypeError(
          String::New("Bad parameter")));
  }
  String::Utf8Value ciphers(args[0]->ToString());
  SSL_CTX_set_cipher_list(sc->pCtx, *ciphers);

  return True();
}


Handle<Value> SecureContext::Close(const Arguments& args) {
  HandleScope scope;

  SecureContext *sc = ObjectWrap::Unwrap<SecureContext>(args.Holder());

  if (sc->pCtx != NULL) {
    SSL_CTX_free(sc->pCtx);
    return True();
  }
  return False();
}



void SecureStream::Initialize(Handle<Object> target) {
  HandleScope scope;

  Local<FunctionTemplate> t = FunctionTemplate::New(SecureStream::New);
  t->InstanceTemplate()->SetInternalFieldCount(1);
  t->SetClassName(String::NewSymbol("SecureStream"));

  NODE_SET_PROTOTYPE_METHOD(t, "readInject",
                            SecureStream::ReadInject);
  NODE_SET_PROTOTYPE_METHOD(t, "readExtract",
                            SecureStream::ReadExtract);
  NODE_SET_PROTOTYPE_METHOD(t, "writeInject",
                            SecureStream::WriteInject);
  NODE_SET_PROTOTYPE_METHOD(t, "writeExtract",
                            SecureStream::WriteExtract);
  NODE_SET_PROTOTYPE_METHOD(t, "writeCanExtract",
                            SecureStream::WriteCanExtract);
  NODE_SET_PROTOTYPE_METHOD(t, "getPeerCertificate",
                            SecureStream::GetPeerCertificate);
  NODE_SET_PROTOTYPE_METHOD(t, "isInitFinished",
                            SecureStream::IsInitFinished);
  NODE_SET_PROTOTYPE_METHOD(t, "verifyPeer",
                            SecureStream::VerifyPeer);
  NODE_SET_PROTOTYPE_METHOD(t, "getCurrentCipher",
                            SecureStream::GetCurrentCipher);
  NODE_SET_PROTOTYPE_METHOD(t, "shutdown",
                            SecureStream::Shutdown);
  NODE_SET_PROTOTYPE_METHOD(t, "close",
                            SecureStream::Close);

  target->Set(String::NewSymbol("SecureStream"), t->GetFunction());
}


Handle<Value> SecureStream::New(const Arguments& args) {
  HandleScope scope;
  SecureStream *p = new SecureStream();
  p->Wrap(args.Holder());

  if (args.Length() != 2 ||
      !args[0]->IsObject() ||
      !args[1]->IsNumber()) {
    return ThrowException(Exception::Error(String::New("Bad arguments.")));
  }
  SecureContext *sc = ObjectWrap::Unwrap<SecureContext>(args[0]->ToObject());
  int isServer = args[1]->Int32Value();

  p->pSSL = SSL_new(sc->pCtx);
  p->pbioRead = BIO_new(BIO_s_mem());
  p->pbioWrite = BIO_new(BIO_s_mem());
  SSL_set_bio(p->pSSL, p->pbioRead, p->pbioWrite);
  p->server = isServer>0;
  if (p->server) {
    SSL_set_accept_state(p->pSSL);
  } else {
    SSL_set_connect_state(p->pSSL);
  }

  return args.This();
}


Handle<Value> SecureStream::ReadInject(const Arguments& args) {
  HandleScope scope;

  SecureStream *ss = ObjectWrap::Unwrap<SecureStream>(args.Holder());

  if (args.Length() < 3) {
    return ThrowException(Exception::TypeError(
          String::New("Takes 3 parameters")));
  }

  if (!Buffer::HasInstance(args[0])) {
    return ThrowException(Exception::TypeError(
          String::New("Second argument should be a buffer")));
  }

  Buffer * buffer = ObjectWrap::Unwrap<Buffer>(args[0]->ToObject());

  size_t off = args[1]->Int32Value();
  if (off >= buffer->length()) {
    return ThrowException(Exception::Error(
          String::New("Offset is out of bounds")));
  }

  size_t len = args[2]->Int32Value();
  if (off + len > buffer->length()) {
    return ThrowException(Exception::Error(
          String::New("Length is extends beyond buffer")));
  }

  int bytes_written = BIO_write(ss->pbioRead, (char*)buffer->data() + off, len);

  if (bytes_written < 0) {
    if (errno == EAGAIN || errno == EINTR) return Null();
    return ThrowException(ErrnoException(errno, "read"));
  }

  return scope.Close(Integer::New(bytes_written));
}

Handle<Value> SecureStream::ReadExtract(const Arguments& args) {
  HandleScope scope;

  SecureStream *ss = ObjectWrap::Unwrap<SecureStream>(args.Holder());

  if (args.Length() < 3) {
    return ThrowException(Exception::TypeError(
          String::New("Takes 3 parameters")));
  }

  if (!Buffer::HasInstance(args[0])) {
    return ThrowException(Exception::TypeError(
          String::New("Second argument should be a buffer")));
  }

  Buffer * buffer = ObjectWrap::Unwrap<Buffer>(args[0]->ToObject());

  size_t off = args[1]->Int32Value();
  if (off >= buffer->length()) {
    return ThrowException(Exception::Error(
          String::New("Offset is out of bounds")));
  }

  size_t len = args[2]->Int32Value();
  if (off + len > buffer->length()) {
    return ThrowException(Exception::Error(
          String::New("Length is extends beyond buffer")));
  }

  int bytes_read;

  if (!SSL_is_init_finished(ss->pSSL)) {
    if (ss->server) {
      bytes_read = SSL_accept(ss->pSSL);
    } else {
      bytes_read = SSL_connect(ss->pSSL);
    }
    if (bytes_read < 0) {
      int err;
      if ((err = SSL_get_error(ss->pSSL, bytes_read)) == SSL_ERROR_WANT_READ) {
        return scope.Close(Integer::New(0));
      }
    }
    return scope.Close(Integer::New(0));
  }

  bytes_read = SSL_read(ss->pSSL, (char*)buffer->data() + off, len);
  if (bytes_read < 0) {
    int err = SSL_get_error(ss->pSSL, bytes_read);
    if (err == SSL_ERROR_WANT_READ) {
      return scope.Close(Integer::New(0));
    }
    // SSL read error
    return scope.Close(Integer::New(-2));
  }

  if (bytes_read < 0) {
    if (errno == EAGAIN || errno == EINTR) return Null();
    return ThrowException(ErrnoException(errno, "read"));
  }

  return scope.Close(Integer::New(bytes_read));
}

Handle<Value> SecureStream::WriteCanExtract(const Arguments& args) {
  HandleScope scope;

  SecureStream *ss = ObjectWrap::Unwrap<SecureStream>(args.Holder());
  int bytes_pending = BIO_pending(ss->pbioWrite);
  return scope.Close(Integer::New(bytes_pending));
}

Handle<Value> SecureStream::WriteExtract(const Arguments& args) {
  HandleScope scope;

  SecureStream *ss = ObjectWrap::Unwrap<SecureStream>(args.Holder());

  if (args.Length() < 3) {
    return ThrowException(Exception::TypeError(
          String::New("Takes 3 parameters")));
  }

  if (!Buffer::HasInstance(args[0])) {
    return ThrowException(Exception::TypeError(
          String::New("Second argument should be a buffer")));
  }

  Buffer * buffer = ObjectWrap::Unwrap<Buffer>(args[0]->ToObject());

  size_t off = args[1]->Int32Value();
  if (off >= buffer->length()) {
    return ThrowException(Exception::Error(
          String::New("Offset is out of bounds")));
  }

  size_t len = args[2]->Int32Value();
  if (off + len > buffer->length()) {
    return ThrowException(Exception::Error(
          String::New("Length is extends beyond buffer")));
  }

  int bytes_read = BIO_read(ss->pbioWrite, (char*)buffer->data() + off, len);

  return scope.Close(Integer::New(bytes_read));
}


Handle<Value> SecureStream::WriteInject(const Arguments& args) {
  HandleScope scope;

  SecureStream *ss = ObjectWrap::Unwrap<SecureStream>(args.Holder());

  if (args.Length() < 3) {
    return ThrowException(Exception::TypeError(
          String::New("Takes 3 parameters")));
  }

  if (!Buffer::HasInstance(args[0])) {
    return ThrowException(Exception::TypeError(
          String::New("Second argument should be a buffer")));
  }

  Buffer * buffer = ObjectWrap::Unwrap<Buffer>(args[0]->ToObject());

  size_t off = args[1]->Int32Value();
  if (off >= buffer->length()) {
    return ThrowException(Exception::Error(
          String::New("Offset is out of bounds")));
  }

  size_t len = args[2]->Int32Value();
  if (off + len > buffer->length()) {
    return ThrowException(Exception::Error(
          String::New("Length is extends beyond buffer")));
  }

  if (!SSL_is_init_finished(ss->pSSL)) {
    int s;
    if (ss->server) {
      s = SSL_accept(ss->pSSL);
    } else {
      s = SSL_connect(ss->pSSL);
    }
    return scope.Close(Integer::New(0));
  }
  int bytes_written = SSL_write(ss->pSSL, (char*)buffer->data() + off, len);

  return scope.Close(Integer::New(bytes_written));
}


Handle<Value> SecureStream::GetPeerCertificate(const Arguments& args) {
  HandleScope scope;

  SecureStream *ss = ObjectWrap::Unwrap<SecureStream>(args.Holder());

  if (ss->pSSL == NULL) return Undefined();
  Local<Object> info = Object::New();
  X509* peer_cert = SSL_get_peer_certificate(ss->pSSL);
  if (peer_cert != NULL) {
    char* subject = X509_NAME_oneline(X509_get_subject_name(peer_cert), 0, 0);
    if (subject != NULL) {
      info->Set(subject_symbol, String::New(subject));
      OPENSSL_free(subject);
    }
    char* issuer = X509_NAME_oneline(X509_get_issuer_name(peer_cert), 0, 0);
    if (subject != NULL) {
      info->Set(issuer_symbol, String::New(issuer));
      OPENSSL_free(issuer);
    }
    char buf[256];
    BIO* bio = BIO_new(BIO_s_mem());
    ASN1_TIME_print(bio, X509_get_notBefore(peer_cert));
    memset(buf, 0, sizeof(buf));
    BIO_read(bio, buf, sizeof(buf) - 1);
    info->Set(valid_from_symbol, String::New(buf));
    ASN1_TIME_print(bio, X509_get_notAfter(peer_cert));
    memset(buf, 0, sizeof(buf));
    BIO_read(bio, buf, sizeof(buf) - 1);
    BIO_free(bio);
    info->Set(valid_to_symbol, String::New(buf));

    X509_free(peer_cert);
  }
  return scope.Close(info);
}

Handle<Value> SecureStream::Shutdown(const Arguments& args) {
  HandleScope scope;

  SecureStream *ss = ObjectWrap::Unwrap<SecureStream>(args.Holder());

  if (ss->pSSL == NULL) return False();
  if (SSL_shutdown(ss->pSSL) == 1) {
    return True();
  }
  return False();
}

Handle<Value> SecureStream::IsInitFinished(const Arguments& args) {
  HandleScope scope;

  SecureStream *ss = ObjectWrap::Unwrap<SecureStream>(args.Holder());

  if (ss->pSSL == NULL) return False();
  if (SSL_is_init_finished(ss->pSSL)) {
    return True();
  }
  return False();
}


Handle<Value> SecureStream::VerifyPeer(const Arguments& args) {
  HandleScope scope;

  SecureStream *ss = ObjectWrap::Unwrap<SecureStream>(args.Holder());
  SecureContext *sc = ObjectWrap::Unwrap<SecureContext>(args[0]->ToObject());

  if (ss->pSSL == NULL) return False();
  if (sc->caStore == NULL) return False();

  X509 *cert = SSL_get_peer_certificate(ss->pSSL);
  STACK_OF(X509) *certChain = SSL_get_peer_cert_chain(ss->pSSL);
  X509_STORE_set_verify_cb_func(sc->caStore, verify_callback);
  X509_STORE_CTX *storeCtx = X509_STORE_CTX_new();
  X509_STORE_CTX_init(storeCtx, sc->caStore, cert, certChain);

  x509_verify_error = 0;
  // OS X Bug in openssl : x509_verify_cert is always true?
  // This is why we have our global.
  X509_verify_cert(storeCtx);

  X509_STORE_CTX_free(storeCtx);

  // Can also check for:
  // X509_V_ERR_CERT_HAS_EXPIRED
  // X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT
  // X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN
  // X509_V_ERR_INVALID_CA
  // X509_V_ERR_PATH_LENGTH_EXCEEDED
  // X509_V_ERR_INVALID_PURPOSE
  // X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT

  // printf("%s\n", X509_verify_cert_error_string(x509_verify_error));

  if (!x509_verify_error) return True();
  return False();
}

Handle<Value> SecureStream::GetCurrentCipher(const Arguments& args) {
  HandleScope scope;

  SecureStream *ss = ObjectWrap::Unwrap<SecureStream>(args.Holder());
  SSL_CIPHER *c;

  if ( ss->pSSL == NULL ) return Undefined();
  c = SSL_get_current_cipher(ss->pSSL);
  if ( c == NULL ) return Undefined();
  Local<Object> info = Object::New();
  const char *cipher_name = SSL_CIPHER_get_name(c);
  info->Set(name_symbol, String::New(cipher_name));
  const char *cipher_version = SSL_CIPHER_get_version(c);
  info->Set(version_symbol, String::New(cipher_version));
  return scope.Close(info);
}

Handle<Value> SecureStream::Close(const Arguments& args) {
  HandleScope scope;

  SecureStream *ss = ObjectWrap::Unwrap<SecureStream>(args.Holder());

  if (ss->pSSL != NULL) {
    SSL_free(ss->pSSL);
    ss->pSSL = NULL;
  }
  return True();
}



void InitCrypto(Handle<Object> target) {
  HandleScope scope;

  SSL_library_init();
  OpenSSL_add_ssl_algorithms();
  SSL_load_error_strings();
  ERR_load_crypto_strings();

  SecureContext::Initialize(target);
  SecureStream::Initialize(target);

  subject_symbol        = NODE_PSYMBOL("subject");
  issuer_symbol        = NODE_PSYMBOL("issuer");
  valid_from_symbol        = NODE_PSYMBOL("valid_from");
  valid_to_symbol        = NODE_PSYMBOL("valid_to");
  name_symbol        = NODE_PSYMBOL("name");
  version_symbol        = NODE_PSYMBOL("version");
}



}  // namespace node

