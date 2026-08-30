#include "crypto/crypto_x509_gen.h"

#include "base_object-inl.h"
#include "crypto/crypto_x509.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "ncrypto.h"
#include "node_buffer.h"
#include "node_errors.h"
#include "threadpoolwork-inl.h"
#include "uv.h"
#include "v8.h"

#include <openssl/asn1.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <array>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace node {

using ncrypto::BignumPointer;
using ncrypto::BIOPointer;
using ncrypto::DataPointer;
using ncrypto::EVPKeyCtxPointer;
using ncrypto::EVPKeyPointer;
using ncrypto::EVPMDCtxPointer;
using ncrypto::MarkPopErrorOnReturn;
using ncrypto::X509Pointer;
using v8::Array;
using v8::Boolean;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::JustVoid;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Nothing;
using v8::Object;
using v8::Uint32;
using v8::Value;

namespace crypto {
namespace {

using ASN1BitStringPointer =
    ncrypto::DeleteFnPtr<ASN1_BIT_STRING, ASN1_BIT_STRING_free>;
using ASN1IntegerPointer =
    ncrypto::DeleteFnPtr<ASN1_INTEGER, ASN1_INTEGER_free>;
using ASN1ObjectPointer = ncrypto::DeleteFnPtr<ASN1_OBJECT, ASN1_OBJECT_free>;
using ASN1OctetStringPointer =
    ncrypto::DeleteFnPtr<ASN1_OCTET_STRING, ASN1_OCTET_STRING_free>;
using AuthorityKeyIdPointer =
    ncrypto::DeleteFnPtr<AUTHORITY_KEYID, AUTHORITY_KEYID_free>;
using BasicConstraintsPointer =
    ncrypto::DeleteFnPtr<BASIC_CONSTRAINTS, BASIC_CONSTRAINTS_free>;
using ExtendedKeyUsagePointer =
    ncrypto::DeleteFnPtr<EXTENDED_KEY_USAGE, EXTENDED_KEY_USAGE_free>;
using GeneralNamePointer =
    ncrypto::DeleteFnPtr<GENERAL_NAME, GENERAL_NAME_free>;
using GeneralNamesPointer =
    ncrypto::DeleteFnPtr<GENERAL_NAMES, GENERAL_NAMES_free>;
using X509ExtensionPointer =
    ncrypto::DeleteFnPtr<X509_EXTENSION, X509_EXTENSION_free>;
using X509NamePointer = ncrypto::DeleteFnPtr<X509_NAME, X509_NAME_free>;

struct X509ExtensionsDeleter final {
  void operator()(STACK_OF(X509_EXTENSION) * extensions) const {
    sk_X509_EXTENSION_pop_free(extensions, X509_EXTENSION_free);
  }
};

using X509ExtensionsPointer =
    std::unique_ptr<STACK_OF(X509_EXTENSION), X509ExtensionsDeleter>;

constexpr std::string_view kSubjectAltNameOid = "2.5.29.17";
constexpr std::string_view kSubjectKeyIdentifierOid = "2.5.29.14";
constexpr std::string_view kAuthorityKeyIdentifierOid = "2.5.29.35";
constexpr std::string_view kBasicConstraintsOid = "2.5.29.19";
constexpr std::string_view kKeyUsageOid = "2.5.29.15";
constexpr std::string_view kExtendedKeyUsageOid = "2.5.29.37";
constexpr size_t kRfc7093KeyIdentifierLength = 20;
constexpr size_t kMaximumSerialLength = 20;

enum class GeneralNameKind : uint32_t {
  kDns = 0,
  kIp = 1,
  kRfc822 = 2,
  kUri = 4,
};

enum class ExtensionKind : uint32_t {
  kRaw = 0,
  kBasicConstraints = 1,
  kKeyUsage = 2,
  kExtendedKeyUsage = 3,
  kSubjectAltName = 4,
  kSubjectKeyIdentifier = 5,
  kAuthorityKeyIdentifier = 6,
};

constexpr uint32_t kKeyUsageKeyCertSign = 1 << 5;

struct ExtensionProfile final {
  bool basic_constraints_ca = false;
  bool basic_constraints_path_length = false;
  bool key_usage_present = false;
  uint32_t key_usage = 0;
  bool subject_alt_name_critical = false;
};

X509Pointer NewCertificate() {
#if NCRYPTO_USE_OPENSSL_PROVIDER
  return X509Pointer(X509_new_ex(nullptr, nullptr));
#elif NCRYPTO_USE_BORINGSSL
  return X509Pointer(X509_new());
#else
#error "Unsupported ncrypto backend"
#endif
}

ByteSource EncodeCertificate(X509* certificate) {
  const int length = i2d_X509(certificate, nullptr);
  if (length <= 0) [[unlikely]]
    return {};

  auto der = DataPointer::Alloc(static_cast<size_t>(length));
  if (!der) [[unlikely]]
    return {};
  unsigned char* cursor = der.get<unsigned char>();
  if (i2d_X509(certificate, &cursor) != length ||
      cursor != der.get<unsigned char>() + length) [[unlikely]] {
    return {};
  }
  return ByteSource::Allocated(der.release());
}

X509RequestPointer ParseDerRequest(ncrypto::Buffer<const unsigned char> input,
                                   DataPointer* encoded = nullptr) {
  if (input.data == nullptr || input.len == 0 ||
      input.len > static_cast<size_t>(LONG_MAX)) {
    return {};
  }
  const unsigned char* cursor = input.data;
  X509RequestPointer request(d2i_X509_REQ(
      nullptr, &cursor, static_cast<long>(input.len)));  // NOLINT(runtime/int)
  if (!request) return {};
  if (encoded != nullptr) {
    *encoded = DataPointer::Copy({
        .data = input.data,
        .len = static_cast<size_t>(cursor - input.data),
    });
    if (!*encoded) return {};
  }
  return request;
}

X509RequestPointer ParseRequest(ncrypto::Buffer<const unsigned char> input,
                                DataPointer* encoded) {
  if (encoded == nullptr) return {};
  if (input.data == nullptr || input.len == 0 || input.len > INT_MAX) return {};

  BIOPointer bio = BIOPointer::New(input.data, input.len);
  unsigned char* der = nullptr;
  long der_length = 0;  // NOLINT(runtime/int)
  if (bio &&
      PEM_bytes_read_bio(&der,
                         &der_length,
                         nullptr,
                         PEM_STRING_X509_REQ,
                         bio.get(),
                         NoPasswordCallback,
                         nullptr) == 1 &&
      der != nullptr && der_length > 0) {
    DataPointer decoded(der, static_cast<size_t>(der_length));
    return ParseDerRequest(
        {
            .data = decoded.get<const unsigned char>(),
            .len = decoded.size(),
        },
        encoded);
  }
  OPENSSL_free(der);
  return ParseDerRequest(input, encoded);
}

bool IsValidRequest(X509_REQ* request) {
  if (request == nullptr || X509_REQ_get_version(request) != 0) return false;
  const X509_NAME* subject = X509_REQ_get_subject_name(request);
  return subject != nullptr && X509_NAME_entry_count(subject) >= 0 &&
         X509_REQ_get0_pubkey(request) != nullptr;
}

std::optional<std::vector<ncrypto::X509RawExtension>>
InspectRequestedExtensions(X509_REQ* request) {
  const int attribute_count = X509_REQ_get_attr_count(request);
  if (attribute_count < 0) return std::nullopt;
  bool has_extension_request = false;
  for (int index = 0; index < attribute_count; index++) {
    X509_ATTRIBUTE* attribute = X509_REQ_get_attr(request, index);
    if (attribute == nullptr) return std::nullopt;
    const ASN1_OBJECT* object = X509_ATTRIBUTE_get0_object(attribute);
    if (object == nullptr) return std::nullopt;
    if (X509_REQ_extension_nid(OBJ_obj2nid(object)) == 1) {
      has_extension_request = true;
      break;
    }
  }
  if (!has_extension_request) {
    return std::vector<ncrypto::X509RawExtension>();
  }
  X509ExtensionsPointer extensions(X509_REQ_get_extensions(request));
  if (!extensions) return std::nullopt;
  return ncrypto::inspectX509Extensions(extensions.get());
}

bool GetArrayEntry(Local<Context> context,
                   Local<Array> array,
                   uint32_t index,
                   Local<Value>* value) {
  return array->Get(context, index).ToLocal(value);
}

ASN1ObjectPointer ParseOid(Environment* env,
                           Local<Value> value,
                           std::string* canonical = nullptr) {
  if (!value->IsString()) return {};
  Utf8Value oid(env->isolate(), value);
  if (*oid == nullptr || oid.length() == 0) return {};
  const std::string input = oid.ToString();
  ASN1ObjectPointer object(OBJ_txt2obj(*oid, 1));
  if (!object) return {};

  const int length = OBJ_obj2txt(nullptr, 0, object.get(), 1);
  if (length <= 0 || length == std::numeric_limits<int>::max()) return {};
  std::string text(static_cast<size_t>(length) + 1, '\0');
  if (OBJ_obj2txt(text.data(), length + 1, object.get(), 1) != length) {
    return {};
  }
  text.resize(static_cast<size_t>(length));
  if (text != input) return {};
  if (canonical != nullptr) *canonical = std::move(text);
  return object;
}

std::string ReadString(Environment* env, Local<Value> value) {
  if (!value->IsString()) return {};
  return Utf8Value(env->isolate(), value).ToString();
}

bool IsAsciiWithoutNul(std::string_view input) {
  for (unsigned char byte : input) {
    if (byte == 0 || byte > 0x7f) return false;
  }
  return true;
}

X509NamePointer BuildName(Environment* env, Local<Array> spec) {
  Local<Context> context = env->context();
  X509NamePointer name(X509_NAME_new());
  if (!name) [[unlikely]]
    return {};

  for (uint32_t rdn_index = 0; rdn_index < spec->Length(); rdn_index++) {
    Local<Value> rdn_value;
    if (!GetArrayEntry(context, spec, rdn_index, &rdn_value) ||
        !rdn_value->IsArray()) {
      return {};
    }
    Local<Array> rdn = rdn_value.As<Array>();
    if (rdn->Length() == 0) return {};

    for (uint32_t index = 0; index < rdn->Length(); index++) {
      Local<Value> attribute_value;
      if (!GetArrayEntry(context, rdn, index, &attribute_value) ||
          !attribute_value->IsArray()) {
        return {};
      }
      Local<Array> attribute = attribute_value.As<Array>();
      if (attribute->Length() != 2) return {};

      Local<Value> oid_value;
      Local<Value> content_value;
      if (!GetArrayEntry(context, attribute, 0, &oid_value) ||
          !GetArrayEntry(context, attribute, 1, &content_value) ||
          !content_value->IsString()) {
        return {};
      }
      ASN1ObjectPointer oid = ParseOid(env, oid_value);
      if (!oid) return {};

      const std::string text = ReadString(env, content_value);
      if (text.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
        return {};
      }
      const auto* bytes = reinterpret_cast<const unsigned char*>(text.data());
      if (X509_NAME_add_entry_by_OBJ(name.get(),
                                     oid.get(),
                                     MBSTRING_UTF8,
                                     bytes,
                                     static_cast<int>(text.size()),
                                     -1,
                                     index == 0 ? 0 : -1) != 1) {
        return {};
      }
    }
  }
  return name;
}

X509NamePointer DecodeName(Local<Value> value) {
  ArrayBufferOrViewContents<unsigned char> input(value);
  if (!input.CheckSizeInt32() || input.size() == 0) return {};

  const unsigned char* cursor = input.data();
  X509NamePointer name(
      d2i_X509_NAME(nullptr,
                    &cursor,
                    static_cast<long>(input.size())));  // NOLINT(runtime/int)
  if (!name || cursor != input.data() + input.size()) return {};
  return name;
}

bool SetString(ASN1_STRING* target, std::string_view value) {
  return value.size() <= static_cast<size_t>(std::numeric_limits<int>::max()) &&
         ASN1_STRING_set(
             target, value.data(), static_cast<int>(value.size())) == 1;
}

GeneralNamePointer BuildGeneralName(Environment* env, Local<Array> spec) {
  Local<Context> context = env->context();
  if (spec->Length() != 2) return {};

  Local<Value> kind_value;
  Local<Value> value;
  if (!GetArrayEntry(context, spec, 0, &kind_value) ||
      !GetArrayEntry(context, spec, 1, &value) || !kind_value->IsUint32() ||
      !value->IsString()) {
    return {};
  }

  const auto kind =
      static_cast<GeneralNameKind>(kind_value.As<Uint32>()->Value());
  const std::string text = ReadString(env, value);
  GeneralNamePointer name(GENERAL_NAME_new());
  if (!name) return {};

  switch (kind) {
    case GeneralNameKind::kDns:
    case GeneralNameKind::kRfc822:
    case GeneralNameKind::kUri: {
      if (text.empty() || !IsAsciiWithoutNul(text)) return {};
      ncrypto::DeleteFnPtr<ASN1_IA5STRING, ASN1_IA5STRING_free> string(
          ASN1_IA5STRING_new());
      if (!string || !SetString(string.get(), text)) return {};
      const int type = kind == GeneralNameKind::kDns      ? GEN_DNS
                       : kind == GeneralNameKind::kRfc822 ? GEN_EMAIL
                                                          : GEN_URI;
      GENERAL_NAME_set0_value(name.get(), type, string.release());
      break;
    }
    case GeneralNameKind::kIp: {
      if (text.find('\0') != std::string::npos ||
          text.find('%') != std::string::npos) {
        return {};
      }
      std::array<unsigned char, 16> address{};
      size_t length;
      if (uv_inet_pton(AF_INET, text.c_str(), address.data()) == 0) {
        length = 4;
      } else if (uv_inet_pton(AF_INET6, text.c_str(), address.data()) == 0) {
        length = address.size();
      } else {
        return {};
      }
      ASN1OctetStringPointer string(ASN1_OCTET_STRING_new());
      if (!string ||
          ASN1_OCTET_STRING_set(
              string.get(), address.data(), static_cast<int>(length)) != 1) {
        return {};
      }
      GENERAL_NAME_set0_value(name.get(), GEN_IPADD, string.release());
      break;
    }
    default:
      return {};
  }
  return name;
}

GeneralNamesPointer BuildGeneralNames(Environment* env, Local<Array> specs) {
  Local<Context> context = env->context();
  if (specs->Length() == 0) return {};
  GeneralNamesPointer names(GENERAL_NAMES_new());
  if (!names) return {};

  for (uint32_t index = 0; index < specs->Length(); index++) {
    Local<Value> value;
    if (!GetArrayEntry(context, specs, index, &value) || !value->IsArray()) {
      return {};
    }
    GeneralNamePointer name = BuildGeneralName(env, value.As<Array>());
    if (!name || sk_GENERAL_NAME_push(names.get(), name.get()) <= 0) return {};
    name.release();
  }
  return names;
}

bool IsValidSerial(const unsigned char* data, size_t size) {
  if (size == 0 || size > kMaximumSerialLength || data[0] == 0) return false;
  return size != kMaximumSerialLength || (data[0] & 0x80) == 0;
}

bool ReadKeyIdentifierSpec(Environment* env,
                           Local<Array> spec,
                           bool* automatic,
                           ASN1OctetStringPointer* identifier) {
  Local<Context> context = env->context();
  Local<Value> critical_value;
  Local<Value> identifier_value;
  if (spec->Length() != 3 ||
      !GetArrayEntry(context, spec, 1, &critical_value) ||
      !GetArrayEntry(context, spec, 2, &identifier_value) ||
      !critical_value->IsBoolean()) {
    return false;
  }

  *automatic = identifier_value->IsUndefined();
  identifier->reset();
  if (*automatic) return true;
  if (!IsAnyBufferSource(identifier_value)) return false;

  ArrayBufferOrViewContents<unsigned char> contents(identifier_value);
  if (!contents.CheckSizeInt32()) return false;
  identifier->reset(ASN1_OCTET_STRING_new());
  return *identifier &&
         ASN1_OCTET_STRING_set(identifier->get(),
                               contents.size() == 0 ? nullptr : contents.data(),
                               static_cast<int>(contents.size())) == 1;
}

// RFC 7093, Section 2, method 1.
ASN1OctetStringPointer ComputeKeyIdentifier(const X509* certificate) {
  std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
  unsigned int digest_length = 0;
  if (X509_pubkey_digest(
          certificate, EVP_sha256(), digest.data(), &digest_length) != 1 ||
      digest_length < kRfc7093KeyIdentifierLength) {
    return {};
  }
  ASN1OctetStringPointer identifier(ASN1_OCTET_STRING_new());
  if (!identifier || ASN1_OCTET_STRING_set(
                         identifier.get(),
                         digest.data(),
                         static_cast<int>(kRfc7093KeyIdentifierLength)) != 1) {
    return {};
  }
  return identifier;
}

bool AddExtension(X509* certificate, X509ExtensionPointer extension) {
  return extension && X509_add_ext(certificate, extension.get(), -1) == 1;
}

bool BuildAndAddExtension(
    Environment* env,
    X509* certificate,
    const X509* signing_certificate,
    const ASN1_OCTET_STRING* self_signed_subject_key_identifier,
    Local<Array> spec,
    std::string* oid_text,
    ExtensionProfile* profile) {
  Local<Context> context = env->context();
  if (spec->Length() < 2) return false;

  Local<Value> kind_value;
  if (!GetArrayEntry(context, spec, 0, &kind_value) ||
      !kind_value->IsUint32()) {
    return false;
  }
  const auto kind =
      static_cast<ExtensionKind>(kind_value.As<Uint32>()->Value());

  Local<Value> critical_value;
  if (kind == ExtensionKind::kRaw) {
    Local<Value> oid_value;
    Local<Value> value;
    if (spec->Length() != 4 || !GetArrayEntry(context, spec, 1, &oid_value) ||
        !GetArrayEntry(context, spec, 2, &critical_value) ||
        !GetArrayEntry(context, spec, 3, &value) || !oid_value->IsString() ||
        !critical_value->IsBoolean()) {
      return false;
    }
    ASN1ObjectPointer oid = ParseOid(env, oid_value, oid_text);
    if (!oid || !IsAnyBufferSource(value)) {
      return false;
    }
    ArrayBufferOrViewContents<unsigned char> contents(value);
    if (!contents.CheckSizeInt32()) return false;
    ASN1OctetStringPointer octets(ASN1_OCTET_STRING_new());
    if (!octets ||
        ASN1_OCTET_STRING_set(octets.get(),
                              contents.data(),
                              static_cast<int>(contents.size())) != 1) {
      return false;
    }
    X509ExtensionPointer extension(X509_EXTENSION_create_by_OBJ(
        nullptr,
        oid.get(),
        critical_value.As<Boolean>()->Value() ? 1 : 0,
        octets.get()));
    return AddExtension(certificate, std::move(extension));
  }

  if (!GetArrayEntry(context, spec, 1, &critical_value) ||
      !critical_value->IsBoolean()) {
    return false;
  }
  const bool critical = critical_value.As<Boolean>()->Value();

  switch (kind) {
    case ExtensionKind::kRaw:
      UNREACHABLE();
    case ExtensionKind::kBasicConstraints: {
      Local<Value> ca_value;
      Local<Value> path_length_value;
      if (spec->Length() != 4 || !GetArrayEntry(context, spec, 2, &ca_value) ||
          !GetArrayEntry(context, spec, 3, &path_length_value) ||
          !ca_value->IsBoolean()) {
        return false;
      }
      BasicConstraintsPointer constraints(BASIC_CONSTRAINTS_new());
      if (!constraints) return false;
      constraints->ca = ca_value.As<Boolean>()->Value() ? 0xff : 0;
      profile->basic_constraints_ca = constraints->ca != 0;
      if (!path_length_value->IsUndefined()) {
        int64_t path_length;
        if (!path_length_value->IntegerValue(context).To(&path_length) ||
            path_length < 0) {
          return false;
        }
        constraints->pathlen = ASN1_INTEGER_new();
        if (constraints->pathlen == nullptr ||
            ASN1_INTEGER_set_int64(constraints->pathlen, path_length) != 1) {
          return false;
        }
        profile->basic_constraints_path_length = true;
      }
      oid_text->assign(kBasicConstraintsOid);
      return AddExtension(
          certificate,
          X509ExtensionPointer(X509V3_EXT_i2d(
              NID_basic_constraints, critical ? 1 : 0, constraints.get())));
    }
    case ExtensionKind::kKeyUsage: {
      Local<Value> mask_value;
      if (spec->Length() != 3 ||
          !GetArrayEntry(context, spec, 2, &mask_value) ||
          !mask_value->IsUint32()) {
        return false;
      }
      const uint32_t mask = mask_value.As<Uint32>()->Value();
      if (mask == 0 || (mask & ~0x1ffU) != 0) return false;
      ASN1BitStringPointer usage(ASN1_BIT_STRING_new());
      if (!usage) return false;
      for (int bit = 0; bit <= 8; bit++) {
        if ((mask & (1U << bit)) != 0 &&
            ASN1_BIT_STRING_set_bit(usage.get(), bit, 1) != 1) {
          return false;
        }
      }
      profile->key_usage_present = true;
      profile->key_usage = mask;
      oid_text->assign(kKeyUsageOid);
      return AddExtension(certificate,
                          X509ExtensionPointer(X509V3_EXT_i2d(
                              NID_key_usage, critical ? 1 : 0, usage.get())));
    }
    case ExtensionKind::kExtendedKeyUsage: {
      Local<Value> usages_value;
      if (spec->Length() != 3 ||
          !GetArrayEntry(context, spec, 2, &usages_value) ||
          !usages_value->IsArray()) {
        return false;
      }
      Local<Array> usages = usages_value.As<Array>();
      if (usages->Length() == 0) return false;
      ExtendedKeyUsagePointer usage(EXTENDED_KEY_USAGE_new());
      if (!usage) return false;
      for (uint32_t index = 0; index < usages->Length(); index++) {
        Local<Value> oid_value;
        if (!GetArrayEntry(context, usages, index, &oid_value)) return false;
        ASN1ObjectPointer oid = ParseOid(env, oid_value);
        if (!oid || sk_ASN1_OBJECT_push(usage.get(), oid.get()) <= 0) {
          return false;
        }
        oid.release();
      }
      oid_text->assign(kExtendedKeyUsageOid);
      return AddExtension(
          certificate,
          X509ExtensionPointer(X509V3_EXT_i2d(
              NID_ext_key_usage, critical ? 1 : 0, usage.get())));
    }
    case ExtensionKind::kSubjectAltName: {
      Local<Value> names_value;
      if (spec->Length() != 3 ||
          !GetArrayEntry(context, spec, 2, &names_value) ||
          !names_value->IsArray()) {
        return false;
      }
      Local<Array> name_specs = names_value.As<Array>();
      GeneralNamesPointer names = BuildGeneralNames(env, name_specs);
      if (!names) return false;
      profile->subject_alt_name_critical = critical;
      oid_text->assign(kSubjectAltNameOid);
      return AddExtension(
          certificate,
          X509ExtensionPointer(X509V3_EXT_i2d(
              NID_subject_alt_name, critical ? 1 : 0, names.get())));
    }
    case ExtensionKind::kSubjectKeyIdentifier:
    case ExtensionKind::kAuthorityKeyIdentifier: {
      bool automatic;
      ASN1OctetStringPointer identifier;
      if (!ReadKeyIdentifierSpec(env, spec, &automatic, &identifier)) {
        return false;
      }
      if (automatic) {
        if (kind == ExtensionKind::kAuthorityKeyIdentifier) {
          if (signing_certificate == certificate &&
              self_signed_subject_key_identifier != nullptr) {
            identifier.reset(
                ASN1_OCTET_STRING_dup(self_signed_subject_key_identifier));
            if (!identifier) {
              THROW_ERR_CRYPTO_OPERATION_FAILED(
                  env, "Failed to copy an X.509 key identifier");
              return false;
            }
          } else if (signing_certificate != certificate) {
            int issuer_identifier_status = -1;
            identifier.reset(static_cast<ASN1_OCTET_STRING*>(
                X509_get_ext_d2i(signing_certificate,
                                 NID_subject_key_identifier,
                                 &issuer_identifier_status,
                                 nullptr)));
            if (!identifier && issuer_identifier_status != -1) {
              THROW_ERR_INVALID_ARG_VALUE(
                  env, "The issuer has an invalid Subject Key Identifier");
              return false;
            }
          }
        }
        if (!identifier) {
          const X509* source = kind == ExtensionKind::kSubjectKeyIdentifier
                                   ? certificate
                                   : signing_certificate;
          identifier = ComputeKeyIdentifier(source);
        }
        if (!identifier) {
          THROW_ERR_CRYPTO_OPERATION_FAILED(
              env, "Failed to derive an X.509 key identifier");
          return false;
        }
      }

      if (kind == ExtensionKind::kSubjectKeyIdentifier) {
        oid_text->assign(kSubjectKeyIdentifierOid);
        return AddExtension(
            certificate,
            X509ExtensionPointer(X509V3_EXT_i2d(NID_subject_key_identifier,
                                                critical ? 1 : 0,
                                                identifier.get())));
      }

      AuthorityKeyIdPointer value(AUTHORITY_KEYID_new());
      if (!value) return false;
      value->keyid = identifier.release();
      oid_text->assign(kAuthorityKeyIdentifierOid);
      return AddExtension(
          certificate,
          X509ExtensionPointer(X509V3_EXT_i2d(
              NID_authority_key_identifier, critical ? 1 : 0, value.get())));
    }
    default:
      return false;
  }
}

bool ValidateExtensionProfile(bool subject_empty,
                              const ExtensionProfile& profile) {
  if (subject_empty && !profile.subject_alt_name_critical) {
    return false;
  }

  const bool key_cert_sign = profile.key_usage_present &&
                             (profile.key_usage & kKeyUsageKeyCertSign) != 0;
  if (key_cert_sign && !profile.basic_constraints_ca) return false;
  if (profile.basic_constraints_path_length &&
      (!profile.basic_constraints_ca || !key_cert_sign)) {
    return false;
  }
  return true;
}

void ParseX509CertificateRequest(const FunctionCallbackInfo<Value>& args) {
  MarkPopErrorOnReturn mark_pop_error_on_return;
  Environment* env = Environment::GetCurrent(args);
  Local<Context> context = env->context();
  CHECK(IsAnyBufferSource(args[0]));
  ArrayBufferOrViewContents<unsigned char> input(args[0]);
  if (!input.CheckSizeInt32()) [[unlikely]] {
    THROW_ERR_OUT_OF_RANGE(env, "X.509 certificate request is too large");
    return;
  }

  DataPointer encoded;
  X509RequestPointer request(
      ParseRequest({.data = input.data(), .len = input.size()}, &encoded));
  if (!IsValidRequest(request.get())) [[unlikely]] {
    THROW_ERR_INVALID_ARG_VALUE(
        env, "The \"options.csr\" argument is not a valid X.509 request");
    return;
  }

  const auto inspected = InspectRequestedExtensions(request.get());
  if (!inspected.has_value() || !encoded) [[unlikely]] {
    THROW_ERR_INVALID_ARG_VALUE(
        env, "The \"options.csr\" argument is not a valid X.509 request");
    return;
  }

  Local<Object> der;
  Local<Value> requested_extensions;
  Local<Array> result = Array::New(env->isolate(), 2);
  const char* data = encoded.size() == 0 ? "" : encoded.get<const char>();
  if (!Buffer::Copy(env, data, encoded.size()).ToLocal(&der) ||
      !CreateRawX509Extensions(env, inspected.value())
           .ToLocal(&requested_extensions) ||
      result->CreateDataProperty(context, 0, der).IsNothing() ||
      result->CreateDataProperty(context, 1, requested_extensions)
          .IsNothing()) {
    return;
  }
  args.GetReturnValue().Set(result);
}

bool GenerationFailed(CryptoErrorStore* errors) {
  errors->Capture();
  errors->Insert(NodeCryptoError::X509_GENERATION_FAILED);
  errors->SetNodeErrorCode("ERR_CRYPTO_OPERATION_FAILED");
  return false;
}

bool RequestVerificationFailed(CryptoErrorStore* errors) {
  errors->Insert(NodeCryptoError::X509_REQUEST_VERIFICATION_FAILED);
  errors->SetNodeErrorCode("ERR_CRYPTO_OPERATION_FAILED");
  return false;
}

}  // namespace

X509GenerateConfig::X509GenerateConfig(X509GenerateConfig&& other) noexcept
    : certificate(std::move(other.certificate)),
      request(std::move(other.request)),
      request_public_key(std::move(other.request_public_key)),
      signing_key(std::move(other.signing_key)),
      digest(other.digest),
      salt_length(other.salt_length),
      padding(other.padding) {}

X509GenerateConfig& X509GenerateConfig::operator=(
    X509GenerateConfig&& other) noexcept {
  if (&other == this) return *this;
  this->~X509GenerateConfig();
  return *new (this) X509GenerateConfig(std::move(other));
}

void X509GenerateConfig::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackFieldWithSize("certificate", certificate ? kSizeOf_X509 : 0);
  tracker->TrackFieldWithSize("request", request ? kSizeOf_X509_REQ : 0);
  tracker->TrackFieldWithSize("request_public_key",
                              request_public_key ? kSizeOf_EVP_PKEY : 0);
  tracker->TrackField("signing_key", signing_key);
}

