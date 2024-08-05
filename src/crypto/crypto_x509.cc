#include "crypto_x509.h"
#include "base_object-inl.h"
#include "crypto_bio.h"
#include "crypto_common.h"
#include "crypto_context.h"
#include "crypto_keys.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "ncrypto.h"
#include "node_errors.h"
#include "util-inl.h"
#include "v8.h"

#include <string>
#include <vector>

namespace node {

using v8::Array;
using v8::ArrayBuffer;
using v8::ArrayBufferView;
using v8::Context;
using v8::EscapableHandleScope;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::NewStringType;
using v8::Object;
using v8::String;
using v8::Uint32;
using v8::Value;

namespace crypto {

ManagedX509::ManagedX509(X509Pointer&& cert) : cert_(std::move(cert)) {}

ManagedX509::ManagedX509(const ManagedX509& that) {
  *this = that;
}

ManagedX509& ManagedX509::operator=(const ManagedX509& that) {
  cert_.reset(that.get());

  if (cert_)
    X509_up_ref(cert_.get());

  return *this;
}

void ManagedX509::MemoryInfo(MemoryTracker* tracker) const {
  // This is an approximation based on the der encoding size.
  int size = i2d_X509(cert_.get(), nullptr);
  tracker->TrackFieldWithSize("cert", size);
}

namespace {
template <const EVP_MD* (*algo)()>
void Fingerprint(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  X509Certificate* cert;
  ASSIGN_OR_RETURN_UNWRAP(&cert, args.This());
  Local<Value> ret;
  if (GetFingerprintDigest(env, algo(), cert->get()).ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
}

MaybeLocal<Value> ToV8Value(Local<Context> context, BIOPointer&& bio) {
  if (!bio) return {};
  BUF_MEM* mem;
  BIO_get_mem_ptr(bio.get(), &mem);
  Local<Value> ret;
  if (!String::NewFromUtf8(context->GetIsolate(),
                           mem->data,
                           NewStringType::kNormal,
                           mem->length)
           .ToLocal(&ret))
    return {};
  return ret;
}

MaybeLocal<Value> ToBuffer(Environment* env, BIOPointer&& bio) {
  if (!bio) return {};
  BUF_MEM* mem;
  BIO_get_mem_ptr(bio.get(), &mem);
  auto backing = ArrayBuffer::NewBackingStore(
      mem->data,
      mem->length,
      [](void*, size_t, void* data) {
        BIOPointer free_me(static_cast<BIO*>(data));
      },
      bio.release());
  auto ab = ArrayBuffer::New(env->isolate(), std::move(backing));
  Local<Value> ret;
  if (!Buffer::New(env, ab, 0, ab->ByteLength()).ToLocal(&ret)) return {};
  return ret;
}

void Pem(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  X509Certificate* cert;
  ASSIGN_OR_RETURN_UNWRAP(&cert, args.This());
  Local<Value> ret;
  if (ToV8Value(env->context(), cert->view().toPEM()).ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
}

void Der(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  X509Certificate* cert;
  ASSIGN_OR_RETURN_UNWRAP(&cert, args.This());
  Local<Value> ret;
  if (ToBuffer(env, cert->view().toDER()).ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
}

void Subject(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  X509Certificate* cert;
  ASSIGN_OR_RETURN_UNWRAP(&cert, args.This());
  Local<Value> ret;
  if (ToV8Value(env->context(), cert->view().getSubject()).ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
}

void SubjectAltName(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  X509Certificate* cert;
  ASSIGN_OR_RETURN_UNWRAP(&cert, args.This());
  Local<Value> ret;
  if (ToV8Value(env->context(), cert->view().getSubjectAltName())
          .ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
}

void Issuer(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  X509Certificate* cert;
  ASSIGN_OR_RETURN_UNWRAP(&cert, args.This());
  Local<Value> ret;
  if (ToV8Value(env->context(), cert->view().getIssuer()).ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
}

void InfoAccess(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  X509Certificate* cert;
  ASSIGN_OR_RETURN_UNWRAP(&cert, args.This());
  Local<Value> ret;
  if (ToV8Value(env->context(), cert->view().getInfoAccess()).ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
}

void ValidFrom(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  X509Certificate* cert;
  ASSIGN_OR_RETURN_UNWRAP(&cert, args.This());
  Local<Value> ret;
  if (ToV8Value(env->context(), cert->view().getValidFrom()).ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
}

void ValidTo(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  X509Certificate* cert;
  ASSIGN_OR_RETURN_UNWRAP(&cert, args.This());
  Local<Value> ret;
  if (ToV8Value(env->context(), cert->view().getValidTo()).ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
}

void SerialNumber(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  X509Certificate* cert;
  ASSIGN_OR_RETURN_UNWRAP(&cert, args.This());
  if (auto serial = cert->view().getSerialNumber()) {
    args.GetReturnValue().Set(OneByteString(
        env->isolate(), static_cast<unsigned char*>(serial.get())));
  }
}

void PublicKey(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  X509Certificate* cert;
  ASSIGN_OR_RETURN_UNWRAP(&cert, args.This());

  // TODO(tniessen): consider checking X509_get_pubkey() when the
  // X509Certificate object is being created.
  auto result = cert->view().getPublicKey();
  if (!result.value) {
    ThrowCryptoError(env, result.error.value_or(0));
    return;
  }
  std::shared_ptr<KeyObjectData> key_data = KeyObjectData::CreateAsymmetric(
      kKeyTypePublic, ManagedEVPPKey(std::move(result.value)));

  Local<Value> ret;
  if (KeyObjectHandle::Create(env, std::move(key_data)).ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
}

void KeyUsage(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  X509Certificate* cert;
  ASSIGN_OR_RETURN_UNWRAP(&cert, args.This());

  auto eku = cert->view().getKeyUsage();
  if (!eku) return;

  const int count = sk_ASN1_OBJECT_num(eku.get());
  MaybeStackBuffer<Local<Value>, 16> ext_key_usage(count);
  char buf[256];

  int j = 0;
  for (int i = 0; i < count; i++) {
    if (OBJ_obj2txt(buf, sizeof(buf), sk_ASN1_OBJECT_value(eku.get(), i), 1) >=
        0) {
      ext_key_usage[j++] = OneByteString(env->isolate(), buf);
    }
  }

  args.GetReturnValue().Set(
      Array::New(env->isolate(), ext_key_usage.out(), count));
}

void CheckCA(const FunctionCallbackInfo<Value>& args) {
  X509Certificate* cert;
  ASSIGN_OR_RETURN_UNWRAP(&cert, args.This());
  args.GetReturnValue().Set(cert->view().isCA());
}

void CheckIssued(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  X509Certificate* cert;
  ASSIGN_OR_RETURN_UNWRAP(&cert, args.This());
  CHECK(args[0]->IsObject());
  CHECK(X509Certificate::HasInstance(env, args[0].As<Object>()));
  X509Certificate* issuer;
  ASSIGN_OR_RETURN_UNWRAP(&issuer, args[0]);
  args.GetReturnValue().Set(cert->view().isIssuedBy(issuer->view()));
}

void CheckPrivateKey(const FunctionCallbackInfo<Value>& args) {
  X509Certificate* cert;
  ASSIGN_OR_RETURN_UNWRAP(&cert, args.This());
  CHECK(args[0]->IsObject());
  KeyObjectHandle* key;
  ASSIGN_OR_RETURN_UNWRAP(&key, args[0]);
  CHECK_EQ(key->Data()->GetKeyType(), kKeyTypePrivate);
  args.GetReturnValue().Set(
      cert->view().checkPrivateKey(key->Data()->GetAsymmetricKey().pkey()));
}

void CheckPublicKey(const FunctionCallbackInfo<Value>& args) {
  X509Certificate* cert;
  ASSIGN_OR_RETURN_UNWRAP(&cert, args.This());

  CHECK(args[0]->IsObject());
  KeyObjectHandle* key;
  ASSIGN_OR_RETURN_UNWRAP(&key, args[0]);
  CHECK_EQ(key->Data()->GetKeyType(), kKeyTypePublic);

  args.GetReturnValue().Set(
      cert->view().checkPublicKey(key->Data()->GetAsymmetricKey().pkey()));
}

void CheckHost(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  X509Certificate* cert;
  ASSIGN_OR_RETURN_UNWRAP(&cert, args.This());

  CHECK(args[0]->IsString());  // name
  CHECK(args[1]->IsUint32());  // flags

  Utf8Value name(env->isolate(), args[0]);
  uint32_t flags = args[1].As<Uint32>()->Value();
  ncrypto::DataPointer peername;

  switch (cert->view().checkHost(name.ToStringView(), flags, &peername)) {
    case ncrypto::X509View::CheckMatch::MATCH: {  // Match!
      Local<Value> ret = args[0];
      if (peername) {
        ret = OneByteString(env->isolate(),
                            static_cast<const char*>(peername.get()),
                            peername.size());
      }
      return args.GetReturnValue().Set(ret);
    }
    case ncrypto::X509View::CheckMatch::NO_MATCH:  // No Match!
      return;  // No return value is set
    case ncrypto::X509View::CheckMatch::INVALID_NAME:  // Error!
      return THROW_ERR_INVALID_ARG_VALUE(env, "Invalid name");
    default:  // Error!
      return THROW_ERR_CRYPTO_OPERATION_FAILED(env);
  }
}

void CheckEmail(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  X509Certificate* cert;
  ASSIGN_OR_RETURN_UNWRAP(&cert, args.This());

  CHECK(args[0]->IsString());  // name
  CHECK(args[1]->IsUint32());  // flags

  Utf8Value name(env->isolate(), args[0]);
  uint32_t flags = args[1].As<Uint32>()->Value();

  switch (cert->view().checkEmail(name.ToStringView(), flags)) {
    case ncrypto::X509View::CheckMatch::MATCH:  // Match!
      return args.GetReturnValue().Set(args[0]);
    case ncrypto::X509View::CheckMatch::NO_MATCH:  // No Match!
      return;  // No return value is set
    case ncrypto::X509View::CheckMatch::INVALID_NAME:  // Error!
      return THROW_ERR_INVALID_ARG_VALUE(env, "Invalid name");
    default:  // Error!
      return THROW_ERR_CRYPTO_OPERATION_FAILED(env);
  }
}

void CheckIP(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  X509Certificate* cert;
  ASSIGN_OR_RETURN_UNWRAP(&cert, args.This());

  CHECK(args[0]->IsString());  // IP
  CHECK(args[1]->IsUint32());  // flags

  Utf8Value name(env->isolate(), args[0]);
  uint32_t flags = args[1].As<Uint32>()->Value();

  switch (cert->view().checkIp(name.ToStringView(), flags)) {
    case ncrypto::X509View::CheckMatch::MATCH:  // Match!
      return args.GetReturnValue().Set(args[0]);
    case ncrypto::X509View::CheckMatch::NO_MATCH:  // No Match!
      return;  // No return value is set
    case ncrypto::X509View::CheckMatch::INVALID_NAME:  // Error!
      return THROW_ERR_INVALID_ARG_VALUE(env, "Invalid IP");
    default:  // Error!
      return THROW_ERR_CRYPTO_OPERATION_FAILED(env);
  }
}

void GetIssuerCert(const FunctionCallbackInfo<Value>& args) {
  X509Certificate* cert;
  ASSIGN_OR_RETURN_UNWRAP(&cert, args.This());
  auto issuer = cert->getIssuerCert();
  if (issuer) args.GetReturnValue().Set(issuer->object());
}

void Parse(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK(args[0]->IsArrayBufferView());
  ArrayBufferViewContents<unsigned char> buf(args[0].As<ArrayBufferView>());
  Local<Object> cert;

  auto result = X509Pointer::Parse(ncrypto::Buffer<const unsigned char>{
      .data = buf.data(),
      .len = buf.length(),
  });

  if (!result.value) return ThrowCryptoError(env, result.error.value_or(0));

  if (X509Certificate::New(env, std::move(result.value)).ToLocal(&cert)) {
    args.GetReturnValue().Set(cert);
  }
}

void ToLegacy(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  X509Certificate* cert;
  ASSIGN_OR_RETURN_UNWRAP(&cert, args.This());
  ClearErrorOnReturn clear_error_on_return;
  Local<Value> ret;
  if (X509ToObject(env, cert->get()).ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
}
}  // namespace

Local<FunctionTemplate> X509Certificate::GetConstructorTemplate(
    Environment* env) {
  Local<FunctionTemplate> tmpl = env->x509_constructor_template();
  if (tmpl.IsEmpty()) {
    Isolate* isolate = env->isolate();
    tmpl = NewFunctionTemplate(isolate, nullptr);
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        BaseObject::kInternalFieldCount);
    tmpl->SetClassName(
        FIXED_ONE_BYTE_STRING(env->isolate(), "X509Certificate"));
    SetProtoMethod(isolate, tmpl, "subject", Subject);
    SetProtoMethod(isolate, tmpl, "subjectAltName", SubjectAltName);
    SetProtoMethod(isolate, tmpl, "infoAccess", InfoAccess);
    SetProtoMethod(isolate, tmpl, "issuer", Issuer);
    SetProtoMethod(isolate, tmpl, "validTo", ValidTo);
    SetProtoMethod(isolate, tmpl, "validFrom", ValidFrom);
    SetProtoMethod(isolate, tmpl, "fingerprint", Fingerprint<EVP_sha1>);
    SetProtoMethod(isolate, tmpl, "fingerprint256", Fingerprint<EVP_sha256>);
    SetProtoMethod(isolate, tmpl, "fingerprint512", Fingerprint<EVP_sha512>);
    SetProtoMethod(isolate, tmpl, "keyUsage", KeyUsage);
    SetProtoMethod(isolate, tmpl, "serialNumber", SerialNumber);
    SetProtoMethod(isolate, tmpl, "pem", Pem);
    SetProtoMethod(isolate, tmpl, "raw", Der);
    SetProtoMethod(isolate, tmpl, "publicKey", PublicKey);
    SetProtoMethod(isolate, tmpl, "checkCA", CheckCA);
    SetProtoMethod(isolate, tmpl, "checkHost", CheckHost);
    SetProtoMethod(isolate, tmpl, "checkEmail", CheckEmail);
    SetProtoMethod(isolate, tmpl, "checkIP", CheckIP);
    SetProtoMethod(isolate, tmpl, "checkIssued", CheckIssued);
    SetProtoMethod(isolate, tmpl, "checkPrivateKey", CheckPrivateKey);
    SetProtoMethod(isolate, tmpl, "verify", CheckPublicKey);
    SetProtoMethod(isolate, tmpl, "toLegacy", ToLegacy);
    SetProtoMethod(isolate, tmpl, "getIssuerCert", GetIssuerCert);
    env->set_x509_constructor_template(tmpl);
  }
  return tmpl;
}

bool X509Certificate::HasInstance(Environment* env, Local<Object> object) {
  return GetConstructorTemplate(env)->HasInstance(object);
}

MaybeLocal<Object> X509Certificate::New(Environment* env,
                                        X509Pointer cert,
                                        STACK_OF(X509) * issuer_chain) {
  std::shared_ptr<ManagedX509> mcert(new ManagedX509(std::move(cert)));
  return New(env, std::move(mcert), issuer_chain);
}

MaybeLocal<Object> X509Certificate::New(Environment* env,
                                        std::shared_ptr<ManagedX509> cert,
                                        STACK_OF(X509) * issuer_chain) {
  EscapableHandleScope scope(env->isolate());
  Local<Function> ctor;
  if (!GetConstructorTemplate(env)->GetFunction(env->context()).ToLocal(&ctor))
    return MaybeLocal<Object>();

  Local<Object> obj;
  if (!ctor->NewInstance(env->context()).ToLocal(&obj))
    return MaybeLocal<Object>();

  new X509Certificate(env, obj, std::move(cert), issuer_chain);
  return scope.Escape(obj);
}

MaybeLocal<Object> X509Certificate::GetCert(Environment* env,
                                            const SSLPointer& ssl) {
  ClearErrorOnReturn clear_error_on_return;
  X509* cert = SSL_get_certificate(ssl.get());
  if (cert == nullptr) return MaybeLocal<Object>();

  X509Pointer ptr(X509_dup(cert));
  return New(env, std::move(ptr));
}

MaybeLocal<Object> X509Certificate::GetPeerCert(Environment* env,
                                                const SSLPointer& ssl,
                                                GetPeerCertificateFlag flag) {
  ClearErrorOnReturn clear_error_on_return;
  MaybeLocal<Object> maybe_cert;

  bool is_server =
      static_cast<int>(flag) & static_cast<int>(GetPeerCertificateFlag::SERVER);

  X509Pointer cert(is_server ? SSL_get_peer_certificate(ssl.get()) : nullptr);
  STACK_OF(X509)* ssl_certs = SSL_get_peer_cert_chain(ssl.get());
  if (!cert && (ssl_certs == nullptr || sk_X509_num(ssl_certs) == 0))
    return MaybeLocal<Object>();

  std::vector<Local<Value>> certs;

  if (!cert) {
    cert.reset(sk_X509_value(ssl_certs, 0));
    sk_X509_delete(ssl_certs, 0);
  }

  return sk_X509_num(ssl_certs) ? New(env, std::move(cert), ssl_certs)
                                : New(env, std::move(cert));
}

template <MaybeLocal<Value> Property(Environment* env, X509* cert)>
static void ReturnProperty(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  X509Certificate* cert;
  ASSIGN_OR_RETURN_UNWRAP(&cert, args.This());
  Local<Value> ret;
  if (Property(env, cert->get()).ToLocal(&ret)) args.GetReturnValue().Set(ret);
}

X509Certificate::X509Certificate(
    Environment* env,
    Local<Object> object,
    std::shared_ptr<ManagedX509> cert,
    STACK_OF(X509)* issuer_chain)
    : BaseObject(env, object),
      cert_(std::move(cert)) {
  MakeWeak();

  if (issuer_chain != nullptr && sk_X509_num(issuer_chain)) {
    X509Pointer cert(X509_dup(sk_X509_value(issuer_chain, 0)));
    sk_X509_delete(issuer_chain, 0);
    Local<Object> obj = sk_X509_num(issuer_chain)
        ? X509Certificate::New(env, std::move(cert), issuer_chain)
            .ToLocalChecked()
        : X509Certificate::New(env, std::move(cert))
            .ToLocalChecked();
    issuer_cert_.reset(Unwrap<X509Certificate>(obj));
  }
}

void X509Certificate::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("cert", cert_);
}

BaseObjectPtr<BaseObject>
X509Certificate::X509CertificateTransferData::Deserialize(
    Environment* env,
    Local<Context> context,
    std::unique_ptr<worker::TransferData> self) {
  if (context != env->context()) {
    THROW_ERR_MESSAGE_TARGET_CONTEXT_UNAVAILABLE(env);
    return {};
  }

  Local<Value> handle;
  if (!X509Certificate::New(env, data_).ToLocal(&handle))
    return {};

  return BaseObjectPtr<BaseObject>(
      Unwrap<X509Certificate>(handle.As<Object>()));
}

BaseObject::TransferMode X509Certificate::GetTransferMode() const {
  return BaseObject::TransferMode::kCloneable;
}

std::unique_ptr<worker::TransferData> X509Certificate::CloneForMessaging()
    const {
  return std::make_unique<X509CertificateTransferData>(cert_);
}


void X509Certificate::Initialize(Environment* env, Local<Object> target) {
  SetMethod(env->context(), target, "parseX509", Parse);

  NODE_DEFINE_CONSTANT(target, X509_CHECK_FLAG_ALWAYS_CHECK_SUBJECT);
  NODE_DEFINE_CONSTANT(target, X509_CHECK_FLAG_NEVER_CHECK_SUBJECT);
  NODE_DEFINE_CONSTANT(target, X509_CHECK_FLAG_NO_WILDCARDS);
  NODE_DEFINE_CONSTANT(target, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
  NODE_DEFINE_CONSTANT(target, X509_CHECK_FLAG_MULTI_LABEL_WILDCARDS);
  NODE_DEFINE_CONSTANT(target, X509_CHECK_FLAG_SINGLE_LABEL_SUBDOMAINS);
}

void X509Certificate::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(Parse);
  registry->Register(Subject);
  registry->Register(SubjectAltName);
  registry->Register(InfoAccess);
  registry->Register(Issuer);
  registry->Register(ValidTo);
  registry->Register(ValidFrom);
  registry->Register(Fingerprint<EVP_sha1>);
  registry->Register(Fingerprint<EVP_sha256>);
  registry->Register(Fingerprint<EVP_sha512>);
  registry->Register(KeyUsage);
  registry->Register(SerialNumber);
  registry->Register(Pem);
  registry->Register(Der);
  registry->Register(PublicKey);
  registry->Register(CheckCA);
  registry->Register(CheckHost);
  registry->Register(CheckEmail);
  registry->Register(CheckIP);
  registry->Register(CheckIssued);
  registry->Register(CheckPrivateKey);
  registry->Register(CheckPublicKey);
  registry->Register(ToLegacy);
  registry->Register(GetIssuerCert);
}
}  // namespace crypto
}  // namespace node
