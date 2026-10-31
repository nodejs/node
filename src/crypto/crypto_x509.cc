#include "crypto/crypto_x509.h"
#include "base_object-inl.h"
#include "crypto/crypto_common.h"
#include "crypto/crypto_keys.h"
#include "crypto/crypto_util.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "ncrypto.h"
#include "node_errors.h"
#include "util-inl.h"
#include "uv.h"
#include "v8.h"

#include <array>
#include <limits>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

namespace node {

using ncrypto::BignumPointer;
using ncrypto::BIOPointer;
using ncrypto::ClearErrorOnReturn;
using ncrypto::DataPointer;
using ncrypto::Digest;
using ncrypto::MarkPopErrorOnReturn;
using ncrypto::SSLPointer;
using ncrypto::X509Name;
using ncrypto::X509NameInspection;
using ncrypto::X509Pointer;
using ncrypto::X509View;
using v8::Array;
using v8::ArrayBuffer;
using v8::ArrayBufferView;
using v8::Boolean;
using v8::Context;
using v8::Date;
using v8::DictionaryTemplate;
using v8::EscapableHandleScope;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::LocalVector;
using v8::MaybeLocal;
using v8::NewStringType;
using v8::Null;
using v8::Object;
using v8::String;
using v8::Uint32;
using v8::Undefined;
using v8::Value;

namespace crypto {

namespace {

std::optional<X509CertificateSnapshot::DER> EncodeCertificate(
    const X509View& cert) {
  MarkPopErrorOnReturn mark_pop_error_on_return;
  if (!cert) return std::nullopt;

  const int size = i2d_X509(cert.get(), nullptr);
  if (size <= 0) return std::nullopt;

  X509CertificateSnapshot::DER der(static_cast<size_t>(size));
  unsigned char* cursor = der.data();
  if (i2d_X509(cert.get(), &cursor) != size ||
      cursor != der.data() + der.size()) {
    return std::nullopt;
  }
  return der;
}

X509Pointer ParseCertificate(const X509CertificateSnapshot::DER& certificate) {
  MarkPopErrorOnReturn mark_pop_error_on_return;
  constexpr size_t kMaximumCertificateSize = static_cast<size_t>(
      std::numeric_limits<long>::max());  // NOLINT(runtime/int)
  if (certificate.empty() || certificate.size() > kMaximumCertificateSize) {
    return {};
  }

  const unsigned char* cursor = certificate.data();
  const unsigned char* const end = cursor + certificate.size();
  X509Pointer cert(
      d2i_X509(nullptr,
               &cursor,
               static_cast<long>(certificate.size())));  // NOLINT(runtime/int)
  if (!cert || cursor != end) return {};
  return cert;
}

}  // namespace

ManagedX509::ManagedX509(X509Pointer cert) : cert_(std::move(cert)) {}

std::shared_ptr<ManagedX509> ManagedX509::Adopt(X509Pointer cert) {
  if (!cert) return {};
  return std::shared_ptr<ManagedX509>(new ManagedX509(std::move(cert)));
}

X509CertificateSnapshot::X509CertificateSnapshot(std::vector<DER> certificates)
    : certificates_(std::move(certificates)) {}

std::shared_ptr<const X509CertificateSnapshot> X509CertificateSnapshot::Create(
    X509View leaf,
    const STACK_OF(X509) * issuer_chain,
    OPENSSL_SIZE_T issuers_start) {
  auto leaf_der = EncodeCertificate(leaf);
  if (!leaf_der.has_value()) return {};

  const OPENSSL_SIZE_T count =
      issuer_chain == nullptr ? 0 : sk_X509_num(issuer_chain);
  std::vector<DER> certificates;
  certificates.reserve(static_cast<size_t>(count) + 1);
  certificates.push_back(std::move(leaf_der.value()));
  for (OPENSSL_SIZE_T i = issuers_start; i < count; i++) {
    const X509View candidate(sk_X509_value(issuer_chain, i));
    auto candidate_der = EncodeCertificate(candidate);
    if (!candidate_der.has_value()) return {};
    certificates.push_back(std::move(candidate_der.value()));
  }

  return std::shared_ptr<const X509CertificateSnapshot>(
      new X509CertificateSnapshot(std::move(certificates)));
}

X509Pointer X509CertificateSnapshot::Parse(size_t index) const {
  if (index >= certificates_.size()) return {};
  return ParseCertificate(certificates_[index]);
}

size_t X509CertificateSnapshot::size_bytes() const {
  return std::accumulate(certificates_.begin(),
                         certificates_.end(),
                         size_t{0},
                         [](size_t total, const DER& certificate) {
                           return total + certificate.size();
                         });
}

void X509CertificateSnapshot::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackFieldWithSize("certificates", size_bytes());
}

void ManagedX509::MemoryInfo(MemoryTracker* tracker) const {
  if (!cert_) return;
  MarkPopErrorOnReturn mark_pop_error_on_return;
  // This is an approximation based on the der encoding size.
  const int size = i2d_X509(cert_.get(), nullptr);
  if (size > 0) tracker->TrackFieldWithSize("cert", size);
}

namespace {
enum class MalformedSubjectAltNameMode {
  kThrow,
  kReturnLegacySentinel,
};

constexpr std::string_view kMalformedSubjectAltName = "DNS:";

MaybeLocal<Value> GetFingerprintDigest(Environment* env,
                                       const Digest& method,
                                       const X509View& cert) {
  auto fingerprint = cert.getFingerprint(method);
  // Returning an empty string indicates that the digest failed for
  // some reason.
  if (!fingerprint.has_value()) [[unlikely]] {
    return Undefined(env->isolate());
  }
  auto& fp = fingerprint.value();
  return OneByteString(env->isolate(), fp.data(), fp.length());
}

template <const ncrypto::Digest& algo>
void Fingerprint(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  X509Certificate* cert;
  ASSIGN_OR_RETURN_UNWRAP(&cert, args.This());
  Local<Value> ret;
  if (GetFingerprintDigest(env, algo, cert->view()).ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
}

MaybeLocal<String> ToV8Value(Environment* env, std::string_view val) {
  return String::NewFromUtf8(
      env->isolate(), val.data(), NewStringType::kNormal, val.size());
}

MaybeLocal<Value> ToV8Value(Local<Context> context, BIOPointer&& bio) {
  if (!bio) [[unlikely]]
    return {};
  BUF_MEM* mem = bio;
  if (!mem) [[unlikely]]
    return {};
  Local<Value> ret;
  if (!String::NewFromUtf8(Isolate::GetCurrent(),
                           mem->data,
                           NewStringType::kNormal,
                           mem->length)
           .ToLocal(&ret))
    return {};
  return ret;
}

MaybeLocal<Value> ToV8Value(Local<Context> context, const BIOPointer& bio) {
  if (!bio) [[unlikely]]
    return {};
  BUF_MEM* mem = bio;
  if (!mem) [[unlikely]]
    return {};
  Local<Value> ret;
  if (!String::NewFromUtf8(Isolate::GetCurrent(),
                           mem->data,
                           NewStringType::kNormal,
                           mem->length)
           .ToLocal(&ret))
    return {};
  return ret;
}

MaybeLocal<Value> ToBuffer(Environment* env, BIOPointer* bio) {
  if (bio == nullptr || !*bio) [[unlikely]]
    return {};
  BUF_MEM* mem = *bio;
  if (!mem) [[unlikely]]
    return {};
  auto backing = AdoptIntoBackingStore(
      env->isolate(),
      mem->data,
      mem->length,
      [](void*, size_t, void* data) {
        BIOPointer free_me(static_cast<BIO*>(data));
      },
      bio->release());
  if (!backing) {
    THROW_ERR_MEMORY_ALLOCATION_FAILED(env);
    return MaybeLocal<Value>();
  }
  auto ab = ArrayBuffer::New(env->isolate(), std::move(backing));
  Local<Value> ret;
  if (!Buffer::New(env, ab, 0, ab->ByteLength()).ToLocal(&ret)) return {};
  return ret;
}

MaybeLocal<Value> GetDer(Environment* env, const X509View& view) {
  Local<Value> ret;
  auto bio = view.toDER();
  if (!bio) [[unlikely]]
    return Undefined(env->isolate());
  if (!ToBuffer(env, &bio).ToLocal(&ret)) {
    return {};
  }
  return ret;
}

MaybeLocal<Value> GetSubjectAltNameString(
    Environment* env,
    const X509View& view,
    MalformedSubjectAltNameMode malformed_name_mode) {
  Local<Value> ret;
  bool has_extension = false;
  auto bio = view.getSubjectAltName(&has_extension);
  if (!bio) [[unlikely]] {
    // An absent SAN maps to undefined. A malformed, present SAN throws for
    // direct X509Certificate inspection, while the legacy TLS object uses a
    // deliberately nonmatching value to prevent commonName fallback.
    if (!has_extension) return Undefined(env->isolate());
    if (malformed_name_mode ==
        MalformedSubjectAltNameMode::kReturnLegacySentinel) {
      // The legacy object is used by TLS hostname verification. Return an
      // empty DNS name so verification cannot fall back to the common name,
      // while keeping getPeerCertificate() usable for malformed certificates.
      return OneByteString(env->isolate(), kMalformedSubjectAltName);
    }
    THROW_ERR_CRYPTO_OPERATION_FAILED(
        env, "Failed to format subject alternative name");
    return {};
  }
  if (!ToV8Value(env->context(), bio).ToLocal(&ret)) return {};
  return ret;
}

MaybeLocal<Value> GetInfoAccessString(Environment* env, const X509View& view) {
  Local<Value> ret;
  auto bio = view.getInfoAccess();
  if (!bio) [[unlikely]]
    return Undefined(env->isolate());
  if (!ToV8Value(env->context(), bio).ToLocal(&ret)) {
    return {};
  }
  return ret;
}

MaybeLocal<Value> GetValidFrom(Environment* env, const X509View& view) {
  Local<Value> ret;
  auto bio = view.getValidFrom();
  if (!bio) [[unlikely]]
    return Undefined(env->isolate());
  if (!ToV8Value(env->context(), bio).ToLocal(&ret)) {
    return {};
  }
  return ret;
}

MaybeLocal<Value> GetValidTo(Environment* env, const X509View& view) {
  Local<Value> ret;
  auto bio = view.getValidTo();
  if (!bio) [[unlikely]]
    return Undefined(env->isolate());
  if (!ToV8Value(env->context(), bio).ToLocal(&ret)) {
    return {};
  }
  return ret;
}

MaybeLocal<Value> GetValidFromDate(Environment* env, const X509View& view) {
  const auto valid_from_time = view.getValidFromTime();
  const double milliseconds = valid_from_time.has_value()
                                  ? valid_from_time.value() * 1000.
                                  : std::numeric_limits<double>::quiet_NaN();
  return Date::New(env->context(), milliseconds);
}

MaybeLocal<Value> GetValidToDate(Environment* env, const X509View& view) {
  const auto valid_to_time = view.getValidToTime();
  const double milliseconds = valid_to_time.has_value()
                                  ? valid_to_time.value() * 1000.
                                  : std::numeric_limits<double>::quiet_NaN();
  return Date::New(env->context(), milliseconds);
}

MaybeLocal<Value> GetSignatureAlgorithm(Environment* env,
                                        const X509View& view) {
  auto algo = view.getSignatureAlgorithm();
  if (!algo.has_value()) [[unlikely]]
    return Undefined(env->isolate());
  Local<Value> ret;
  if (!ToV8Value(env, algo.value()).ToLocal(&ret)) {
    return {};
  }
  return ret;
}

MaybeLocal<Value> GetSignatureAlgorithmOID(Environment* env,
                                           const X509View& view) {
  auto oid = view.getSignatureAlgorithmOID();
  if (!oid.has_value()) [[unlikely]]
    return Undefined(env->isolate());
  Local<Value> ret;
  if (!ToV8Value(env, oid.value()).ToLocal(&ret)) {
    return {};
  }
  return ret;
}

MaybeLocal<Value> GetSerialNumber(Environment* env, const X509View& view) {
  if (auto serial = view.getSerialNumber()) {
    return ToV8Value(
        env, ToUpper(std::string_view(static_cast<char*>(serial.get()))));
  }
  return Undefined(env->isolate());
}

MaybeLocal<Value> GetKeyUsage(Environment* env, const X509View& cert) {
  LocalVector<Value> vec(env->isolate());
  bool res = cert.enumUsages([&](std::string_view view) {
    vec.push_back(OneByteString(env->isolate(), view));
  });
  if (!res) return Undefined(env->isolate());
  return Array::New(env->isolate(), vec.data(), vec.size());
}

bool SetValue(Local<Context> context,
              Local<Object> object,
              Local<String> name,
              Local<Value> value) {
  return object->CreateDataProperty(context, name, value).FromMaybe(false);
}

bool SetArrayValue(Local<Context> context,
                   Local<Array> array,
                   uint32_t index,
                   Local<Value> value) {
  return array->CreateDataProperty(context, index, value).FromMaybe(false);
}

MaybeLocal<Value> CopyBuffer(Environment* env,
                             ncrypto::Buffer<const unsigned char> value) {
  const char* data =
      value.len == 0 ? "" : reinterpret_cast<const char*>(value.data);
  return Buffer::Copy(env, data, value.len);
}

MaybeLocal<Value> CopyBuffer(Environment* env,
                             const std::vector<unsigned char>& value) {
  return CopyBuffer(env, {.data = value.data(), .len = value.size()});
}

std::optional<std::string> OidToText(const ASN1_OBJECT* object) {
  if (object == nullptr) return std::nullopt;
  const int length = OBJ_obj2txt(nullptr, 0, object, 1);
  if (length <= 0 || length == std::numeric_limits<int>::max()) {
    return std::nullopt;
  }
  std::string output(static_cast<size_t>(length) + 1, '\0');
  if (OBJ_obj2txt(output.data(), length + 1, object, 1) != length) {
    return std::nullopt;
  }
  output.resize(static_cast<size_t>(length));
  return output;
}

struct SequenceSpan final {
  ncrypto::Buffer<const unsigned char> encoded;
  ncrypto::Buffer<const unsigned char> content;
};

bool ReadSequence(ncrypto::Buffer<const unsigned char> input,
                  SequenceSpan* out) {
  if (out == nullptr || input.data == nullptr || input.len == 0 ||
      input.len > static_cast<size_t>(LONG_MAX)) {
    return false;
  }

  const unsigned char* const start = input.data;
  const unsigned char* content = start;
  long content_length = 0;  // NOLINT(runtime/int)
  int tag = 0;
  int tag_class = 0;
  const int flags =
      ASN1_get_object(&content,
                      &content_length,
                      &tag,
                      &tag_class,
                      static_cast<long>(input.len));  // NOLINT(runtime/int)
  constexpr int kIndefiniteLength = 0x01;
  constexpr int kDecodeError = 0x80;
  if ((flags & (kIndefiniteLength | kDecodeError)) != 0 ||
      (flags & V_ASN1_CONSTRUCTED) == 0 || tag != V_ASN1_SEQUENCE ||
      tag_class != V_ASN1_UNIVERSAL || content_length < 0) {
    return false;
  }

  const size_t header_length = static_cast<size_t>(content - start);
  const size_t length = static_cast<size_t>(content_length);
  if (header_length > input.len || length > input.len - header_length) {
    return false;
  }
  *out = {
      .encoded = {.data = start, .len = header_length + length},
      .content = {.data = content, .len = length},
  };
  return true;
}

std::optional<ncrypto::Buffer<const unsigned char>> GetExactTbs(
    const X509CertificateSnapshot::DER& certificate) {
  SequenceSpan outer;
  const ncrypto::Buffer<const unsigned char> encoded{
      .data = certificate.data(),
      .len = certificate.size(),
  };
  if (!ReadSequence(encoded, &outer) || outer.encoded.len != encoded.len) {
    return std::nullopt;
  }

  SequenceSpan tbs;
  if (!ReadSequence(outer.content, &tbs)) return std::nullopt;
  return tbs.encoded;
}

bool GetNameDer(const X509_NAME* name,
                ncrypto::Buffer<const unsigned char>* encoded) {
  if (name == nullptr || encoded == nullptr) return false;
  const unsigned char* data = nullptr;
  size_t length = 0;
  if (X509_NAME_get0_der(name, &data, &length) != 1 ||
      (data == nullptr && length != 0)) {
    return false;
  }
  *encoded = {.data = data, .len = length};
  return true;
}

MaybeLocal<Value> CreateName(Environment* env,
                             const X509NameInspection& native,
                             const X509_NAME* name) {
  Local<Context> context = env->context();
  size_t rdn_count = 0;
  for (const auto& attribute : native) {
    if (attribute.set < 0) return {};
    const size_t set = static_cast<size_t>(attribute.set);
    if (set == rdn_count) {
      rdn_count++;
    } else if (rdn_count == 0 || set != rdn_count - 1) {
      return {};
    }
  }

  if (rdn_count > static_cast<size_t>(INT_MAX)) return {};
  Local<Array> rdns = Array::New(env->isolate(), static_cast<int>(rdn_count));
  LocalVector<Array> groups(env->isolate());
  groups.reserve(rdn_count);
  std::vector<uint32_t> group_lengths(rdn_count, 0);
  for (size_t i = 0; i < rdn_count; i++) {
    Local<Array> group = Array::New(env->isolate());
    if (!SetArrayValue(context, rdns, i, group)) return {};
    groups.push_back(group);
  }

  for (const auto& attribute : native) {
    Local<Object> output = Object::New(env->isolate());
    if (!SetValue(context,
                  output,
                  env->oid_string(),
                  OneByteString(env->isolate(), attribute.oid))) {
      return {};
    }
    if (attribute.value.has_value()) {
      Local<Value> value;
      if (!ToV8Value(env, attribute.value.value()).ToLocal(&value) ||
          !SetValue(context, output, env->value_string(), value)) {
        return {};
      }
    }
    const size_t rdn_index = static_cast<size_t>(attribute.set);
    if (!SetArrayValue(
            context, groups[rdn_index], group_lengths[rdn_index]++, output)) {
      return {};
    }
  }

  ncrypto::Buffer<const unsigned char> encoded;
  Local<Value> raw;
  Local<Object> output = Object::New(env->isolate());
  if (!GetNameDer(name, &encoded) || !CopyBuffer(env, encoded).ToLocal(&raw) ||
      !SetValue(context, output, env->raw_string(), raw) ||
      !SetValue(context, output, env->rdns_string(), rdns)) {
    return {};
  }
  return output;
}

MaybeLocal<Value> CreateRawExtensions(
    Environment* env,
    const std::vector<ncrypto::X509RawExtension>& extensions) {
  Local<Context> context = env->context();
  if (extensions.size() > static_cast<size_t>(INT_MAX)) return {};
  Local<Array> output =
      Array::New(env->isolate(), static_cast<int>(extensions.size()));
  for (size_t i = 0; i < extensions.size(); i++) {
    const auto& extension = extensions[i];
    Local<Object> value = Object::New(env->isolate());
    Local<Value> bytes;
    if (!CopyBuffer(env, extension.value).ToLocal(&bytes) ||
        !SetValue(context,
                  value,
                  env->oid_string(),
                  OneByteString(env->isolate(), extension.oid)) ||
        !SetValue(context,
                  value,
                  env->critical_string(),
                  Boolean::New(env->isolate(), extension.critical)) ||
        !SetValue(context, value, env->value_string(), bytes) ||
        !SetArrayValue(context, output, i, value)) {
      return {};
    }
  }
  return output;
}

MaybeLocal<Value> CreateAlgorithmIdentifier(Environment* env,
                                            const X509_ALGOR* native) {
  if (native == nullptr) return {};
  const ASN1_OBJECT* native_oid = nullptr;
  int parameter_type = V_ASN1_UNDEF;
  const void* parameter_value = nullptr;
  X509_ALGOR_get0(&native_oid, &parameter_type, &parameter_value, native);
  auto oid = OidToText(native_oid);
  if (!oid.has_value()) return {};

  Local<Context> context = env->context();
  Local<Object> output = Object::New(env->isolate());
  if (!SetValue(context,
                output,
                env->oid_string(),
                OneByteString(env->isolate(), oid.value()))) {
    return {};
  }
  if (parameter_type != V_ASN1_UNDEF) {
    ncrypto::DeleteFnPtr<ASN1_TYPE, ASN1_TYPE_free> native_parameters(
        ASN1_TYPE_new());
    if (!native_parameters ||
        ASN1_TYPE_set1(
            native_parameters.get(), parameter_type, parameter_value) != 1) {
      return {};
    }
    const int length = i2d_ASN1_TYPE(native_parameters.get(), nullptr);
    if (length <= 0) return {};
    std::vector<unsigned char> encoded(static_cast<size_t>(length));
    unsigned char* cursor = encoded.data();
    if (i2d_ASN1_TYPE(native_parameters.get(), &cursor) != length ||
        cursor != encoded.data() + encoded.size()) {
      return {};
    }

    Local<Value> parameters;
    if (!CopyBuffer(env, encoded).ToLocal(&parameters) ||
        !SetValue(context, output, env->parameters_string(), parameters)) {
      return {};
    }
  }
  return output;
}

MaybeLocal<Value> CreateSignature(Environment* env,
                                  const X509View& certificate) {
  const ASN1_BIT_STRING* native_signature = nullptr;
  const X509_ALGOR* native_algorithm = nullptr;
  X509_get0_signature(&native_signature, &native_algorithm, certificate.get());
  if (native_signature == nullptr || native_algorithm == nullptr ||
      ASN1_STRING_type(native_signature) != V_ASN1_BIT_STRING) {
    return {};
  }

  const int native_length = ASN1_STRING_length(native_signature);
  const unsigned char* native_data = ASN1_STRING_get0_data(native_signature);
  if (native_length < 0 || (native_length > 0 && native_data == nullptr)) {
    return {};
  }
#if NCRYPTO_HAS_ASN1_BIT_STRING_GET_LENGTH
  size_t byte_length = 0;
  int unused_bits = 0;
  if (ASN1_BIT_STRING_get_length(
          native_signature, &byte_length, &unused_bits) != 1 ||
      unused_bits != 0 || byte_length != static_cast<size_t>(native_length)) {
    return {};
  }
#elif NCRYPTO_USE_OPENSSL_PROVIDER || NCRYPTO_USE_BORINGSSL
  if ((native_signature->flags & ASN1_STRING_FLAG_BITS_LEFT) != 0 &&
      (native_signature->flags & 0x07) != 0) {
    return {};
  }
#else
#error "Unsupported crypto backend"
#endif

  const ncrypto::Buffer<const unsigned char> signature{
      .data = native_data,
      .len = static_cast<size_t>(native_length),
  };

  Local<Value> algorithm;
  Local<Value> value;
  if (!CreateAlgorithmIdentifier(env, native_algorithm).ToLocal(&algorithm) ||
      !CopyBuffer(env, signature).ToLocal(&value)) {
    return {};
  }

  Local<Context> context = env->context();
  Local<Object> output = Object::New(env->isolate());
  if (!SetValue(context, output, env->algorithm_string(), algorithm) ||
      !SetValue(context, output, env->value_string(), value)) {
    return {};
  }
  return output;
}

void Version(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  X509Certificate* certificate;
  ASSIGN_OR_RETURN_UNWRAP(&certificate, args.This());
  const auto version = certificate->view().getVersion();
  if (!version.has_value()) return;
  args.GetReturnValue().Set(
      Integer::New(env->isolate(), static_cast<int32_t>(version.value())));
}

void StructuredName(const FunctionCallbackInfo<Value>& args, bool subject) {
  Environment* env = Environment::GetCurrent(args);
  X509Certificate* certificate;
  ASSIGN_OR_RETURN_UNWRAP(&certificate, args.This());

  const auto native = subject ? certificate->view().inspectSubjectName()
                              : certificate->view().inspectIssuerName();
  if (!native.has_value()) return;

  MarkPopErrorOnReturn mark_pop_error_on_return;
  const X509_NAME* name = subject ? X509_get_subject_name(certificate->get())
                                  : X509_get_issuer_name(certificate->get());
  Local<Value> output;
  if (!CreateName(env, native.value(), name).ToLocal(&output)) {
    return;
  }
  args.GetReturnValue().Set(output);
}

void SubjectName(const FunctionCallbackInfo<Value>& args) {
  StructuredName(args, true);
}

void IssuerName(const FunctionCallbackInfo<Value>& args) {
  StructuredName(args, false);
}

void Extensions(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  X509Certificate* certificate;
  ASSIGN_OR_RETURN_UNWRAP(&certificate, args.This());

  const auto extensions = certificate->view().inspectExtensions();
  Local<Value> output;
  if (!extensions.has_value() ||
      !CreateRawExtensions(env, extensions.value()).ToLocal(&output)) {
    return;
  }
  args.GetReturnValue().Set(output);
}

void Tbs(const FunctionCallbackInfo<Value>& args) {
  MarkPopErrorOnReturn mark_pop_error_on_return;
  Environment* env = Environment::GetCurrent(args);
  X509Certificate* certificate;
  ASSIGN_OR_RETURN_UNWRAP(&certificate, args.This());

  const auto tbs = GetExactTbs(certificate->snapshot_der());
  Local<Value> output;
  if (!tbs.has_value() || !CopyBuffer(env, tbs.value()).ToLocal(&output)) {
    return;
  }
  args.GetReturnValue().Set(output);
}

void Signature(const FunctionCallbackInfo<Value>& args) {
  MarkPopErrorOnReturn mark_pop_error_on_return;
  Environment* env = Environment::GetCurrent(args);
  X509Certificate* certificate;
  ASSIGN_OR_RETURN_UNWRAP(&certificate, args.This());

  Local<Value> output;
  if (!CreateSignature(env, certificate->view()).ToLocal(&output)) {
    return;
  }
  args.GetReturnValue().Set(output);
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
  if (GetDer(env, cert->view()).ToLocal(&ret)) {
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
  if (GetSubjectAltNameString(
          env, cert->view(), MalformedSubjectAltNameMode::kThrow)
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
  if (GetInfoAccessString(env, cert->view()).ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
}

void ValidFrom(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  X509Certificate* cert;
  ASSIGN_OR_RETURN_UNWRAP(&cert, args.This());
  Local<Value> ret;
  if (GetValidFrom(env, cert->view()).ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
}

void ValidTo(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  X509Certificate* cert;
  ASSIGN_OR_RETURN_UNWRAP(&cert, args.This());
  Local<Value> ret;
  if (GetValidTo(env, cert->view()).ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
}

void ValidFromDate(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  X509Certificate* cert;
  ASSIGN_OR_RETURN_UNWRAP(&cert, args.This());
  Local<Value> ret;
  if (GetValidFromDate(env, cert->view()).ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
}

void ValidToDate(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  X509Certificate* cert;
  ASSIGN_OR_RETURN_UNWRAP(&cert, args.This());
  Local<Value> ret;
  if (GetValidToDate(env, cert->view()).ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
}

void SignatureAlgorithm(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  X509Certificate* cert;
  ASSIGN_OR_RETURN_UNWRAP(&cert, args.This());
  Local<Value> ret;
  if (GetSignatureAlgorithm(env, cert->view()).ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
}

void SignatureAlgorithmOID(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  X509Certificate* cert;
  ASSIGN_OR_RETURN_UNWRAP(&cert, args.This());
  Local<Value> ret;
  if (GetSignatureAlgorithmOID(env, cert->view()).ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
}

void SerialNumber(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  X509Certificate* cert;
  ASSIGN_OR_RETURN_UNWRAP(&cert, args.This());
  Local<Value> ret;
  if (GetSerialNumber(env, cert->view()).ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
}

void PublicKey(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  X509Certificate* cert;
  ASSIGN_OR_RETURN_UNWRAP(&cert, args.This());

  // TODO(tniessen): consider checking X509_get_pubkey() when the
  // X509Certificate object is being created.
  auto result = cert->view().getPublicKey();
  if (!result.value) [[unlikely]] {
    ThrowCryptoError(env, result.error.value_or(0));
    return;
  }
  auto key_data =
      KeyObjectData::CreateAsymmetric(kKeyTypePublic, std::move(result.value));

  Local<Value> ret;
  if (key_data && KeyObjectHandle::Create(env, key_data).ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
}

void KeyUsage(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  X509Certificate* cert;
  ASSIGN_OR_RETURN_UNWRAP(&cert, args.This());
  Local<Value> ret;
  if (GetKeyUsage(env, cert->view()).ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
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
  CHECK_EQ(key->Data().GetKeyType(), kKeyTypePrivate);
  args.GetReturnValue().Set(
      cert->view().checkPrivateKey(key->Data().GetAsymmetricKey()));
}

void CheckPublicKey(const FunctionCallbackInfo<Value>& args) {
  X509Certificate* cert;
  ASSIGN_OR_RETURN_UNWRAP(&cert, args.This());

  CHECK(args[0]->IsObject());
  KeyObjectHandle* key;
  ASSIGN_OR_RETURN_UNWRAP(&key, args[0]);
  // A Public Key can be derived from a private key, so we allow both.
  CHECK_NE(key->Data().GetKeyType(), kKeyTypeSecret);

  args.GetReturnValue().Set(
      cert->view().checkPublicKey(key->Data().GetAsymmetricKey()));
}

template <typename F>
void CheckX509Subject(const FunctionCallbackInfo<Value>& args, F check) {
  Environment* env = Environment::GetCurrent(args);
  X509Certificate* cert;
  ASSIGN_OR_RETURN_UNWRAP(&cert, args.This());

  CHECK(args[0]->IsString());  // subject
  CHECK(args[1]->IsUint32());  // flags

  Utf8Value subject(env->isolate(), args[0]);
  uint32_t flags = args[1].As<Uint32>()->Value();

  DataPointer matched_subject;
  X509View view = cert->view();
  auto result = check(view, subject.ToStringView(), flags, &matched_subject);
  switch (result) {
    case X509View::CheckMatch::MATCH: {
      Local<Value> ret = args[0];
      if (matched_subject) {
        ret = OneByteString(env->isolate(),
                            matched_subject.get<const char>(),
                            matched_subject.size());
      }
      return args.GetReturnValue().Set(ret);
    }
    case X509View::CheckMatch::NO_MATCH:
      break;  // No return value is set.
    case X509View::CheckMatch::INVALID_NAME:
      return THROW_ERR_INVALID_ARG_VALUE(env, "Invalid name");
    default:
      return THROW_ERR_CRYPTO_OPERATION_FAILED(env);
  }
}

void CheckHost(const FunctionCallbackInfo<Value>& args) {
  CheckX509Subject(
      args,
      [](X509View& cert,
         std::string_view subject,
         uint32_t flags,
         DataPointer* match) { return cert.checkHost(subject, flags, match); });
}

void CheckEmail(const FunctionCallbackInfo<Value>& args) {
  CheckX509Subject(
      args,
      [](X509View& cert,
         std::string_view subject,
         uint32_t flags,
         DataPointer*) { return cert.checkEmail(subject, flags); });
}

void CheckIP(const FunctionCallbackInfo<Value>& args) {
  CheckX509Subject(
      args,
      [](X509View& cert,
         std::string_view subject,
         uint32_t flags,
         DataPointer*) {
        if (subject.find('\0') != std::string_view::npos ||
            subject.find('%') != std::string_view::npos) {
          return X509View::CheckMatch::INVALID_NAME;
        }

        const std::string input(subject);
        std::array<unsigned char, 16> address{};
        size_t length = 0;
        if (uv_inet_pton(AF_INET, input.c_str(), address.data()) == 0) {
          length = 4;
        } else if (uv_inet_pton(AF_INET6, input.c_str(), address.data()) == 0) {
          length = 16;
        } else {
          return X509View::CheckMatch::INVALID_NAME;
        }
        return cert.checkIp({.data = address.data(), .len = length}, flags);
      });
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

  if (!result.value) [[unlikely]] {
    return ThrowCryptoError(env, result.error.value_or(0));
  }

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
  if (cert->toObject(env).ToLocal(&ret)) {
    args.GetReturnValue().Set(ret);
  }
}

template <typename T>
bool Set(Environment* env,
         Local<Object> target,
         Local<Value> name,
         MaybeLocal<T> maybe_value) {
  Local<Value> value;
  if (!maybe_value.ToLocal(&value)) [[unlikely]]
    return false;

  // Undefined is ignored, but still considered successful
  if (value->IsUndefined()) return true;

  return !target->Set(env->context(), name, value).IsNothing();
}

template <typename T>
bool Set(Environment* env,
         Local<Object> target,
         uint32_t index,
         MaybeLocal<T> maybe_value) {
  Local<Value> value;
  if (!maybe_value.ToLocal(&value)) [[unlikely]]
    return false;

  // Undefined is ignored, but still considered successful
  if (value->IsUndefined()) return true;

  return !target->Set(env->context(), index, value).IsNothing();
}

// Convert an X509_NAME* into a JavaScript object.
// Each entry of the name is converted into a property of the object.
// The property value may be a single string or an array of strings.
static MaybeLocal<Value> GetX509NameObject(Environment* env,
                                           const X509Name& name) {
  if (!name) return {};

  Local<Value> v8_name;
  Local<Value> v8_value;
  // Note the the resulting object uses a null prototype.
  Local<Object> result =
      Object::New(env->isolate(), Null(env->isolate()), nullptr, nullptr, 0);
  if (result.IsEmpty()) return {};

  for (auto entry : name) {
    if (!entry.has_value()) [[unlikely]] {
      THROW_ERR_CRYPTO_OPERATION_FAILED(env,
                                        "Failed to convert X509 name to UTF-8");
      return {};
    }

    const auto& [key, value] = *entry;
    // The legacy object can only represent string values. Preserve valid
    // sibling attributes and omit values that cannot be converted to UTF-8.
    if (!value.has_value()) continue;

    if (!ToV8Value(env, key).ToLocal(&v8_name) ||
        !ToV8Value(env, *value).ToLocal(&v8_value)) {
      return {};
    }

    // For backward compatibility, we only create arrays if multiple values
    // exist for the same key. That is not great but there is not much we can
    // change here without breaking things. Note that this creates nested data
    // structures, yet still does not allow representing Distinguished Names
    // accurately.
    bool multiple;
    if (!result->Has(env->context(), v8_name).To(&multiple)) {
      return {};
    }

    if (multiple) {
      Local<Value> accum;
      if (!result->Get(env->context(), v8_name).ToLocal(&accum)) {
        return {};
      }
      if (!accum->IsArray()) {
        Local<Value> items[] = {
            accum,
            v8_value,
        };
        accum = Array::New(env->isolate(), items, arraysize(items));
        if (!Set<Value>(env, result, v8_name, accum)) {
          return {};
        }
      } else {
        Local<Array> array = accum.As<Array>();
        if (!Set<Value>(env, array, array->Length(), v8_value)) {
          return {};
        }
      }
      continue;
    }

    if (!Set<Value>(env, result, v8_name, v8_value)) {
      return {};
    }
  }

  return result;
}

MaybeLocal<Object> GetPubKey(Environment* env, const ncrypto::Rsa& rsa) {
  auto bio = rsa.derPublicKey();
  Local<Value> ret;
  if (!ToBuffer(env, &bio).ToLocal(&ret)) return {};
  return ret.As<Object>();
}

MaybeLocal<Value> GetModulusString(Environment* env, const BIGNUM* n) {
  auto bio = BIOPointer::New(n);
  if (!bio) [[unlikely]]
    return {};
  return ToV8Value(env->context(), bio);
}

MaybeLocal<Value> GetExponentString(Environment* env, const BIGNUM* e) {
  if (e == nullptr) return Null(env->isolate());
  MarkPopErrorOnReturn mark_pop_error_on_return;
  auto bio = BIOPointer::NewMem();
  if (!bio) [[unlikely]]
    return {};
  if (!BIOPointer::WriteAll(bio, "0x") || BN_print(bio.get(), e) != 1) {
    return {};
  }
  return ToV8Value(env->context(), bio);
}

MaybeLocal<Value> GetECPubKey(Environment* env, const ncrypto::Ec& ec) {
  const auto group = ec.getGroup();
  const auto pubkey = ec.getPublicKey();
  if (pubkey == nullptr) [[unlikely]]
    return Undefined(env->isolate());

  return ECPointToBuffer(env, group, pubkey, ec.getPointConversionForm())
      .FromMaybe(Local<Object>());
}

MaybeLocal<Value> GetECGroupBits(Environment* env, const EC_GROUP* group) {
  if (group == nullptr) [[unlikely]]
    return Undefined(env->isolate());

  int bits = EC_GROUP_order_bits(group);
  if (bits <= 0) return Undefined(env->isolate());

  return Integer::New(env->isolate(), bits);
}

template <const char* (*nid2string)(int nid)>
MaybeLocal<Value> GetCurveName(Environment* env, const int nid) {
  const char* const curve_name = nid2string(nid);
  if (curve_name == nullptr) return Undefined(env->isolate());
  const std::string_view name = curve_name;
  return name.size() ? MaybeLocal<Value>(OneByteString(env->isolate(), name))
                     : MaybeLocal<Value>(Undefined(env->isolate()));
}

MaybeLocal<Object> X509ToObject(Environment* env, const X509View& cert) {
  EscapableHandleScope scope(env->isolate());

  auto tmpl = env->x509_dictionary_template();
  if (tmpl.IsEmpty()) {
    static constexpr std::string_view names[] = {
        "subject",
        "issuer",
        "subjectaltname",
        "infoAccess",
        "ca",
        "modulus",
        "exponent",
        "pubkey",
        "bits",
        "valid_from",
        "valid_to",
        "fingerprint",
        "fingerprint256",
        "fingerprint512",
        "ext_key_usage",
        "serialNumber",
        "raw",
        "asn1Curve",
        "nistCurve",
    };
    tmpl = DictionaryTemplate::New(env->isolate(), names);
    env->set_x509_dictionary_template(tmpl);
  }

  Local<Value> subject;
  if (!GetX509NameObject(env, cert.getSubjectName()).ToLocal(&subject)) {
    return {};
  }

  Local<Value> issuer;
  if (!GetX509NameObject(env, cert.getIssuerName()).ToLocal(&issuer)) {
    return {};
  }

  MaybeLocal<Value> values[] = {
      subject,
      issuer,
      GetSubjectAltNameString(
          env, cert, MalformedSubjectAltNameMode::kReturnLegacySentinel),
      GetInfoAccessString(env, cert),
      Boolean::New(env->isolate(), cert.isCA()),
      Undefined(env->isolate()),  // modulus
      Undefined(env->isolate()),  // exponent
      Undefined(env->isolate()),  // pubkey
      Undefined(env->isolate()),  // bits
      GetValidFrom(env, cert),
      GetValidTo(env, cert),
      GetFingerprintDigest(env, Digest::SHA1, cert),
      GetFingerprintDigest(env, Digest::SHA256, cert),
      GetFingerprintDigest(env, Digest::SHA512, cert),
      GetKeyUsage(env, cert),
      GetSerialNumber(env, cert),
      GetDer(env, cert),
      Undefined(env->isolate()),  // asn1curve
      Undefined(env->isolate()),  // nistcurve
  };

  // X509_get0_pubkey() fails when the linked OpenSSL cannot decode the
  // certificate's public key (e.g. an unsupported curve in FIPS builds).
  // ifRsa()/ifEc() assume a decodable key, so leave the key-specific
  // fields undefined in that case.
  const bool has_public_key = [&] {
    MarkPopErrorOnReturn mark_pop_error_on_return;
    return X509_get0_pubkey(cert.get()) != nullptr;
  }();
  if (!has_public_key) {
    return scope.EscapeMaybe(
        NewDictionaryInstance(env->context(), tmpl, values));
  }

  cert.ifRsa([&](const ncrypto::Rsa& rsa) {
    auto pub_key = rsa.getPublicKey();
    values[5] = GetModulusString(env, pub_key.n);   // modulus
    values[6] = GetExponentString(env, pub_key.e);  // exponent
    values[7] = GetPubKey(env, rsa);                // pubkey
    values[8] = Integer::New(env->isolate(),
                             BignumPointer::GetBitCount(pub_key.n));  // bits
    // TODO(@jasnell): The true response is a left-over from the original
    // non DictionaryTemplate-based implementation. It can be removed later.
    return true;
  });

  cert.ifEc([&](const ncrypto::Ec& ec) {
    const auto group = ec.getGroup();
    values[7] = GetECPubKey(env, ec);         // pubkey
    values[8] = GetECGroupBits(env, group);   // bits
    const int nid = ec.getCurve();
    if (nid != 0) {
      // Curve is well-known, get its OID and NIST nick-name (if it has
      // one).
      values[17] = GetCurveName<OBJ_nid2sn>(env, nid);         // asn1curve
      values[18] = GetCurveName<EC_curve_nid2nist>(env, nid);  // nistcurve
    }
    // Unnamed curves can be described by their mathematical properties,
    // but aren't used much (at all?) with X.509/TLS. Support later if
    // needed.
    return true;
  });

  return scope.EscapeMaybe(NewDictionaryInstance(env->context(), tmpl, values));
}
}  // namespace

Local<FunctionTemplate> X509Certificate::GetConstructorTemplate(
    Environment* env) {
  Local<FunctionTemplate> tmpl = env->x509_constructor_template();
  if (tmpl.IsEmpty()) {
    Isolate* isolate = env->isolate();
    tmpl = NewFunctionTemplate(isolate, NewFromHandle);
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        X509Certificate::kInternalFieldCount);
    tmpl->SetClassName(
        FIXED_ONE_BYTE_STRING(env->isolate(), "X509Certificate"));
    SetProtoMethodNoSideEffect(isolate, tmpl, "subject", Subject);
    SetProtoMethodNoSideEffect(isolate, tmpl, "subjectAltName", SubjectAltName);
    SetProtoMethodNoSideEffect(isolate, tmpl, "infoAccess", InfoAccess);
    SetProtoMethodNoSideEffect(isolate, tmpl, "issuer", Issuer);
    SetProtoMethodNoSideEffect(isolate, tmpl, "validTo", ValidTo);
    SetProtoMethodNoSideEffect(isolate, tmpl, "validFrom", ValidFrom);
    SetProtoMethodNoSideEffect(isolate, tmpl, "validToDate", ValidToDate);
    SetProtoMethodNoSideEffect(isolate, tmpl, "validFromDate", ValidFromDate);
    SetProtoMethodNoSideEffect(
        isolate, tmpl, "signatureAlgorithm", SignatureAlgorithm);
    SetProtoMethodNoSideEffect(
        isolate, tmpl, "signatureAlgorithmOid", SignatureAlgorithmOID);
    SetProtoMethodNoSideEffect(
        isolate, tmpl, "fingerprint", Fingerprint<Digest::SHA1>);
    SetProtoMethodNoSideEffect(
        isolate, tmpl, "fingerprint256", Fingerprint<Digest::SHA256>);
    SetProtoMethodNoSideEffect(
        isolate, tmpl, "fingerprint512", Fingerprint<Digest::SHA512>);
    SetProtoMethodNoSideEffect(isolate, tmpl, "keyUsage", KeyUsage);
    SetProtoMethodNoSideEffect(isolate, tmpl, "serialNumber", SerialNumber);
    SetProtoMethodNoSideEffect(isolate, tmpl, "pem", Pem);
    SetProtoMethodNoSideEffect(isolate, tmpl, "raw", Der);
    SetProtoMethodNoSideEffect(isolate, tmpl, "version", Version);
    SetProtoMethodNoSideEffect(isolate, tmpl, "subjectName", SubjectName);
    SetProtoMethodNoSideEffect(isolate, tmpl, "issuerName", IssuerName);
    SetProtoMethodNoSideEffect(isolate, tmpl, "extensions", Extensions);
    SetProtoMethodNoSideEffect(isolate, tmpl, "tbs", Tbs);
    SetProtoMethodNoSideEffect(isolate, tmpl, "signature", Signature);
    SetProtoMethodNoSideEffect(isolate, tmpl, "publicKey", PublicKey);
    SetProtoMethodNoSideEffect(isolate, tmpl, "checkCA", CheckCA);
    SetProtoMethodNoSideEffect(isolate, tmpl, "checkHost", CheckHost);
    SetProtoMethodNoSideEffect(isolate, tmpl, "checkEmail", CheckEmail);
    SetProtoMethodNoSideEffect(isolate, tmpl, "checkIP", CheckIP);
    SetProtoMethodNoSideEffect(isolate, tmpl, "checkIssued", CheckIssued);
    SetProtoMethodNoSideEffect(
        isolate, tmpl, "checkPrivateKey", CheckPrivateKey);
    SetProtoMethodNoSideEffect(isolate, tmpl, "verify", CheckPublicKey);
    SetProtoMethodNoSideEffect(isolate, tmpl, "toLegacy", ToLegacy);
    SetProtoMethodNoSideEffect(isolate, tmpl, "getIssuerCert", GetIssuerCert);
    env->set_x509_constructor_template(tmpl);
  }
  return tmpl;
}

bool X509Certificate::HasInstance(Environment* env, Local<Value> value) {
  return GetConstructorTemplate(env)->HasInstance(value);
}

void X509Certificate::NewFromHandle(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (args.Length() == 1 && args[0]->IsArrayBufferView()) {
    ArrayBufferViewContents<unsigned char> buf(args[0].As<ArrayBufferView>());
    auto result = X509Pointer::Parse(ncrypto::Buffer<const unsigned char>{
        .data = buf.data(),
        .len = buf.length(),
    });
    if (!result.value) [[unlikely]] {
      return ThrowCryptoError(env, result.error.value_or(0));
    }

    auto snapshot = X509CertificateSnapshot::Create(result.value.view());
    if (!snapshot) [[unlikely]] {
      THROW_ERR_CRYPTO_OPERATION_FAILED(env,
                                        "Failed to snapshot X.509 certificate");
      return;
    }

    auto parsed = snapshot->Parse(0);
    if (!parsed) [[unlikely]] {
      THROW_ERR_CRYPTO_OPERATION_FAILED(
          env, "Failed to parse X.509 certificate snapshot");
      return;
    }
    new X509Certificate(env,
                        args.This(),
                        ManagedX509::Adopt(std::move(parsed)),
                        std::move(snapshot),
                        0);
    return;
  }

  if (args.Length() != 1 || !HasInstance(env, args[0])) {
    THROW_ERR_INVALID_ARG_TYPE(
        env, "value must be an X509CertificateHandle or BufferSource");
    return;
  }

  X509Certificate* handle = Unwrap<X509Certificate>(args[0].As<Object>());
  CHECK_NOT_NULL(handle);
  Local<Object> issuer;
  if (handle->issuer_cert_) issuer = handle->issuer_cert_->object();
  new X509Certificate(env,
                      args.This(),
                      handle->cert_,
                      handle->snapshot_,
                      handle->snapshot_index_,
                      issuer);
}

void X509Certificate::CreateX509CertificateClass(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsFunction());

  Local<Value> ctor;
  if (!GetConstructorTemplate(env)->GetFunction(env->context()).ToLocal(&ctor))
    return;

  Local<Value> ret;
  if (!args[0]
           .As<Function>()
           ->Call(env->context(), Undefined(env->isolate()), 1, &ctor)
           .ToLocal(&ret)) {
    return;
  }

  Local<Array> constructors = ret.As<Array>();
  Local<Value> internal_ctor;
  if (!constructors->Get(env->context(), 1).ToLocal(&internal_ctor)) return;
  CHECK(env->crypto_internal_x509_certificate_constructor().IsEmpty());
  env->set_crypto_internal_x509_certificate_constructor(
      internal_ctor.As<Function>());
  args.GetReturnValue().Set(constructors);
}

void X509Certificate::IsX509Certificate(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK_EQ(args.Length(), 1);
  args.GetReturnValue().Set(HasInstance(env, args[0]));
}

MaybeLocal<Object> X509Certificate::New(Environment* env,
                                        X509Pointer cert,
                                        const STACK_OF(X509) * issuer_chain,
                                        OPENSSL_SIZE_T issuers_start) {
  if (!cert) return {};

  auto snapshot =
      X509CertificateSnapshot::Create(cert.view(), issuer_chain, issuers_start);
  if (!snapshot) [[unlikely]] {
    THROW_ERR_CRYPTO_OPERATION_FAILED(env,
                                      "Failed to snapshot X.509 certificate");
    return {};
  }

  // Keep the live certificate independent from the backend object that was
  // serialized while creating the immutable snapshot. Some backends populate
  // or invalidate lazy caches while encoding malformed-but-parseable input.
  auto parsed = snapshot->Parse(0);
  if (!parsed) [[unlikely]] {
    THROW_ERR_CRYPTO_OPERATION_FAILED(
        env, "Failed to parse X.509 certificate snapshot");
    return {};
  }
  return New(env, ManagedX509::Adopt(std::move(parsed)), std::move(snapshot));
}

MaybeLocal<Object> X509Certificate::New(
    Environment* env,
    std::shared_ptr<ManagedX509> cert,
    std::shared_ptr<const X509CertificateSnapshot> snapshot,
    size_t snapshot_index) {
  CHECK(cert);
  CHECK(snapshot);
  CHECK_LT(snapshot_index, snapshot->size());

  EscapableHandleScope scope(env->isolate());
  Local<Context> context = env->context();
  Local<Object> issuer;

  for (size_t i = snapshot->size(); i > snapshot_index + 1; i--) {
    const size_t index = i - 1;
    auto parsed = snapshot->Parse(index);
    if (!parsed) [[unlikely]] {
      THROW_ERR_CRYPTO_OPERATION_FAILED(
          env, "Failed to parse X.509 certificate snapshot");
      return {};
    }

    Local<Object> issuer_object;
    if (!GetConstructorTemplate(env)
             ->InstanceTemplate()
             ->NewInstance(context)
             .ToLocal(&issuer_object)) {
      return {};
    }
    new X509Certificate(env,
                        issuer_object,
                        ManagedX509::Adopt(std::move(parsed)),
                        snapshot,
                        index,
                        issuer);
    issuer = issuer_object;
  }

  Local<Object> obj;
  if (!GetConstructorTemplate(env)
           ->InstanceTemplate()
           ->NewInstance(context)
           .ToLocal(&obj)) {
    return {};
  }
  new X509Certificate(
      env, obj, std::move(cert), std::move(snapshot), snapshot_index, issuer);
  return scope.Escape(obj);
}

MaybeLocal<Object> X509Certificate::New(
    Environment* env,
    std::shared_ptr<const X509CertificateSnapshot> snapshot,
    size_t snapshot_index) {
  if (!snapshot || snapshot_index >= snapshot->size()) return {};
  auto cert = snapshot->Parse(snapshot_index);
  if (!cert) [[unlikely]] {
    THROW_ERR_CRYPTO_OPERATION_FAILED(
        env, "Failed to parse X.509 certificate snapshot");
    return {};
  }
  return New(env,
             ManagedX509::Adopt(std::move(cert)),
             std::move(snapshot),
             snapshot_index);
}

MaybeLocal<Object> X509Certificate::GetCert(Environment* env,
                                            const SSLPointer& ssl) {
  auto cert = X509View::From(ssl);
  if (!cert) [[unlikely]]
    return {};
  return New(env, cert.clone());
}

MaybeLocal<Object> X509Certificate::GetPeerCert(Environment* env,
                                                const SSLPointer& ssl,
                                                GetPeerCertificateFlag flag) {
  ClearErrorOnReturn clear_error_on_return;

  // The peer chain is owned by the SSL session and must not be modified. Its
  // first entry is the peer certificate on the client but not on the server.
  const STACK_OF(X509)* ssl_certs = SSL_get_peer_cert_chain(ssl.get());
  OPENSSL_SIZE_T issuers_start = 0;

  X509Pointer cert;
  if ((flag & GetPeerCertificateFlag::SERVER) ==
      GetPeerCertificateFlag::SERVER) {
    cert = X509Pointer::PeerFrom(ssl);
  }

  if (!cert && (ssl_certs == nullptr || sk_X509_num(ssl_certs) == 0))
    return MaybeLocal<Object>();

  if (!cert) {
    const X509* leaf = sk_X509_value(ssl_certs, 0);
    if (leaf != nullptr) cert = X509View(leaf).clone();
    issuers_start = 1;
  }

  if (!cert) return {};
  return New(env, std::move(cert), ssl_certs, issuers_start);
}

v8::MaybeLocal<v8::Value> X509Certificate::toObject(Environment* env) {
  return toObject(env, view());
}

v8::MaybeLocal<v8::Value> X509Certificate::toObject(Environment* env,
                                                    const X509View& cert) {
  if (!cert) [[unlikely]]
    return {};
  return X509ToObject(env, cert).FromMaybe(Local<Value>());
}

X509Certificate::X509Certificate(
    Environment* env,
    Local<Object> object,
    std::shared_ptr<ManagedX509> cert,
    std::shared_ptr<const X509CertificateSnapshot> snapshot,
    size_t snapshot_index,
    Local<Object> issuer_chain)
    : BaseObject(env, object),
      cert_(std::move(cert)),
      snapshot_(std::move(snapshot)),
      snapshot_index_(snapshot_index) {
  MakeWeak();

  if (!issuer_chain.IsEmpty()) {
    issuer_cert_.reset(Unwrap<X509Certificate>(issuer_chain));
  }
}

void X509Certificate::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackField("cert", cert_);
  tracker->TrackField("snapshot", snapshot_);
}

void X509Certificate::X509CertificateTransferData::MemoryInfo(
    MemoryTracker* tracker) const {
  tracker->TrackField("snapshot", snapshot_);
}

BaseObjectPtr<BaseObject>
X509Certificate::X509CertificateTransferData::Deserialize(
    Environment* env,
    Local<Context> context,
    std::unique_ptr<worker::TransferData> self) {
  if (context != env->context()) [[unlikely]] {
    THROW_ERR_MESSAGE_TARGET_CONTEXT_UNAVAILABLE(env);
    return {};
  }

  Local<Value> handle;
  if (!X509Certificate::New(env, snapshot_, snapshot_index_).ToLocal(&handle))
    return {};

  Local<Value> module =
      FIXED_ONE_BYTE_STRING(env->isolate(), "internal/crypto/x509");
  if (env->builtin_module_require()
          ->Call(context, Null(env->isolate()), 1, &module)
          .IsEmpty()) {
    return {};
  }

  Local<Function> ctor = env->crypto_internal_x509_certificate_constructor();
  CHECK(!ctor.IsEmpty());
  Local<Value> cert;
  if (!ctor->NewInstance(context, 1, &handle).ToLocal(&cert)) return {};

  return BaseObjectPtr<BaseObject>(Unwrap<X509Certificate>(cert.As<Object>()));
}

BaseObject::TransferMode X509Certificate::GetTransferMode() const {
  Local<Value> transfer_mode =
      object()
          ->GetPrivate(env()->context(), env()->transfer_mode_private_symbol())
          .ToLocalChecked();
  if (transfer_mode->IsUint32() &&
      (transfer_mode.As<Uint32>()->Value() & TransferMode::kCloneable) == 0) {
    return BaseObject::TransferMode::kDisallowCloneAndTransfer;
  }
  return BaseObject::TransferMode::kCloneable;
}

std::unique_ptr<worker::TransferData> X509Certificate::CloneForMessaging()
    const {
  return std::make_unique<X509CertificateTransferData>(snapshot_,
                                                       snapshot_index_);
}

void X509Certificate::Initialize(Environment* env, Local<Object> target) {
  SetMethod(env->context(),
            target,
            "createX509CertificateClass",
            CreateX509CertificateClass);
  SetMethodNoSideEffect(
      env->context(), target, "isX509Certificate", IsX509Certificate);
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
  registry->Register(CreateX509CertificateClass);
  registry->Register(IsX509Certificate);
  registry->Register(NewFromHandle);
  registry->Register(Parse);
  registry->Register(Subject);
  registry->Register(SubjectAltName);
  registry->Register(InfoAccess);
  registry->Register(Issuer);
  registry->Register(ValidTo);
  registry->Register(ValidFrom);
  registry->Register(ValidToDate);
  registry->Register(ValidFromDate);
  registry->Register(SignatureAlgorithm);
  registry->Register(SignatureAlgorithmOID);
  registry->Register(Fingerprint<Digest::SHA1>);
  registry->Register(Fingerprint<Digest::SHA256>);
  registry->Register(Fingerprint<Digest::SHA512>);
  registry->Register(KeyUsage);
  registry->Register(SerialNumber);
  registry->Register(Pem);
  registry->Register(Der);
  registry->Register(Version);
  registry->Register(SubjectName);
  registry->Register(IssuerName);
  registry->Register(Extensions);
  registry->Register(Tbs);
  registry->Register(Signature);
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