Maybe<void> X509GenerateTraits::AdditionalConfig(
    CryptoJobMode mode,
    const FunctionCallbackInfo<Value>& args,
    unsigned int offset,
    X509GenerateConfig* params) {
  MarkPopErrorOnReturn mark_pop_error_on_return;
  Environment* env = Environment::GetCurrent(args);
  Local<Context> context = env->context();
  static_cast<void>(mode);

  CHECK(args[offset]->IsUndefined() || IsAnyBufferSource(args[offset]));
  CHECK(args[offset + 1]->IsUndefined() || args[offset + 1]->IsArray() ||
        IsAnyBufferSource(args[offset + 1]));
  const bool from_request = IsAnyBufferSource(args[offset]);
  unsigned int cursor = offset + 2;
  KeyObjectData public_key;
  X509RequestPointer request;
  EVPKeyPointer owned_request_public_key;
  OSSL3_CONST X509_NAME* request_subject = nullptr;
  EVP_PKEY* request_public_key = nullptr;
  if (from_request) {
    CHECK(args[offset + 1]->IsUndefined());
    ArrayBufferOrViewContents<unsigned char> input(args[offset]);
    request = ParseDerRequest({.data = input.data(), .len = input.size()});
    if (!IsValidRequest(request.get())) [[unlikely]] {
      THROW_ERR_INVALID_ARG_VALUE(
          env, "The \"options.csr\" argument is not a valid X.509 request");
      return Nothing<void>();
    }
    request_subject = X509_REQ_get_subject_name(request.get());
    request_public_key = X509_REQ_get0_pubkey(request.get());
    if (request_public_key == nullptr ||
        EVP_PKEY_up_ref(request_public_key) != 1) [[unlikely]] {
      THROW_ERR_INVALID_ARG_VALUE(
          env, "The \"options.csr\" argument is not a valid X.509 request");
      return Nothing<void>();
    }
    owned_request_public_key.reset(request_public_key);
    for (unsigned int index = 0; index < 5; index++) {
      CHECK(args[cursor + index]->IsUndefined());
    }
    cursor += 5;
  } else {
    CHECK(args[offset + 1]->IsArray() || IsAnyBufferSource(args[offset + 1]));
    public_key = KeyObjectData::GetPublicOrPrivateKeyFromJs(args, &cursor);
    if (!public_key) return Nothing<void>();
    if (public_key.GetKeyType() == kKeyTypeSecret) [[unlikely]] {
      THROW_ERR_INVALID_ARG_VALUE(env, "Invalid certificate public key");
      return Nothing<void>();
    }
  }

  const unsigned int issuer_offset = cursor++;
  const unsigned int serial_offset = cursor++;
  const unsigned int not_before_offset = cursor++;
  const unsigned int not_after_offset = cursor++;
  const unsigned int extensions_offset = cursor++;
  CHECK(args[issuer_offset]->IsUndefined() ||
        X509Certificate::HasInstance(env, args[issuer_offset]));
  CHECK(IsAnyBufferSource(args[serial_offset]));
  CHECK(args[not_before_offset]->IsString());
  CHECK(args[not_after_offset]->IsString());
  CHECK(args[extensions_offset]->IsArray());

  KeyObjectData signing_key =
      KeyObjectData::GetPrivateKeyFromJs(args, &cursor, true);
  if (!signing_key) return Nothing<void>();
  if (signing_key.GetKeyType() != kKeyTypePrivate) [[unlikely]] {
    THROW_ERR_INVALID_ARG_VALUE(
        env, "The \"options.signature.key\" argument must be a private key");
    return Nothing<void>();
  }

  const unsigned int digest_offset = cursor++;
  const unsigned int salt_length_offset = cursor++;
  const unsigned int padding_offset = cursor;
  CHECK(args[digest_offset]->IsString() || args[digest_offset]->IsNull());
  CHECK(args[salt_length_offset]->IsUndefined() ||
        args[salt_length_offset]->IsInt32());
  CHECK(args[padding_offset]->IsUndefined() || args[padding_offset]->IsInt32());
  if (args[digest_offset]->IsString()) {
    Utf8Value digest(env->isolate(), args[digest_offset]);
    params->digest = ncrypto::Digest::FromName(*digest);
    if (!params->digest) [[unlikely]] {
      THROW_ERR_CRYPTO_INVALID_DIGEST(env, "Invalid digest: %s", digest);
      return Nothing<void>();
    }
  } else {
    const auto* algorithm = signing_key.GetAsymmetricKey().getAlgorithm();
    if (algorithm == nullptr || !algorithm->isOneShot()) [[unlikely]] {
      THROW_ERR_INVALID_ARG_VALUE(
          env,
          "The \"options.signature.digest\" option may be null only for "
          "an intrinsic signature algorithm");
      return Nothing<void>();
    }
  }
  if (args[salt_length_offset]->IsInt32()) {
    params->salt_length = args[salt_length_offset].As<v8::Int32>()->Value();
  }
  if (args[padding_offset]->IsInt32()) {
    params->padding = args[padding_offset].As<v8::Int32>()->Value();
  }

#if !NCRYPTO_HAS_X509_RSA_PSS_FLEXIBLE_PARAMETERS
  const EVPKeyPointer& native_signing_key = signing_key.GetAsymmetricKey();
  if (native_signing_key.isRsaVariant()) {
    const int padding =
        params->padding.value_or(native_signing_key.getDefaultSignPadding());
    if (padding == RSA_PKCS1_PSS_PADDING) {
      const int digest_nid =
          params->digest ? EVP_MD_type(params->digest.get()) : NID_undef;
      const int digest_size =
          params->digest ? EVP_MD_size(params->digest.get()) : -1;
      const bool supported_digest = digest_nid == NID_sha256 ||
                                    digest_nid == NID_sha384 ||
                                    digest_nid == NID_sha512;
      const bool supported_salt =
          !params->salt_length.has_value() ||
          params->salt_length == RSA_PSS_SALTLEN_DIGEST ||
          params->salt_length == digest_size;
      if (!supported_digest || !supported_salt) {
        THROW_ERR_CRYPTO_UNSUPPORTED_OPERATION(
            env,
            "This backend does not support the requested X.509 RSA-PSS "
            "parameters");
        return Nothing<void>();
      }
    }
  }
#endif

  X509NamePointer owned_subject;
  OSSL3_CONST X509_NAME* subject = request_subject;
  EVP_PKEY* certificate_public_key = request_public_key;
  if (!from_request) {
    Local<Value> subject_value = args[offset + 1];
    owned_subject = subject_value->IsArray()
                        ? BuildName(env, subject_value.As<Array>())
                        : DecodeName(subject_value);
    if (!owned_subject) [[unlikely]] {
      THROW_ERR_INVALID_ARG_VALUE(env, "Invalid subject distinguished name");
      return Nothing<void>();
    }
    subject = owned_subject.get();
    certificate_public_key = public_key.GetAsymmetricKey().get();
  }
  const bool subject_empty = X509_NAME_entry_count(subject) == 0;
  if (subject_empty && args[issuer_offset]->IsUndefined()) [[unlikely]] {
    THROW_ERR_INVALID_ARG_VALUE(
        env, "A self-signed X.509 certificate must have a non-empty subject");
    return Nothing<void>();
  }

  ArrayBufferOrViewContents<unsigned char> serial(args[serial_offset]);
  if (!serial.CheckSizeInt32()) [[unlikely]] {
    THROW_ERR_OUT_OF_RANGE(env, "X.509 generation input is too large");
    return Nothing<void>();
  }
  if (!IsValidSerial(serial.data(), serial.size())) [[unlikely]] {
    THROW_ERR_INVALID_ARG_VALUE(env, "Invalid certificate serial number");
    return Nothing<void>();
  }

  X509Pointer certificate = NewCertificate();
  if (!certificate) [[unlikely]] {
    THROW_ERR_CRYPTO_OPERATION_FAILED(
        env, "Failed to allocate an X.509 certificate");
    return Nothing<void>();
  }
  if (X509_set_version(certificate.get(), 2) != 1 ||
      X509_set_subject_name(certificate.get(), subject) != 1 ||
      X509_set_pubkey(certificate.get(), certificate_public_key) != 1)
      [[unlikely]] {
    THROW_ERR_CRYPTO_OPERATION_FAILED(
        env, "Failed to construct an X.509 certificate");
    return Nothing<void>();
  }

  const X509* signing_certificate = certificate.get();
  if (args[issuer_offset]->IsUndefined()) {
    if (X509_set_issuer_name(certificate.get(), subject) != 1) [[unlikely]] {
      THROW_ERR_CRYPTO_OPERATION_FAILED(env,
                                        "Failed to set the X.509 issuer name");
      return Nothing<void>();
    }
  } else {
    X509Certificate* issuer;
    ASSIGN_OR_RETURN_UNWRAP(
        &issuer, args[issuer_offset].As<Object>(), Nothing<void>());
    auto* issuer_name = X509_get_subject_name(issuer->get());
    if (issuer_name == nullptr || X509_NAME_entry_count(issuer_name) <= 0) {
      THROW_ERR_INVALID_ARG_VALUE(
          env,
          "The \"options.issuer\" certificate must have a non-empty subject");
      return Nothing<void>();
    }
    if (X509_set_issuer_name(certificate.get(), issuer_name) != 1)
        [[unlikely]] {
      THROW_ERR_CRYPTO_OPERATION_FAILED(env,
                                        "Failed to set the X.509 issuer name");
      return Nothing<void>();
    }
    signing_certificate = issuer->get();
  }

  switch (ncrypto::X509View(signing_certificate)
              .checkPrivateKey(signing_key.GetAsymmetricKey())) {
    case ncrypto::X509View::CheckKeyResult::MATCH:
      break;
    case ncrypto::X509View::CheckKeyResult::NO_MATCH:
      THROW_ERR_INVALID_ARG_VALUE(
          env,
          "The \"options.signature.key\" argument does not match the "
          "certificate public key");
      return Nothing<void>();
    case ncrypto::X509View::CheckKeyResult::UNSUPPORTED_OPERATION:
      break;
    case ncrypto::X509View::CheckKeyResult::INVALID_CERTIFICATE:
    case ncrypto::X509View::CheckKeyResult::OPERATION_FAILED:
      THROW_ERR_CRYPTO_OPERATION_FAILED(env, "Failed to compare public keys");
      return Nothing<void>();
  }

  BignumPointer serial_value(serial.data(), serial.size());
  ASN1IntegerPointer serial_number(
      serial_value ? BN_to_ASN1_INTEGER(serial_value.get(), nullptr) : nullptr);
  if (!serial_number ||
      X509_set_serialNumber(certificate.get(), serial_number.get()) != 1)
      [[unlikely]] {
    THROW_ERR_CRYPTO_OPERATION_FAILED(env,
                                      "Failed to set the X.509 serial number");
    return Nothing<void>();
  }

  Utf8Value not_before(env->isolate(), args[not_before_offset]);
  Utf8Value not_after(env->isolate(), args[not_after_offset]);
  if (*not_before == nullptr || *not_after == nullptr ||
      ASN1_TIME_set_string_X509(X509_getm_notBefore(certificate.get()),
                                *not_before) != 1 ||
      ASN1_TIME_set_string_X509(X509_getm_notAfter(certificate.get()),
                                *not_after) != 1) [[unlikely]] {
    THROW_ERR_INVALID_ARG_VALUE(env, "Invalid certificate validity time");
    return Nothing<void>();
  }

  Local<Array> extensions = args[extensions_offset].As<Array>();
  ASN1OctetStringPointer self_signed_subject_key_identifier;
  if (signing_certificate == certificate.get()) {
    for (uint32_t index = 0; index < extensions->Length(); index++) {
      Local<Value> entry_value;
      Local<Value> kind_value;
      if (!GetArrayEntry(context, extensions, index, &entry_value) ||
          !entry_value->IsArray() ||
          !GetArrayEntry(context, entry_value.As<Array>(), 0, &kind_value) ||
          !kind_value->IsUint32()) {
        if (env->isolate()->HasPendingException()) return Nothing<void>();
        THROW_ERR_INVALID_ARG_VALUE(env, "Invalid X.509 extension");
        return Nothing<void>();
      }
      if (static_cast<ExtensionKind>(kind_value.As<Uint32>()->Value()) !=
          ExtensionKind::kSubjectKeyIdentifier) {
        continue;
      }
      bool automatic;
      ASN1OctetStringPointer identifier;
      if (!ReadKeyIdentifierSpec(
              env, entry_value.As<Array>(), &automatic, &identifier)) {
        if (env->isolate()->HasPendingException()) return Nothing<void>();
        THROW_ERR_INVALID_ARG_VALUE(env, "Invalid X.509 extension");
        return Nothing<void>();
      }
      if (!automatic) {
        self_signed_subject_key_identifier = std::move(identifier);
      }
      break;
    }
  }

  std::unordered_set<std::string> extension_oids;
  ExtensionProfile profile;
  for (uint32_t index = 0; index < extensions->Length(); index++) {
    Local<Value> entry_value;
    if (!extensions->Get(context, index).ToLocal(&entry_value)) {
      return Nothing<void>();
    }
    CHECK(entry_value->IsArray());
    std::string oid;
    const int previous_extension_count = X509_get_ext_count(certificate.get());
    if (previous_extension_count < 0 ||
        !BuildAndAddExtension(env,
                              certificate.get(),
                              signing_certificate,
                              self_signed_subject_key_identifier.get(),
                              entry_value.As<Array>(),
                              &oid,
                              &profile)) [[unlikely]] {
      if (env->isolate()->HasPendingException()) return Nothing<void>();
      THROW_ERR_INVALID_ARG_VALUE(env, "Invalid X.509 extension");
      return Nothing<void>();
    }
    if (X509_get_ext_count(certificate.get()) != previous_extension_count + 1)
        [[unlikely]] {
      THROW_ERR_CRYPTO_UNSUPPORTED_OPERATION(
          env, "The crypto backend does not support this X.509 extension");
      return Nothing<void>();
    }
    if (!extension_oids.emplace(std::move(oid)).second) [[unlikely]] {
      THROW_ERR_INVALID_ARG_VALUE(env, "Duplicate X.509 extension OID");
      return Nothing<void>();
    }
  }
  if (!ValidateExtensionProfile(subject_empty, profile)) [[unlikely]] {
    THROW_ERR_INVALID_ARG_VALUE(env, "Invalid X.509 extension profile");
    return Nothing<void>();
  }

  params->certificate = std::move(certificate);
  params->request = std::move(request);
  params->request_public_key = std::move(owned_request_public_key);
  params->signing_key = std::move(signing_key);
  return JustVoid();
}

bool X509GenerateTraits::DeriveBits(Environment* env,
                                    const X509GenerateConfig& params,
                                    ByteSource* out,
                                    CryptoJobMode mode,
                                    CryptoErrorStore* errors) {
  static_cast<void>(env);
  static_cast<void>(mode);

  if (params.request) {
    int verification_result = 0;
    {
      MarkPopErrorOnReturn mark_pop_error_on_return;
      if (params.request_public_key) {
        verification_result = X509_REQ_verify(params.request.get(),
                                              params.request_public_key.get());
      }
    }
    if (verification_result != 1) [[unlikely]] {
      return RequestVerificationFailed(errors);
    }
  }

  X509* certificate = params.certificate.get();
  const EVPKeyPointer& signing_key = params.signing_key.GetAsymmetricKey();
  EVPMDCtxPointer context = EVPMDCtxPointer::New();
  if (!context) [[unlikely]]
    return GenerationFailed(errors);
  auto signing_context = context.signInit(signing_key, params.digest);
  if (!signing_context.has_value()) [[unlikely]] {
    return GenerationFailed(errors);
  }
  if (signing_key.isRsaVariant()) {
    const int padding =
        params.padding.value_or(signing_key.getDefaultSignPadding());
    if (!EVPKeyCtxPointer::setRsaPadding(
            *signing_context, padding, params.salt_length)) [[unlikely]] {
      return GenerationFailed(errors);
    }
  }
  if (X509_sign_ctx(certificate, context.get()) <= 0) [[unlikely]] {
    return GenerationFailed(errors);
  }

  *out = EncodeCertificate(certificate);
  return *out ? true : GenerationFailed(errors);
}

MaybeLocal<Value> X509GenerateTraits::EncodeOutput(
    Environment* env, const X509GenerateConfig& params, ByteSource* out) {
  static_cast<void>(params);
  return out->ToArrayBuffer(env);
}

void X509Generation::Initialize(Environment* env, Local<Object> target) {
  SetMethod(env->context(),
            target,
            "parseX509CertificateRequest",
            ParseX509CertificateRequest);
}

void X509Generation::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(ParseX509CertificateRequest);
}

}  // namespace crypto
}  // namespace node
