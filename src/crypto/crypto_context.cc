#include "crypto/crypto_context.h"
#include "base_object-inl.h"
#include "crypto/crypto_bio.h"
#include "crypto/crypto_common.h"
#include "crypto/crypto_util.h"
#include "env-inl.h"
#include "memory_tracker-inl.h"
#include "ncrypto.h"
#include "node.h"
#include "node_buffer.h"
#include "node_options.h"
#include "util.h"
#include "v8.h"

#include <openssl/x509.h>
#include <openssl/pkcs12.h>
#include <openssl/rand.h>
#ifndef OPENSSL_NO_ENGINE
#include <openssl/engine.h>
#endif  // !OPENSSL_NO_ENGINE
#ifdef __APPLE__
#include <Security/Security.h>
#endif

#ifdef _WIN32
#include <Windows.h>
#include <wincrypt.h>
#endif

namespace node {

using ncrypto::BignumPointer;
using ncrypto::BIOPointer;
using ncrypto::ClearErrorOnReturn;
using ncrypto::CryptoErrorList;
using ncrypto::DHPointer;
#ifndef OPENSSL_NO_ENGINE
using ncrypto::EnginePointer;
#endif  // !OPENSSL_NO_ENGINE
using ncrypto::EVPKeyPointer;
using ncrypto::MarkPopErrorOnReturn;
using ncrypto::SSLPointer;
using ncrypto::StackOfX509;
using ncrypto::X509Pointer;
using ncrypto::X509View;
using v8::Array;
using v8::ArrayBufferView;
using v8::Boolean;
using v8::Context;
using v8::DontDelete;
using v8::EscapableHandleScope;
using v8::Exception;
using v8::External;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Int32;
using v8::Integer;
using v8::Isolate;
using v8::JustVoid;
using v8::Local;
using v8::LocalVector;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Nothing;
using v8::Object;
using v8::PropertyAttribute;
using v8::ReadOnly;
using v8::Signature;
using v8::String;
using v8::Value;

namespace crypto {
static const char* const root_certs[] = {
#include "node_root_certs.h"  // NOLINT(build/include_order)
};

static const char system_cert_path[] = NODE_OPENSSL_SYSTEM_CERT_PATH;

static std::string extra_root_certs_file;  // NOLINT(runtime/string)

static std::atomic<bool> has_cached_bundled_root_certs{false};
static std::atomic<bool> has_cached_system_root_certs{false};
static std::atomic<bool> has_cached_extra_root_certs{false};

X509_STORE* GetOrCreateRootCertStore() {
  // Guaranteed thread-safe by standard, just don't use -fno-threadsafe-statics.
  static X509_STORE* store = NewRootCertStore();
  return store;
}

// Takes a string or buffer and loads it into a BIO.
// Caller responsible for BIO_free_all-ing the returned object.
BIOPointer LoadBIO(Environment* env, Local<Value> v) {
  if (v->IsString() || v->IsArrayBufferView()) {
    auto bio = BIOPointer::NewSecMem();
    if (!bio) return {};
    ByteSource bsrc = ByteSource::FromStringOrBuffer(env, v);
    if (bsrc.size() > INT_MAX) return {};
    int written = BIOPointer::Write(
        &bio, std::string_view(bsrc.data<char>(), bsrc.size()));
    if (written < 0) return {};
    if (static_cast<size_t>(written) != bsrc.size()) return {};
    return bio;
  }
  return {};
}

namespace {
int SSL_CTX_use_certificate_chain(SSL_CTX* ctx,
                                  X509Pointer&& x,
                                  STACK_OF(X509)* extra_certs,
                                  X509Pointer* cert,
                                  X509Pointer* issuer_) {
  CHECK(!*issuer_);
  CHECK(!*cert);
  X509* issuer = nullptr;

  int ret = SSL_CTX_use_certificate(ctx, x.get());

  if (ret) {
    // If we could set up our certificate, now proceed to
    // the CA certificates.
    SSL_CTX_clear_extra_chain_certs(ctx);

    for (int i = 0; i < sk_X509_num(extra_certs); i++) {
      X509* ca = sk_X509_value(extra_certs, i);

      // NOTE: Increments reference count on `ca`
      if (!SSL_CTX_add1_chain_cert(ctx, ca)) {
        ret = 0;
        issuer = nullptr;
        break;
      }
      // Note that we must not free r if it was successfully
      // added to the chain (while we must free the main
      // certificate, since its reference count is increased
      // by SSL_CTX_use_certificate).

      // Find issuer
      if (issuer != nullptr || X509_check_issued(ca, x.get()) != X509_V_OK)
        continue;

      issuer = ca;
    }
  }

  // Try getting issuer from a cert store
  if (ret) {
    if (issuer == nullptr) {
      // TODO(tniessen): SSL_CTX_get_issuer does not allow the caller to
      // distinguish between a failed operation and an empty result. Fix that
      // and then handle the potential error properly here (set ret to 0).
      *issuer_ = X509Pointer::IssuerFrom(ctx, x.view());
      // NOTE: get_cert_store doesn't increment reference count,
      // no need to free `store`
    } else {
      // Increment issuer reference count
      issuer_->reset(X509_dup(issuer));
      if (!*issuer_) {
        ret = 0;
      }
    }
  }

  if (ret && x != nullptr) {
    cert->reset(X509_dup(x.get()));
    if (!*cert)
      ret = 0;
  }
  return ret;
}

}  // namespace

// Read a file that contains our certificate in "PEM" format,
// possibly followed by a sequence of CA certificates that should be
// sent to the peer in the Certificate message.
//
// Taken from OpenSSL - edited for style.
int SSL_CTX_use_certificate_chain(SSL_CTX* ctx,
                                  BIOPointer&& in,
                                  X509Pointer* cert,
                                  X509Pointer* issuer) {
  // Just to ensure that `ERR_peek_last_error` below will return only errors
  // that we are interested in
  ERR_clear_error();

  X509Pointer x(
      PEM_read_bio_X509_AUX(in.get(), nullptr, NoPasswordCallback, nullptr));

  if (!x)
    return 0;

  unsigned long err = 0;  // NOLINT(runtime/int)

  StackOfX509 extra_certs(sk_X509_new_null());
  if (!extra_certs)
    return 0;

  while (X509Pointer extra {PEM_read_bio_X509(in.get(),
                                    nullptr,
                                    NoPasswordCallback,
                                    nullptr)}) {
    if (sk_X509_push(extra_certs.get(), extra.get())) {
      extra.release();
      continue;
    }

    return 0;
  }

  // When the while loop ends, it's usually just EOF.
  err = ERR_peek_last_error();
  if (ERR_GET_LIB(err) == ERR_LIB_PEM &&
      ERR_GET_REASON(err) == PEM_R_NO_START_LINE) {
    ERR_clear_error();
  } else {
    // some real error
    return 0;
  }

  return SSL_CTX_use_certificate_chain(ctx,
                                       std::move(x),
                                       extra_certs.get(),
                                       cert,
                                       issuer);
}

static unsigned long LoadCertsFromFile(  // NOLINT(runtime/int)
    std::vector<X509*>* certs,
    const char* file) {
  MarkPopErrorOnReturn mark_pop_error_on_return;

  auto bio = BIOPointer::NewFile(file, "r");
  if (!bio) return ERR_get_error();

  while (X509* x509 = PEM_read_bio_X509(
             bio.get(), nullptr, NoPasswordCallback, nullptr)) {
    certs->push_back(x509);
  }

  unsigned long err = ERR_peek_last_error();  // NOLINT(runtime/int)
  // Ignore error if its EOF/no start line found.
  if (ERR_GET_LIB(err) == ERR_LIB_PEM &&
      ERR_GET_REASON(err) == PEM_R_NO_START_LINE) {
    return 0;
  } else {
    return err;
  }
}

// Indicates the trust status of a certificate.
enum class TrustStatus {
  // Trust status is unknown / uninitialized.
  UNKNOWN,
  // Certificate inherits trust value from its issuer. If the certificate is the
  // root of the chain, this implies distrust.
  UNSPECIFIED,
  // Certificate is a trust anchor.
  TRUSTED,
  // Certificate is blocked / explicitly distrusted.
  DISTRUSTED
};

bool isSelfIssued(X509* cert) {
  auto subject = X509_get_subject_name(cert);
  auto issuer = X509_get_issuer_name(cert);

  return X509_NAME_cmp(subject, issuer) == 0;
}

// The following code is loosely based on
// https://github.com/chromium/chromium/blob/54bd8e3/net/cert/internal/trust_store_mac.cc
// and
// https://github.com/chromium/chromium/blob/0192587/net/cert/internal/trust_store_win.cc
// Copyright 2015 The Chromium Authors
// Licensed under a BSD-style license
// See https://chromium.googlesource.com/chromium/src/+/HEAD/LICENSE for
// details.
#ifdef __APPLE__
TrustStatus IsTrustDictionaryTrustedForPolicy(CFDictionaryRef trust_dict,
                                              bool is_self_issued) {
  // Trust settings may be scoped to a single application
  // skip as this is not supported
  if (CFDictionaryContainsKey(trust_dict, kSecTrustSettingsApplication)) {
    return TrustStatus::UNSPECIFIED;
  }

  // Trust settings may be scoped using policy-specific constraints. For
  // example, SSL trust settings might be scoped to a single hostname, or EAP
  // settings specific to a particular WiFi network.
  // As this is not presently supported, skip any policy-specific trust
  // settings.
  if (CFDictionaryContainsKey(trust_dict, kSecTrustSettingsPolicyString)) {
    return TrustStatus::UNSPECIFIED;
  }

  // If the trust settings are scoped to a specific policy (via
  // kSecTrustSettingsPolicy), ensure that the policy is the same policy as
  // |kSecPolicyAppleSSL|. If there is no kSecTrustSettingsPolicy key, it's
  // considered a match for all policies.
  if (CFDictionaryContainsKey(trust_dict, kSecTrustSettingsPolicy)) {
    SecPolicyRef policy_ref = reinterpret_cast<SecPolicyRef>(const_cast<void*>(
        CFDictionaryGetValue(trust_dict, kSecTrustSettingsPolicy)));

    if (!policy_ref) {
      return TrustStatus::UNSPECIFIED;
    }

    CFDictionaryRef policy_dict(SecPolicyCopyProperties(policy_ref));

    // kSecPolicyOid is guaranteed to be present in the policy dictionary.
    CFStringRef policy_oid = reinterpret_cast<CFStringRef>(
        const_cast<void*>(CFDictionaryGetValue(policy_dict, kSecPolicyOid)));

    if (!CFEqual(policy_oid, kSecPolicyAppleSSL)) {
      return TrustStatus::UNSPECIFIED;
    }
  }

  int trust_settings_result = kSecTrustSettingsResultTrustRoot;
  if (CFDictionaryContainsKey(trust_dict, kSecTrustSettingsResult)) {
    CFNumberRef trust_settings_result_ref =
        reinterpret_cast<CFNumberRef>(const_cast<void*>(
            CFDictionaryGetValue(trust_dict, kSecTrustSettingsResult)));

    if (!trust_settings_result_ref ||
        !CFNumberGetValue(trust_settings_result_ref,
                          kCFNumberIntType,
                          &trust_settings_result)) {
      return TrustStatus::UNSPECIFIED;
    }

    if (trust_settings_result == kSecTrustSettingsResultDeny) {
      return TrustStatus::DISTRUSTED;
    }

    // This is a bit of a hack: if the cert is self-issued allow either
    // kSecTrustSettingsResultTrustRoot or kSecTrustSettingsResultTrustAsRoot on
    // the basis that SecTrustSetTrustSettings should not allow creating an
    // invalid trust record in the first place. (The spec is that
    // kSecTrustSettingsResultTrustRoot can only be applied to root(self-signed)
    // certs and kSecTrustSettingsResultTrustAsRoot is used for other certs.)
    // This hack avoids having to check the signature on the cert which is slow
    // if using the platform APIs, and may require supporting MD5 signature
    // algorithms on some older OSX versions or locally added roots, which is
    // undesirable in the built-in signature verifier.
    if (is_self_issued) {
      return trust_settings_result == kSecTrustSettingsResultTrustRoot ||
                     trust_settings_result == kSecTrustSettingsResultTrustAsRoot
                 ? TrustStatus::TRUSTED
                 : TrustStatus::UNSPECIFIED;
    }

    // kSecTrustSettingsResultTrustAsRoot can only be applied to non-root certs.
    return (trust_settings_result == kSecTrustSettingsResultTrustAsRoot)
               ? TrustStatus::TRUSTED
               : TrustStatus::UNSPECIFIED;
  }

  return TrustStatus::UNSPECIFIED;
}

TrustStatus IsTrustSettingsTrustedForPolicy(CFArrayRef trust_settings,
                                            bool is_self_issued) {
  // The trust_settings parameter can return a valid but empty CFArrayRef.
  // This empty trust-settings array means “always trust this certificate”
  // with an overall trust setting for the certificate of
  // kSecTrustSettingsResultTrustRoot
  if (CFArrayGetCount(trust_settings) == 0) {
    return is_self_issued ? TrustStatus::TRUSTED : TrustStatus::UNSPECIFIED;
  }

  for (CFIndex i = 0; i < CFArrayGetCount(trust_settings); ++i) {
    CFDictionaryRef trust_dict = reinterpret_cast<CFDictionaryRef>(
        const_cast<void*>(CFArrayGetValueAtIndex(trust_settings, i)));

    TrustStatus trust =
        IsTrustDictionaryTrustedForPolicy(trust_dict, is_self_issued);

    if (trust == TrustStatus::DISTRUSTED || trust == TrustStatus::TRUSTED) {
      return trust;
    }
  }
  return TrustStatus::UNSPECIFIED;
}

bool IsCertificateTrustValid(SecCertificateRef ref) {
  SecTrustRef sec_trust = nullptr;
  CFMutableArrayRef subj_certs =
      CFArrayCreateMutable(nullptr, 1, &kCFTypeArrayCallBacks);
  CFArraySetValueAtIndex(subj_certs, 0, ref);

  SecPolicyRef policy = SecPolicyCreateSSL(false, nullptr);
  OSStatus ortn =
      SecTrustCreateWithCertificates(subj_certs, policy, &sec_trust);
  bool result = false;
  if (ortn) {
    /* should never happen */
  } else {
    result = SecTrustEvaluateWithError(sec_trust, nullptr);
  }

  if (policy) {
    CFRelease(policy);
  }
  if (sec_trust) {
    CFRelease(sec_trust);
  }
  if (subj_certs) {
    CFRelease(subj_certs);
  }
  return result;
}

bool IsCertificateTrustedForPolicy(X509* cert, SecCertificateRef ref) {
  OSStatus err;

  bool trust_evaluated = false;
  bool is_self_issued = isSelfIssued(cert);

  // Evaluate user trust domain, then admin. User settings can override
  // admin (and both override the system domain, but we don't check that).
  for (const auto& trust_domain :
       {kSecTrustSettingsDomainUser, kSecTrustSettingsDomainAdmin}) {
    CFArrayRef trust_settings = nullptr;
    err = SecTrustSettingsCopyTrustSettings(ref, trust_domain, &trust_settings);

    if (err != errSecSuccess && err != errSecItemNotFound) {
      fprintf(stderr,
              "ERROR: failed to copy trust settings of system certificate%d\n",
              err);
      continue;
    }

    if (err == errSecSuccess && trust_settings != nullptr) {
      TrustStatus result =
          IsTrustSettingsTrustedForPolicy(trust_settings, is_self_issued);
      if (result != TrustStatus::UNSPECIFIED) {
        CFRelease(trust_settings);
        return result == TrustStatus::TRUSTED;
      }
    }

    // An empty trust settings array isn’t the same as no trust settings,
    // where the trust_settings parameter returns NULL.
    // No trust-settings array means
    // “this certificate must be verifiable using a known trusted certificate”.
    if (trust_settings == nullptr && !trust_evaluated) {
      bool result = IsCertificateTrustValid(ref);
      if (result) {
        return true;
      }
      // no point re-evaluating this in the admin domain
      trust_evaluated = true;
    } else if (trust_settings) {
      CFRelease(trust_settings);
    }
  }
  return false;
}

void ReadMacOSKeychainCertificates(
    std::vector<X509*>* system_root_certificates_X509) {
  CFTypeRef search_keys[] = {kSecClass, kSecMatchLimit, kSecReturnRef};
  CFTypeRef search_values[] = {
      kSecClassCertificate, kSecMatchLimitAll, kCFBooleanTrue};
  CFDictionaryRef search = CFDictionaryCreate(kCFAllocatorDefault,
                                              search_keys,
                                              search_values,
                                              3,
                                              &kCFTypeDictionaryKeyCallBacks,
                                              &kCFTypeDictionaryValueCallBacks);

  CFArrayRef curr_anchors = nullptr;
  OSStatus ortn =
      SecItemCopyMatching(search, reinterpret_cast<CFTypeRef*>(&curr_anchors));
  CFRelease(search);

  if (ortn) {
    fprintf(stderr, "ERROR: SecItemCopyMatching failed %d\n", ortn);
  }

  CFIndex count = CFArrayGetCount(curr_anchors);

  for (int i = 0; i < count; ++i) {
    SecCertificateRef cert_ref = reinterpret_cast<SecCertificateRef>(
        const_cast<void*>(CFArrayGetValueAtIndex(curr_anchors, i)));

    CFDataRef der_data = SecCertificateCopyData(cert_ref);
    if (!der_data) {
      fprintf(stderr, "ERROR: SecCertificateCopyData failed\n");
      continue;
    }
    auto data_buffer_pointer = CFDataGetBytePtr(der_data);

    X509* cert =
        d2i_X509(nullptr, &data_buffer_pointer, CFDataGetLength(der_data));
    CFRelease(der_data);
    bool is_valid = IsCertificateTrustedForPolicy(cert, cert_ref);
    if (is_valid) {
      system_root_certificates_X509->emplace_back(cert);
    }
  }
  CFRelease(curr_anchors);
}
#endif  // __APPLE__

#ifdef _WIN32

// Returns true if the cert can be used for server authentication, based on
// certificate properties.
//
// While there are a variety of certificate properties that can affect how
// trust is computed, the main property is CERT_ENHKEY_USAGE_PROP_ID, which
// is intersected with the certificate's EKU extension (if present).
// The intersection is documented in the Remarks section of
// CertGetEnhancedKeyUsage, and is as follows:
// - No EKU property, and no EKU extension = Trusted for all purpose
// - Either an EKU property, or EKU extension, but not both = Trusted only
//   for the listed purposes
// - Both an EKU property and an EKU extension = Trusted for the set
//   intersection of the listed purposes
// CertGetEnhancedKeyUsage handles this logic, and if an empty set is
// returned, the distinction between the first and third case can be
// determined by GetLastError() returning CRYPT_E_NOT_FOUND.
//
// See:
// https://docs.microsoft.com/en-us/windows/win32/api/wincrypt/nf-wincrypt-certgetenhancedkeyusage
//
// If we run into any errors reading the certificate properties, we fail
// closed.
bool IsCertTrustedForServerAuth(PCCERT_CONTEXT cert) {
  DWORD usage_size = 0;

  if (!CertGetEnhancedKeyUsage(cert, 0, nullptr, &usage_size)) {
    return false;
  }

  std::vector<BYTE> usage_bytes(usage_size);
  CERT_ENHKEY_USAGE* usage =
      reinterpret_cast<CERT_ENHKEY_USAGE*>(usage_bytes.data());
  if (!CertGetEnhancedKeyUsage(cert, 0, usage, &usage_size)) {
    return false;
  }

  if (usage->cUsageIdentifier == 0) {
    // check GetLastError
    HRESULT error_code = GetLastError();

    switch (error_code) {
      case CRYPT_E_NOT_FOUND:
        return true;
      case S_OK:
        return false;
      default:
        return false;
    }
  }

  // SAFETY: `usage->rgpszUsageIdentifier` is an array of LPSTR (pointer to null
  // terminated string) of length `usage->cUsageIdentifier`.
  for (DWORD i = 0; i < usage->cUsageIdentifier; ++i) {
    std::string_view eku(usage->rgpszUsageIdentifier[i]);
    if ((eku == szOID_PKIX_KP_SERVER_AUTH) ||
        (eku == szOID_ANY_ENHANCED_KEY_USAGE)) {
      return true;
    }
  }

  return false;
}

void GatherCertsForLocation(std::vector<X509*>* vector,
                            DWORD location,
                            LPCWSTR store_name) {
  if (!(location == CERT_SYSTEM_STORE_LOCAL_MACHINE ||
        location == CERT_SYSTEM_STORE_LOCAL_MACHINE_GROUP_POLICY ||
        location == CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE ||
        location == CERT_SYSTEM_STORE_CURRENT_USER ||
        location == CERT_SYSTEM_STORE_CURRENT_USER_GROUP_POLICY)) {
    return;
  }

  DWORD flags =
      location | CERT_STORE_OPEN_EXISTING_FLAG | CERT_STORE_READONLY_FLAG;

  HCERTSTORE opened_store(
      CertOpenStore(CERT_STORE_PROV_SYSTEM,
                    0,
                    // The Windows API only accepts NULL for hCryptProv.
                    NULL, /* NOLINT (readability/null_usage) */
                    flags,
                    store_name));
  if (!opened_store) {
    return;
  }

  auto cleanup = OnScopeLeave(
      [opened_store]() { CHECK_EQ(CertCloseStore(opened_store, 0), TRUE); });

  PCCERT_CONTEXT cert_from_store = nullptr;
  while ((cert_from_store = CertEnumCertificatesInStore(
              opened_store, cert_from_store)) != nullptr) {
    if (!IsCertTrustedForServerAuth(cert_from_store)) {
      continue;
    }
    const unsigned char* cert_data =
        reinterpret_cast<const unsigned char*>(cert_from_store->pbCertEncoded);
    const size_t cert_size = cert_from_store->cbCertEncoded;

    vector->emplace_back(d2i_X509(nullptr, &cert_data, cert_size));
  }
}

void ReadWindowsCertificates(
    std::vector<X509*>* system_root_certificates_X509) {
  // TODO(joyeecheung): match Chromium's policy, collect more certificates
  // from user-added CAs and support disallowed (revoked) certificates.

  // Grab the user-added roots.
  GatherCertsForLocation(
      system_root_certificates_X509, CERT_SYSTEM_STORE_LOCAL_MACHINE, L"ROOT");
  GatherCertsForLocation(system_root_certificates_X509,
                         CERT_SYSTEM_STORE_LOCAL_MACHINE_GROUP_POLICY,
                         L"ROOT");
  GatherCertsForLocation(system_root_certificates_X509,
                         CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE,
                         L"ROOT");
  GatherCertsForLocation(
      system_root_certificates_X509, CERT_SYSTEM_STORE_CURRENT_USER, L"ROOT");
  GatherCertsForLocation(system_root_certificates_X509,
                         CERT_SYSTEM_STORE_CURRENT_USER_GROUP_POLICY,
                         L"ROOT");

  // Grab the intermediate certs
  GatherCertsForLocation(
      system_root_certificates_X509, CERT_SYSTEM_STORE_LOCAL_MACHINE, L"CA");
  GatherCertsForLocation(system_root_certificates_X509,
                         CERT_SYSTEM_STORE_LOCAL_MACHINE_GROUP_POLICY,
                         L"CA");
  GatherCertsForLocation(system_root_certificates_X509,
                         CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE,
                         L"CA");
  GatherCertsForLocation(
      system_root_certificates_X509, CERT_SYSTEM_STORE_CURRENT_USER, L"CA");
  GatherCertsForLocation(system_root_certificates_X509,
                         CERT_SYSTEM_STORE_CURRENT_USER_GROUP_POLICY,
                         L"CA");

  // Grab the user-added trusted server certs. Trusted end-entity certs are
  // only allowed for server auth in the "local machine" store, but not in the
  // "current user" store.
  GatherCertsForLocation(system_root_certificates_X509,
                         CERT_SYSTEM_STORE_LOCAL_MACHINE,
                         L"TrustedPeople");
  GatherCertsForLocation(system_root_certificates_X509,
                         CERT_SYSTEM_STORE_LOCAL_MACHINE_GROUP_POLICY,
                         L"TrustedPeople");
  GatherCertsForLocation(system_root_certificates_X509,
                         CERT_SYSTEM_STORE_LOCAL_MACHINE_ENTERPRISE,
                         L"TrustedPeople");
}
#endif

static void LoadCertsFromDir(std::vector<X509*>* certs,
                             std::string_view cert_dir) {
  uv_fs_t dir_req;
  auto cleanup = OnScopeLeave([&dir_req]() { uv_fs_req_cleanup(&dir_req); });
  int err = uv_fs_scandir(nullptr, &dir_req, cert_dir.data(), 0, nullptr);
  if (err < 0) {
    fprintf(stderr,
            "Cannot open directory %s to load OpenSSL certificates.\n",
            cert_dir.data());
    return;
  }

  for (;;) {
    uv_dirent_t ent;

    int r = uv_fs_scandir_next(&dir_req, &ent);
    if (r == UV_EOF) {
      break;
    }
    if (r < 0) {
      char message[64];
      uv_strerror_r(r, message, sizeof(message));
      fprintf(stderr,
              "Cannot scan directory %s to load OpenSSL certificates.\n",
              cert_dir.data());
      return;
    }

    uv_fs_t stats_req;
    std::string file_path = std::string(cert_dir) + "/" + ent.name;
    int stats_r = uv_fs_stat(nullptr, &stats_req, file_path.c_str(), nullptr);
    if (stats_r == 0 &&
        (static_cast<uv_stat_t*>(stats_req.ptr)->st_mode & S_IFREG)) {
      LoadCertsFromFile(certs, file_path.c_str());
    }
    uv_fs_req_cleanup(&stats_req);
  }
}

// Loads CA certificates from the default certificate paths respected by
// OpenSSL.
void GetOpenSSLSystemCertificates(std::vector<X509*>* system_store_certs) {
  std::string cert_file;
  // While configurable when OpenSSL is built, this is usually SSL_CERT_FILE.
  if (!credentials::SafeGetenv(X509_get_default_cert_file_env(), &cert_file)) {
    // This is usually /etc/ssl/cert.pem if we are using the OpenSSL statically
    // linked and built with default configurations.
    cert_file = X509_get_default_cert_file();
  }

  std::string cert_dir;
  // While configurable when OpenSSL is built, this is usually SSL_CERT_DIR.
  if (!credentials::SafeGetenv(X509_get_default_cert_dir_env(), &cert_dir)) {
    // This is usually /etc/ssl/certs if we are using the OpenSSL statically
    // linked and built with default configurations.
    cert_dir = X509_get_default_cert_dir();
  }

  if (!cert_file.empty()) {
    LoadCertsFromFile(system_store_certs, cert_file.c_str());
  }

  if (!cert_dir.empty()) {
    LoadCertsFromDir(system_store_certs, cert_dir.c_str());
  }
}

static std::vector<X509*> InitializeBundledRootCertificates() {
  // Read the bundled certificates in node_root_certs.h into
  // bundled_root_certs_vector.
  std::vector<X509*> bundled_root_certs;
  size_t bundled_root_cert_count = arraysize(root_certs);
  bundled_root_certs.reserve(bundled_root_cert_count);
  for (size_t i = 0; i < bundled_root_cert_count; i++) {
    X509* x509 = PEM_read_bio_X509(
        NodeBIO::NewFixed(root_certs[i], strlen(root_certs[i])).get(),
        nullptr,  // no re-use of X509 structure
        NoPasswordCallback,
        nullptr);  // no callback data

    // Parse errors from the built-in roots are fatal.
    CHECK_NOT_NULL(x509);

    bundled_root_certs.push_back(x509);
  }
  return bundled_root_certs;
}

// TODO(joyeecheung): it is a bit excessive to do this PEM -> X509
// dance when we could've just pass everything around in binary. Change the
// root_certs to be embedded as DER so that we can save the serialization
// and deserialization.
static std::vector<X509*>& GetBundledRootCertificates() {
  // Use function-local static to guarantee thread safety.
  static std::vector<X509*> bundled_root_certs =
      InitializeBundledRootCertificates();
  has_cached_bundled_root_certs.store(true);
  return bundled_root_certs;
}

static std::vector<X509*> InitializeSystemStoreCertificates() {
  std::vector<X509*> system_store_certs;
#ifdef __APPLE__
  ReadMacOSKeychainCertificates(&system_store_certs);
#endif
#ifdef _WIN32
  ReadWindowsCertificates(&system_store_certs);
#endif
#if !defined(__APPLE__) && !defined(_WIN32)
  GetOpenSSLSystemCertificates(&system_store_certs);
#endif
  return system_store_certs;
}

static std::vector<X509*>& GetSystemStoreCACertificates() {
  // Use function-local static to guarantee thread safety.
  static std::vector<X509*> system_store_certs =
      InitializeSystemStoreCertificates();
  has_cached_system_root_certs.store(true);
  return system_store_certs;
}

static std::vector<X509*> InitializeExtraCACertificates() {
  std::vector<X509*> extra_certs;
  unsigned long err = LoadCertsFromFile(  // NOLINT(runtime/int)
      &extra_certs,
      extra_root_certs_file.c_str());
  if (err) {
    char buf[256];
    ERR_error_string_n(err, buf, sizeof(buf));
    fprintf(stderr,
            "Warning: Ignoring extra certs from `%s`, load failed: %s\n",
            extra_root_certs_file.c_str(),
            buf);
  }
  return extra_certs;
}

static std::vector<X509*>& GetExtraCACertificates() {
  // Use function-local static to guarantee thread safety.
  static std::vector<X509*> extra_certs = InitializeExtraCACertificates();
  has_cached_extra_root_certs.store(true);
  return extra_certs;
}

// Due to historical reasons the various options of CA certificates
// may invalid one another. The current rule is:
// 1. If the configure-time option --openssl-use-def-ca-store is NOT used
//    (default):
//    a. If the runtime option --use-openssl-ca is used, load the
//       CA certificates from the default locations respected by OpenSSL.
//    b. Otherwise, --use-bundled-ca is assumed to be the default, and we
//       use the bundled CA certificates.
// 2. If the configure-time option --openssl-use-def-ca-store IS used,
//    --use-openssl-ca is assumed to be the default, with the default
//    location set to the path specified by the configure-time option.
// 3. --use-openssl-ca and --use-bundled-ca are mutually exclusive.
// 4. --use-openssl-ca and --use-system-ca are mutually exclusive.
// 5. --use-bundled-ca and --use-system-ca can be used together.
//    The certificates can be combined.
// 6. Independent of all other flags, NODE_EXTRA_CA_CERTS always
//    adds extra certificates from the specified path, so it works
//    with all the other flags.
// 7. Certificates from --use-bundled-ca, --use-system-ca and
//    NODE_EXTRA_CA_CERTS are cached after first load. Certificates
//    from --use-system-ca are not cached and always reloaded from
//    disk.
// TODO(joyeecheung): maybe these rules need a bit of consolidation?
X509_STORE* NewRootCertStore() {
  X509_STORE* store = X509_STORE_new();
  CHECK_NOT_NULL(store);

  if (*system_cert_path != '\0') {
    ERR_set_mark();
    X509_STORE_load_locations(store, system_cert_path, nullptr);
    ERR_pop_to_mark();
  }

  Mutex::ScopedLock cli_lock(node::per_process::cli_options_mutex);
  if (per_process::cli_options->ssl_openssl_cert_store) {
    CHECK_EQ(1, X509_STORE_set_default_paths(store));
  } else {
    for (X509* cert : GetBundledRootCertificates()) {
      CHECK_EQ(1, X509_STORE_add_cert(store, cert));
    }
    if (per_process::cli_options->use_system_ca) {
      for (X509* cert : GetSystemStoreCACertificates()) {
        CHECK_EQ(1, X509_STORE_add_cert(store, cert));
      }
    }
  }

  if (!extra_root_certs_file.empty()) {
    for (X509* cert : GetExtraCACertificates()) {
      CHECK_EQ(1, X509_STORE_add_cert(store, cert));
    }
  }

  return store;
}

void CleanupCachedRootCertificates() {
  if (has_cached_bundled_root_certs.load()) {
    for (X509* cert : GetBundledRootCertificates()) {
      X509_free(cert);
    }
  }
  if (has_cached_system_root_certs.load()) {
    for (X509* cert : GetSystemStoreCACertificates()) {
      X509_free(cert);
    }
  }

  if (has_cached_extra_root_certs.load()) {
    for (X509* cert : GetExtraCACertificates()) {
      X509_free(cert);
    }
  }
}

void GetBundledRootCertificates(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Value> result[arraysize(root_certs)];

  for (size_t i = 0; i < arraysize(root_certs); i++) {
    if (!String::NewFromOneByte(
            env->isolate(),
            reinterpret_cast<const uint8_t*>(root_certs[i]))
            .ToLocal(&result[i])) {
      return;
    }
  }

  args.GetReturnValue().Set(
      Array::New(env->isolate(), result, arraysize(root_certs)));
}

MaybeLocal<Array> X509sToArrayOfStrings(Environment* env,
                                        const std::vector<X509*>& certs) {
  ClearErrorOnReturn clear_error_on_return;
  EscapableHandleScope scope(env->isolate());

  LocalVector<Value> result(env->isolate(), certs.size());
  for (size_t i = 0; i < certs.size(); ++i) {
    X509View view(certs[i]);
    auto pem_bio = view.toPEM();
    if (!pem_bio) {
      ThrowCryptoError(env, ERR_get_error(), "X509 to PEM conversion");
      return MaybeLocal<Array>();
    }

    char* pem_data = nullptr;
    auto pem_size = BIO_get_mem_data(pem_bio.get(), &pem_data);
    if (pem_size <= 0 || !pem_data) {
      ThrowCryptoError(env, ERR_get_error(), "Reading PEM data");
      return MaybeLocal<Array>();
    }
    // PEM is base64-encoded, so it must be one-byte.
    if (!String::NewFromOneByte(env->isolate(),
                                reinterpret_cast<uint8_t*>(pem_data),
                                v8::NewStringType::kNormal,
                                pem_size)
             .ToLocal(&result[i])) {
      return MaybeLocal<Array>();
    }
  }
  return scope.Escape(Array::New(env->isolate(), result.data(), result.size()));
}

void GetSystemCACertificates(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Local<Array> results;
  if (X509sToArrayOfStrings(env, GetSystemStoreCACertificates())
          .ToLocal(&results)) {
    args.GetReturnValue().Set(results);
  }
}

void GetExtraCACertificates(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (extra_root_certs_file.empty()) {
    return args.GetReturnValue().Set(Array::New(env->isolate()));
  }
  Local<Array> results;
  if (X509sToArrayOfStrings(env, GetExtraCACertificates()).ToLocal(&results)) {
    args.GetReturnValue().Set(results);
  }
}

bool SecureContext::HasInstance(Environment* env, const Local<Value>& value) {
  return GetConstructorTemplate(env)->HasInstance(value);
}

Local<FunctionTemplate> SecureContext::GetConstructorTemplate(
    Environment* env) {
  Local<FunctionTemplate> tmpl = env->secure_context_constructor_template();
  if (tmpl.IsEmpty()) {
    Isolate* isolate = env->isolate();
    tmpl = NewFunctionTemplate(isolate, New);
    tmpl->InstanceTemplate()->SetInternalFieldCount(
        SecureContext::kInternalFieldCount);
    tmpl->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "SecureContext"));

    SetProtoMethod(isolate, tmpl, "init", Init);
    SetProtoMethod(isolate, tmpl, "setKey", SetKey);
    SetProtoMethod(isolate, tmpl, "setCert", SetCert);
    SetProtoMethod(isolate, tmpl, "addCACert", AddCACert);
    SetProtoMethod(
        isolate, tmpl, "setAllowPartialTrustChain", SetAllowPartialTrustChain);
    SetProtoMethod(isolate, tmpl, "addCRL", AddCRL);
    SetProtoMethod(isolate, tmpl, "addRootCerts", AddRootCerts);
    SetProtoMethod(isolate, tmpl, "setCipherSuites", SetCipherSuites);
    SetProtoMethod(isolate, tmpl, "setCiphers", SetCiphers);
    SetProtoMethod(isolate, tmpl, "setSigalgs", SetSigalgs);
    SetProtoMethod(isolate, tmpl, "setECDHCurve", SetECDHCurve);
    SetProtoMethod(isolate, tmpl, "setDHParam", SetDHParam);
    SetProtoMethod(isolate, tmpl, "setMaxProto", SetMaxProto);
    SetProtoMethod(isolate, tmpl, "setMinProto", SetMinProto);
    SetProtoMethod(isolate, tmpl, "getMaxProto", GetMaxProto);
    SetProtoMethod(isolate, tmpl, "getMinProto", GetMinProto);
    SetProtoMethod(isolate, tmpl, "setOptions", SetOptions);
    SetProtoMethod(isolate, tmpl, "setSessionIdContext", SetSessionIdContext);
    SetProtoMethod(isolate, tmpl, "setSessionTimeout", SetSessionTimeout);
    SetProtoMethod(isolate, tmpl, "close", Close);
    SetProtoMethod(isolate, tmpl, "loadPKCS12", LoadPKCS12);
    SetProtoMethod(isolate, tmpl, "setTicketKeys", SetTicketKeys);
    SetProtoMethod(
        isolate, tmpl, "enableTicketKeyCallback", EnableTicketKeyCallback);

    SetProtoMethodNoSideEffect(isolate, tmpl, "getTicketKeys", GetTicketKeys);
    SetProtoMethodNoSideEffect(
        isolate, tmpl, "getCertificate", GetCertificate<true>);
    SetProtoMethodNoSideEffect(
        isolate, tmpl, "getIssuer", GetCertificate<false>);

#ifndef OPENSSL_NO_ENGINE
    SetProtoMethod(isolate, tmpl, "setEngineKey", SetEngineKey);
    SetProtoMethod(isolate, tmpl, "setClientCertEngine", SetClientCertEngine);
#endif  // !OPENSSL_NO_ENGINE

#define SET_INTEGER_CONSTANTS(name, value)                                     \
  tmpl->Set(FIXED_ONE_BYTE_STRING(isolate, name),                              \
            Integer::NewFromUnsigned(isolate, value));
    SET_INTEGER_CONSTANTS("kTicketKeyReturnIndex", kTicketKeyReturnIndex);
    SET_INTEGER_CONSTANTS("kTicketKeyHMACIndex", kTicketKeyHMACIndex);
    SET_INTEGER_CONSTANTS("kTicketKeyAESIndex", kTicketKeyAESIndex);
    SET_INTEGER_CONSTANTS("kTicketKeyNameIndex", kTicketKeyNameIndex);
    SET_INTEGER_CONSTANTS("kTicketKeyIVIndex", kTicketKeyIVIndex);
  #undef SET_INTEGER_CONSTANTS

    Local<FunctionTemplate> ctx_getter_templ = FunctionTemplate::New(
        isolate, CtxGetter, Local<Value>(), Signature::New(isolate, tmpl));

    tmpl->PrototypeTemplate()->SetAccessorProperty(
        FIXED_ONE_BYTE_STRING(isolate, "_external"),
        ctx_getter_templ,
        Local<FunctionTemplate>(),
        static_cast<PropertyAttribute>(ReadOnly | DontDelete));

    env->set_secure_context_constructor_template(tmpl);
  }
  return tmpl;
}

void SecureContext::Initialize(Environment* env, Local<Object> target) {
  Local<Context> context = env->context();
  SetConstructorFunction(context,
                         target,
                         "SecureContext",
                         GetConstructorTemplate(env),
                         SetConstructorFunctionFlag::NONE);

  SetMethodNoSideEffect(context,
                        target,
                        "getBundledRootCertificates",
                        GetBundledRootCertificates);
  SetMethodNoSideEffect(
      context, target, "getSystemCACertificates", GetSystemCACertificates);
  SetMethodNoSideEffect(
      context, target, "getExtraCACertificates", GetExtraCACertificates);
}

void SecureContext::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  registry->Register(New);
  registry->Register(Init);
  registry->Register(SetKey);
  registry->Register(SetCert);
  registry->Register(AddCACert);
  registry->Register(AddCRL);
  registry->Register(AddRootCerts);
  registry->Register(SetAllowPartialTrustChain);
  registry->Register(SetCipherSuites);
  registry->Register(SetCiphers);
  registry->Register(SetSigalgs);
  registry->Register(SetECDHCurve);
  registry->Register(SetDHParam);
  registry->Register(SetMaxProto);
  registry->Register(SetMinProto);
  registry->Register(GetMaxProto);
  registry->Register(GetMinProto);
  registry->Register(SetOptions);
  registry->Register(SetSessionIdContext);
  registry->Register(SetSessionTimeout);
  registry->Register(Close);
  registry->Register(LoadPKCS12);
  registry->Register(SetTicketKeys);
  registry->Register(EnableTicketKeyCallback);
  registry->Register(GetTicketKeys);
  registry->Register(GetCertificate<true>);
  registry->Register(GetCertificate<false>);

#ifndef OPENSSL_NO_ENGINE
  registry->Register(SetEngineKey);
  registry->Register(SetClientCertEngine);
#endif  // !OPENSSL_NO_ENGINE

  registry->Register(CtxGetter);

  registry->Register(GetBundledRootCertificates);
  registry->Register(GetSystemCACertificates);
  registry->Register(GetExtraCACertificates);
}

SecureContext* SecureContext::Create(Environment* env) {
  Local<Object> obj;
  if (!GetConstructorTemplate(env)
          ->InstanceTemplate()
          ->NewInstance(env->context()).ToLocal(&obj)) {
    return nullptr;
  }

  return new SecureContext(env, obj);
}

SecureContext::SecureContext(Environment* env, Local<Object> wrap)
    : BaseObject(env, wrap) {
  MakeWeak();
  env->isolate()->AdjustAmountOfExternalAllocatedMemory(kExternalSize);
}

inline void SecureContext::Reset() {
  if (ctx_ != nullptr) {
    env()->isolate()->AdjustAmountOfExternalAllocatedMemory(-kExternalSize);
  }
  ctx_.reset();
  cert_.reset();
  issuer_.reset();
}

SecureContext::~SecureContext() {
  Reset();
}

void SecureContext::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  new SecureContext(env, args.This());
}

void SecureContext::Init(const FunctionCallbackInfo<Value>& args) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.This());
  Environment* env = sc->env();

  CHECK_EQ(args.Length(), 3);
  CHECK(args[1]->IsInt32());
  CHECK(args[2]->IsInt32());

  int min_version = args[1].As<Int32>()->Value();
  int max_version = args[2].As<Int32>()->Value();
  const SSL_METHOD* method = TLS_method();

  if (max_version == 0)
    max_version = kMaxSupportedVersion;

  if (args[0]->IsString()) {
    Utf8Value sslmethod(env->isolate(), args[0]);

    // Note that SSLv2 and SSLv3 are disallowed but SSLv23_method and friends
    // are still accepted.  They are OpenSSL's way of saying that all known
    // protocols below TLS 1.3 are supported unless explicitly disabled (which
    // we do below for SSLv2 and SSLv3.)
    if (sslmethod == "SSLv2_method" ||
        sslmethod == "SSLv2_server_method" ||
        sslmethod == "SSLv2_client_method") {
      THROW_ERR_TLS_INVALID_PROTOCOL_METHOD(env, "SSLv2 methods disabled");
      return;
    } else if (sslmethod == "SSLv3_method" ||
               sslmethod == "SSLv3_server_method" ||
               sslmethod == "SSLv3_client_method") {
      THROW_ERR_TLS_INVALID_PROTOCOL_METHOD(env, "SSLv3 methods disabled");
      return;
    } else if (sslmethod == "SSLv23_method") {
      max_version = TLS1_2_VERSION;
    } else if (sslmethod == "SSLv23_server_method") {
      max_version = TLS1_2_VERSION;
      method = TLS_server_method();
    } else if (sslmethod == "SSLv23_client_method") {
      max_version = TLS1_2_VERSION;
      method = TLS_client_method();
    } else if (sslmethod == "TLS_method") {
      min_version = 0;
      max_version = kMaxSupportedVersion;
    } else if (sslmethod == "TLS_server_method") {
      min_version = 0;
      max_version = kMaxSupportedVersion;
      method = TLS_server_method();
    } else if (sslmethod == "TLS_client_method") {
      min_version = 0;
      max_version = kMaxSupportedVersion;
      method = TLS_client_method();
    } else if (sslmethod == "TLSv1_method") {
      min_version = TLS1_VERSION;
      max_version = TLS1_VERSION;
    } else if (sslmethod == "TLSv1_server_method") {
      min_version = TLS1_VERSION;
      max_version = TLS1_VERSION;
      method = TLS_server_method();
    } else if (sslmethod == "TLSv1_client_method") {
      min_version = TLS1_VERSION;
      max_version = TLS1_VERSION;
      method = TLS_client_method();
    } else if (sslmethod == "TLSv1_1_method") {
      min_version = TLS1_1_VERSION;
      max_version = TLS1_1_VERSION;
    } else if (sslmethod == "TLSv1_1_server_method") {
      min_version = TLS1_1_VERSION;
      max_version = TLS1_1_VERSION;
      method = TLS_server_method();
    } else if (sslmethod == "TLSv1_1_client_method") {
      min_version = TLS1_1_VERSION;
      max_version = TLS1_1_VERSION;
      method = TLS_client_method();
    } else if (sslmethod == "TLSv1_2_method") {
      min_version = TLS1_2_VERSION;
      max_version = TLS1_2_VERSION;
    } else if (sslmethod == "TLSv1_2_server_method") {
      min_version = TLS1_2_VERSION;
      max_version = TLS1_2_VERSION;
      method = TLS_server_method();
    } else if (sslmethod == "TLSv1_2_client_method") {
      min_version = TLS1_2_VERSION;
      max_version = TLS1_2_VERSION;
      method = TLS_client_method();
    } else {
      THROW_ERR_TLS_INVALID_PROTOCOL_METHOD(
          env, "Unknown method: %s", *sslmethod);
      return;
    }
  }

  sc->ctx_.reset(method);
  if (!sc->ctx_) {
    return ThrowCryptoError(env, ERR_get_error(), "SSL_CTX_new");
  }
  SSL_CTX_set_app_data(sc->ctx_.get(), sc);

  // Disable SSLv2 in the case when method == TLS_method() and the
  // cipher list contains SSLv2 ciphers (not the default, should be rare.)
  // The bundled OpenSSL doesn't have SSLv2 support but the system OpenSSL may.
  // SSLv3 is disabled because it's susceptible to downgrade attacks (POODLE.)
  SSL_CTX_set_options(sc->ctx_.get(), SSL_OP_NO_SSLv2);
  SSL_CTX_set_options(sc->ctx_.get(), SSL_OP_NO_SSLv3);
#if OPENSSL_VERSION_MAJOR >= 3
  SSL_CTX_set_options(sc->ctx_.get(), SSL_OP_ALLOW_CLIENT_RENEGOTIATION);
#endif

  // Enable automatic cert chaining. This is enabled by default in OpenSSL, but
  // disabled by default in BoringSSL. Enable it explicitly to make the
  // behavior match when Node is built with BoringSSL.
  SSL_CTX_clear_mode(sc->ctx_.get(), SSL_MODE_NO_AUTO_CHAIN);

  // SSL session cache configuration
  SSL_CTX_set_session_cache_mode(sc->ctx_.get(),
                                 SSL_SESS_CACHE_CLIENT |
                                 SSL_SESS_CACHE_SERVER |
                                 SSL_SESS_CACHE_NO_INTERNAL |
                                 SSL_SESS_CACHE_NO_AUTO_CLEAR);

  CHECK(SSL_CTX_set_min_proto_version(sc->ctx_.get(), min_version));
  CHECK(SSL_CTX_set_max_proto_version(sc->ctx_.get(), max_version));

  // OpenSSL 1.1.0 changed the ticket key size, but the OpenSSL 1.0.x size was
  // exposed in the public API. To retain compatibility, install a callback
  // which restores the old algorithm.
  if (!ncrypto::CSPRNG(sc->ticket_key_name_, sizeof(sc->ticket_key_name_)) ||
      !ncrypto::CSPRNG(sc->ticket_key_hmac_, sizeof(sc->ticket_key_hmac_)) ||
      !ncrypto::CSPRNG(sc->ticket_key_aes_, sizeof(sc->ticket_key_aes_))) {
    return THROW_ERR_CRYPTO_OPERATION_FAILED(
        env, "Error generating ticket keys");
  }
  SSL_CTX_set_tlsext_ticket_key_cb(sc->ctx_.get(), TicketCompatibilityCallback);
}

SSLPointer SecureContext::CreateSSL() {
  return SSLPointer(SSL_new(ctx_.get()));
}

void SecureContext::SetNewSessionCallback(NewSessionCb cb) {
  SSL_CTX_sess_set_new_cb(ctx_.get(), cb);
}

void SecureContext::SetGetSessionCallback(GetSessionCb cb) {
  SSL_CTX_sess_set_get_cb(ctx_.get(), cb);
}

void SecureContext::SetSelectSNIContextCallback(SelectSNIContextCb cb) {
  SSL_CTX_set_tlsext_servername_callback(ctx_.get(), cb);
}

void SecureContext::SetKeylogCallback(KeylogCb cb) {
  SSL_CTX_set_keylog_callback(ctx_.get(), cb);
}

Maybe<void> SecureContext::UseKey(Environment* env, const KeyObjectData& key) {
  if (key.GetKeyType() != KeyType::kKeyTypePrivate) {
    THROW_ERR_CRYPTO_INVALID_KEYTYPE(env);
    return Nothing<void>();
  }

  ClearErrorOnReturn clear_error_on_return;
  if (!SSL_CTX_use_PrivateKey(ctx_.get(), key.GetAsymmetricKey().get())) {
    ThrowCryptoError(env, ERR_get_error(), "SSL_CTX_use_PrivateKey");
    return Nothing<void>();
  }

  return JustVoid();
}

void SecureContext::SetKey(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.This());

  CHECK_GE(args.Length(), 1);  // Private key argument is mandatory

  BIOPointer bio(LoadBIO(env, args[0]));
  if (!bio)
    return;

  ByteSource passphrase;
  if (args[1]->IsString())
    passphrase = ByteSource::FromString(env, args[1].As<String>());
  // This redirection is necessary because the PasswordCallback expects a
  // pointer to a pointer to the passphrase ByteSource to allow passing in
  // const ByteSources.
  const ByteSource* pass_ptr = &passphrase;

  EVPKeyPointer key(
      PEM_read_bio_PrivateKey(bio.get(),
                              nullptr,
                              PasswordCallback,
                              &pass_ptr));

  if (!key)
    return ThrowCryptoError(env, ERR_get_error(), "PEM_read_bio_PrivateKey");

  if (!SSL_CTX_use_PrivateKey(sc->ctx_.get(), key.get()))
    return ThrowCryptoError(env, ERR_get_error(), "SSL_CTX_use_PrivateKey");
}

void SecureContext::SetSigalgs(const FunctionCallbackInfo<Value>& args) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.This());
  Environment* env = sc->env();
  ClearErrorOnReturn clear_error_on_return;

  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsString());

  const Utf8Value sigalgs(env->isolate(), args[0]);

  if (!SSL_CTX_set1_sigalgs_list(sc->ctx_.get(), *sigalgs))
    return ThrowCryptoError(env, ERR_get_error());
}

#ifndef OPENSSL_NO_ENGINE
void SecureContext::SetEngineKey(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.This());

  CHECK_EQ(args.Length(), 2);

  if (env->permission()->enabled()) [[unlikely]] {
    return THROW_ERR_CRYPTO_CUSTOM_ENGINE_NOT_SUPPORTED(
        env,
        "Programmatic selection of OpenSSL engines is unsupported while the "
        "experimental permission model is enabled");
  }

  CryptoErrorList errors;
  Utf8Value engine_id(env->isolate(), args[1]);
  auto engine =
      EnginePointer::getEngineByName(engine_id.ToStringView(), &errors);
  if (!engine) {
    Local<Value> exception;
    if (errors.empty()) {
      errors.add(getNodeCryptoErrorString(NodeCryptoError::ENGINE_NOT_FOUND,
                                          *engine_id));
    }
    if (cryptoErrorListToException(env, errors).ToLocal(&exception))
      env->isolate()->ThrowException(exception);
    return;
  }

  if (!engine.init(true /* finish on exit*/)) {
    return THROW_ERR_CRYPTO_OPERATION_FAILED(
        env, "Failure to initialize engine");
  }

  Utf8Value key_name(env->isolate(), args[0]);
  auto key = engine.loadPrivateKey(key_name.ToStringView());

  if (!key)
    return ThrowCryptoError(env, ERR_get_error(), "ENGINE_load_private_key");

  if (!SSL_CTX_use_PrivateKey(sc->ctx_.get(), key.get()))
    return ThrowCryptoError(env, ERR_get_error(), "SSL_CTX_use_PrivateKey");

  sc->private_key_engine_ = std::move(engine);
}
#endif  // !OPENSSL_NO_ENGINE

Maybe<void> SecureContext::AddCert(Environment* env, BIOPointer&& bio) {
  ClearErrorOnReturn clear_error_on_return;
  // TODO(tniessen): this should be checked by the caller and not treated as ok
  if (!bio) return JustVoid();
  cert_.reset();
  issuer_.reset();

  // The SSL_CTX_use_certificate_chain call here is not from openssl, this is
  // the method implemented elsewhere in this file. The naming is a bit
  // confusing, unfortunately.
  if (SSL_CTX_use_certificate_chain(
          ctx_.get(), std::move(bio), &cert_, &issuer_) == 0) {
    ThrowCryptoError(env, ERR_get_error(), "SSL_CTX_use_certificate_chain");
    return Nothing<void>();
  }
  return JustVoid();
}

void SecureContext::SetCert(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.This());

  CHECK_GE(args.Length(), 1);  // Certificate argument is mandatory

  BIOPointer bio(LoadBIO(env, args[0]));
  USE(sc->AddCert(env, std::move(bio)));
}

// NOLINTNEXTLINE(runtime/int)
void SecureContext::SetX509StoreFlag(unsigned long flags) {
  X509_STORE* cert_store = GetCertStoreOwnedByThisSecureContext();
  CHECK_EQ(1, X509_STORE_set_flags(cert_store, flags));
}

X509_STORE* SecureContext::GetCertStoreOwnedByThisSecureContext() {
  if (own_cert_store_cache_ != nullptr) return own_cert_store_cache_;

  X509_STORE* cert_store = SSL_CTX_get_cert_store(ctx_.get());
  if (cert_store == GetOrCreateRootCertStore()) {
    cert_store = NewRootCertStore();
    SSL_CTX_set_cert_store(ctx_.get(), cert_store);
  }

  return own_cert_store_cache_ = cert_store;
}

void SecureContext::SetAllowPartialTrustChain(
    const FunctionCallbackInfo<Value>& args) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.This());
  sc->SetX509StoreFlag(X509_V_FLAG_PARTIAL_CHAIN);
}

void SecureContext::SetCACert(const BIOPointer& bio) {
  ClearErrorOnReturn clear_error_on_return;
  if (!bio) return;
  while (X509Pointer x509 = X509Pointer(PEM_read_bio_X509_AUX(
             bio.get(), nullptr, NoPasswordCallback, nullptr))) {
    CHECK_EQ(1,
             X509_STORE_add_cert(GetCertStoreOwnedByThisSecureContext(), x509));
    CHECK_EQ(1, SSL_CTX_add_client_CA(ctx_.get(), x509));
  }
}

void SecureContext::AddCACert(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.This());

  CHECK_GE(args.Length(), 1);  // CA certificate argument is mandatory

  BIOPointer bio(LoadBIO(env, args[0]));
  sc->SetCACert(bio);
}

Maybe<void> SecureContext::SetCRL(Environment* env, const BIOPointer& bio) {
  ClearErrorOnReturn clear_error_on_return;
  // TODO(tniessen): this should be checked by the caller and not treated as ok
  if (!bio) return JustVoid();

  DeleteFnPtr<X509_CRL, X509_CRL_free> crl(
      PEM_read_bio_X509_CRL(bio.get(), nullptr, NoPasswordCallback, nullptr));

  if (!crl) {
    THROW_ERR_CRYPTO_OPERATION_FAILED(env, "Failed to parse CRL");
    return Nothing<void>();
  }

  X509_STORE* cert_store = GetCertStoreOwnedByThisSecureContext();

  CHECK_EQ(1, X509_STORE_add_crl(cert_store, crl.get()));
  CHECK_EQ(1,
           X509_STORE_set_flags(
               cert_store, X509_V_FLAG_CRL_CHECK | X509_V_FLAG_CRL_CHECK_ALL));
  return JustVoid();
}

void SecureContext::AddCRL(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.This());

  CHECK_GE(args.Length(), 1);  // CRL argument is mandatory

  BIOPointer bio(LoadBIO(env, args[0]));
  USE(sc->SetCRL(env, bio));
}

void SecureContext::SetRootCerts() {
  ClearErrorOnReturn clear_error_on_return;
  auto store = GetOrCreateRootCertStore();

  // Increment reference count so global store is not deleted along with CTX.
  X509_STORE_up_ref(store);
  SSL_CTX_set_cert_store(ctx_.get(), store);
}

void SecureContext::AddRootCerts(const FunctionCallbackInfo<Value>& args) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.This());
  sc->SetRootCerts();
}

void SecureContext::SetCipherSuites(const FunctionCallbackInfo<Value>& args) {
  // BoringSSL doesn't allow API config of TLS1.3 cipher suites.
#ifndef OPENSSL_IS_BORINGSSL
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.This());
  Environment* env = sc->env();
  ClearErrorOnReturn clear_error_on_return;

  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsString());

  const Utf8Value ciphers(env->isolate(), args[0]);
  if (!SSL_CTX_set_ciphersuites(sc->ctx_.get(), *ciphers))
    return ThrowCryptoError(env, ERR_get_error(), "Failed to set ciphers");
#endif
}

void SecureContext::SetCiphers(const FunctionCallbackInfo<Value>& args) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.This());
  Environment* env = sc->env();
  ClearErrorOnReturn clear_error_on_return;

  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsString());

  Utf8Value ciphers(env->isolate(), args[0]);
  if (!SSL_CTX_set_cipher_list(sc->ctx_.get(), *ciphers)) {
    unsigned long err = ERR_get_error();  // NOLINT(runtime/int)

    if (strlen(*ciphers) == 0 && ERR_GET_REASON(err) == SSL_R_NO_CIPHER_MATCH) {
      // TLS1.2 ciphers were deliberately cleared, so don't consider
      // SSL_R_NO_CIPHER_MATCH to be an error (this is how _set_cipher_suites()
      // works). If the user actually sets a value (like "no-such-cipher"), then
      // that's actually an error.
      return;
    }
    return ThrowCryptoError(env, err, "Failed to set ciphers");
  }
}

void SecureContext::SetECDHCurve(const FunctionCallbackInfo<Value>& args) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.This());
  Environment* env = sc->env();

  CHECK_GE(args.Length(), 1);  // ECDH curve name argument is mandatory
  CHECK(args[0]->IsString());

  Utf8Value curve(env->isolate(), args[0]);

  if (curve != "auto" && !SSL_CTX_set1_curves_list(sc->ctx_.get(), *curve)) {
    return THROW_ERR_CRYPTO_OPERATION_FAILED(env, "Failed to set ECDH curve");
  }
}

void SecureContext::SetDHParam(const FunctionCallbackInfo<Value>& args) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.This());
  Environment* env = sc->env();
  ClearErrorOnReturn clear_error_on_return;

  CHECK_GE(args.Length(), 1);  // DH argument is mandatory

  // If the user specified "auto" for dhparams, the JavaScript layer will pass
  // true to this function instead of the original string. Any other string
  // value will be interpreted as custom DH parameters below.
  if (args[0]->IsTrue()) {
    CHECK(SSL_CTX_set_dh_auto(sc->ctx_.get(), true));
    return;
  }

  DHPointer dh;
  {
    BIOPointer bio(LoadBIO(env, args[0]));
    if (!bio)
      return;

    dh.reset(PEM_read_bio_DHparams(bio.get(), nullptr, nullptr, nullptr));
  }

  // Invalid dhparam is silently discarded and DHE is no longer used.
  // TODO(tniessen): don't silently discard invalid dhparam.
  if (!dh)
    return;

  const BIGNUM* p;
  DH_get0_pqg(dh.get(), &p, nullptr, nullptr);
  const int size = BignumPointer::GetBitCount(p);
  if (size < 1024) {
    return THROW_ERR_INVALID_ARG_VALUE(
        env, "DH parameter is less than 1024 bits");
  } else if (size < 2048) {
    args.GetReturnValue().Set(FIXED_ONE_BYTE_STRING(
        env->isolate(), "DH parameter is less than 2048 bits"));
  }

  if (!SSL_CTX_set_tmp_dh(sc->ctx_.get(), dh.get())) {
    return THROW_ERR_CRYPTO_OPERATION_FAILED(
        env, "Error setting temp DH parameter");
  }
}

void SecureContext::SetMinProto(const FunctionCallbackInfo<Value>& args) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.This());

  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsInt32());

  int version = args[0].As<Int32>()->Value();

  CHECK(SSL_CTX_set_min_proto_version(sc->ctx_.get(), version));
}

void SecureContext::SetMaxProto(const FunctionCallbackInfo<Value>& args) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.This());

  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsInt32());

  int version = args[0].As<Int32>()->Value();

  CHECK(SSL_CTX_set_max_proto_version(sc->ctx_.get(), version));
}

void SecureContext::GetMinProto(const FunctionCallbackInfo<Value>& args) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.This());

  CHECK_EQ(args.Length(), 0);

  long version =  // NOLINT(runtime/int)
    SSL_CTX_get_min_proto_version(sc->ctx_.get());
  args.GetReturnValue().Set(static_cast<uint32_t>(version));
}

void SecureContext::GetMaxProto(const FunctionCallbackInfo<Value>& args) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.This());

  CHECK_EQ(args.Length(), 0);

  long version =  // NOLINT(runtime/int)
    SSL_CTX_get_max_proto_version(sc->ctx_.get());
  args.GetReturnValue().Set(static_cast<uint32_t>(version));
}

void SecureContext::SetOptions(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.This());

  CHECK_GE(args.Length(), 1);
  CHECK(args[0]->IsNumber());

  int64_t val = args[0]->IntegerValue(env->context()).FromMaybe(0);

  SSL_CTX_set_options(sc->ctx_.get(),
                      static_cast<long>(val));  // NOLINT(runtime/int)
}

void SecureContext::SetSessionIdContext(
    const FunctionCallbackInfo<Value>& args) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.This());
  Environment* env = sc->env();

  CHECK_GE(args.Length(), 1);
  CHECK(args[0]->IsString());

  const Utf8Value sessionIdContext(env->isolate(), args[0]);
  const unsigned char* sid_ctx =
      reinterpret_cast<const unsigned char*>(*sessionIdContext);
  unsigned int sid_ctx_len = sessionIdContext.length();

  if (SSL_CTX_set_session_id_context(sc->ctx_.get(), sid_ctx, sid_ctx_len) == 1)
    return;

  Local<String> message;

  auto bio = BIOPointer::NewMem();
  if (!bio) {
    message = FIXED_ONE_BYTE_STRING(env->isolate(),
                                    "SSL_CTX_set_session_id_context error");
  } else {
    ERR_print_errors(bio.get());
    BUF_MEM* mem = bio;
    message = OneByteString(env->isolate(), mem->data, mem->length);
  }

  env->isolate()->ThrowException(Exception::TypeError(message));
}

void SecureContext::SetSessionTimeout(const FunctionCallbackInfo<Value>& args) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.This());

  CHECK_GE(args.Length(), 1);
  CHECK(args[0]->IsInt32());

  int32_t sessionTimeout = args[0].As<Int32>()->Value();
  CHECK_GE(sessionTimeout, 0);
  SSL_CTX_set_timeout(sc->ctx_.get(), sessionTimeout);
}

void SecureContext::Close(const FunctionCallbackInfo<Value>& args) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.This());
  sc->Reset();
}

// Takes .pfx or .p12 and password in string or buffer format
void SecureContext::LoadPKCS12(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  std::vector<char> pass;
  bool ret = false;

  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.This());
  ClearErrorOnReturn clear_error_on_return;

  if (args.Length() < 1) {
    return THROW_ERR_MISSING_ARGS(env, "PFX certificate argument is mandatory");
  }

  BIOPointer in(LoadBIO(env, args[0]));
  if (!in) {
    return THROW_ERR_CRYPTO_OPERATION_FAILED(
        env, "Unable to load PFX certificate");
  }

  if (args.Length() >= 2) {
    THROW_AND_RETURN_IF_NOT_BUFFER(env, args[1], "Pass phrase");
    Local<ArrayBufferView> abv = args[1].As<ArrayBufferView>();
    size_t passlen = abv->ByteLength();
    pass.resize(passlen + 1);
    abv->CopyContents(pass.data(), passlen);
    pass[passlen] = '\0';
  }

  // Free previous certs
  sc->issuer_.reset();
  sc->cert_.reset();

  DeleteFnPtr<PKCS12, PKCS12_free> p12;
  EVPKeyPointer pkey;
  X509Pointer cert;
  StackOfX509 extra_certs;

  PKCS12* p12_ptr = nullptr;
  EVP_PKEY* pkey_ptr = nullptr;
  X509* cert_ptr = nullptr;
  STACK_OF(X509)* extra_certs_ptr = nullptr;

  if (!d2i_PKCS12_bio(in.get(), &p12_ptr)) {
    goto done;
  }

  // Move ownership to the smart pointer:
  p12.reset(p12_ptr);

  if (!PKCS12_parse(
          p12.get(), pass.data(), &pkey_ptr, &cert_ptr, &extra_certs_ptr)) {
    goto done;
  }

  // Move ownership of the parsed data:
  pkey.reset(pkey_ptr);
  cert.reset(cert_ptr);
  extra_certs.reset(extra_certs_ptr);

  if (!pkey) {
    return THROW_ERR_CRYPTO_OPERATION_FAILED(
        env, "Unable to load private key from PFX data");
  }

  if (!cert) {
    return THROW_ERR_CRYPTO_OPERATION_FAILED(
        env, "Unable to load certificate from PFX data");
  }

  if (!SSL_CTX_use_certificate_chain(sc->ctx_.get(),
                                     std::move(cert),
                                     extra_certs.get(),
                                     &sc->cert_,
                                     &sc->issuer_)) {
    goto done;
  }

  if (!SSL_CTX_use_PrivateKey(sc->ctx_.get(), pkey.get())) {
    goto done;
  }

  // Add CA certs too
  for (int i = 0; i < sk_X509_num(extra_certs.get()); i++) {
    X509* ca = sk_X509_value(extra_certs.get(), i);

    X509_STORE_add_cert(sc->GetCertStoreOwnedByThisSecureContext(), ca);
    CHECK_EQ(1, SSL_CTX_add_client_CA(sc->ctx_.get(), ca));
  }
  ret = true;

done:
  if (!ret) {
    // TODO(@jasnell): Should this use ThrowCryptoError?
    unsigned long err = ERR_get_error();  // NOLINT(runtime/int)

#if OPENSSL_VERSION_MAJOR >= 3
    if (ERR_GET_REASON(err) == ERR_R_UNSUPPORTED) {
      // OpenSSL's "unsupported" error without any context is very
      // common and not very helpful, so we override it:
      return THROW_ERR_CRYPTO_UNSUPPORTED_OPERATION(
          env, "Unsupported PKCS12 PFX data");
    }
#endif

    const char* str = ERR_reason_error_string(err);
    str = str != nullptr ? str : "Unknown error";

    return env->ThrowError(str);
  }
}

#ifndef OPENSSL_NO_ENGINE
void SecureContext::SetClientCertEngine(
    const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsString());

  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, args.This());

  MarkPopErrorOnReturn mark_pop_error_on_return;

  // SSL_CTX_set_client_cert_engine does not itself support multiple
  // calls by cleaning up before overwriting the client_cert_engine
  // internal context variable.
  // Instead of trying to fix up this problem we in turn also do not
  // support multiple calls to SetClientCertEngine.
  CHECK(!sc->client_cert_engine_provided_);

  if (env->permission()->enabled()) [[unlikely]] {
    return THROW_ERR_CRYPTO_CUSTOM_ENGINE_NOT_SUPPORTED(
        env,
        "Programmatic selection of OpenSSL engines is unsupported while the "
        "experimental permission model is enabled");
  }

  CryptoErrorList errors;
  const Utf8Value engine_id(env->isolate(), args[0]);
  auto engine =
      EnginePointer::getEngineByName(engine_id.ToStringView(), &errors);
  if (!engine) {
    Local<Value> exception;
    if (errors.empty()) {
      errors.add(getNodeCryptoErrorString(NodeCryptoError::ENGINE_NOT_FOUND,
                                          *engine_id));
    }
    if (cryptoErrorListToException(env, errors).ToLocal(&exception))
      env->isolate()->ThrowException(exception);
    return;
  }

  // Note that this takes another reference to `engine`.
  if (!SSL_CTX_set_client_cert_engine(sc->ctx_.get(), engine.get()))
    return ThrowCryptoError(env, ERR_get_error());
  sc->client_cert_engine_provided_ = true;
}
#endif  // !OPENSSL_NO_ENGINE

void SecureContext::GetTicketKeys(const FunctionCallbackInfo<Value>& args) {
  SecureContext* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.This());

  Local<Object> buff;
  if (!Buffer::New(wrap->env(), 48).ToLocal(&buff))
    return;

  memcpy(Buffer::Data(buff), wrap->ticket_key_name_, 16);
  memcpy(Buffer::Data(buff) + 16, wrap->ticket_key_hmac_, 16);
  memcpy(Buffer::Data(buff) + 32, wrap->ticket_key_aes_, 16);

  args.GetReturnValue().Set(buff);
}

void SecureContext::SetTicketKeys(const FunctionCallbackInfo<Value>& args) {
  SecureContext* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.This());

  CHECK_GE(args.Length(), 1);  // Ticket keys argument is mandatory
  CHECK(args[0]->IsArrayBufferView());
  ArrayBufferViewContents<char> buf(args[0].As<ArrayBufferView>());

  CHECK_EQ(buf.length(), 48);

  memcpy(wrap->ticket_key_name_, buf.data(), 16);
  memcpy(wrap->ticket_key_hmac_, buf.data() + 16, 16);
  memcpy(wrap->ticket_key_aes_, buf.data() + 32, 16);

  args.GetReturnValue().Set(true);
}

// Currently, EnableTicketKeyCallback and TicketKeyCallback are only present for
// the regression test in test/parallel/test-https-resume-after-renew.js.
void SecureContext::EnableTicketKeyCallback(
    const FunctionCallbackInfo<Value>& args) {
  SecureContext* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.This());

  SSL_CTX_set_tlsext_ticket_key_cb(wrap->ctx_.get(), TicketKeyCallback);
}

int SecureContext::TicketKeyCallback(SSL* ssl,
                                     unsigned char* name,
                                     unsigned char* iv,
                                     EVP_CIPHER_CTX* ectx,
                                     HMAC_CTX* hctx,
                                     int enc) {
  static const int kTicketPartSize = 16;

  SecureContext* sc = static_cast<SecureContext*>(
      SSL_CTX_get_app_data(SSL_get_SSL_CTX(ssl)));

  Environment* env = sc->env();
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Value> argv[3];

  if (!Buffer::Copy(
          env,
          reinterpret_cast<char*>(name),
          kTicketPartSize).ToLocal(&argv[0]) ||
      !Buffer::Copy(
          env,
          reinterpret_cast<char*>(iv),
          kTicketPartSize).ToLocal(&argv[1])) {
    return -1;
  }

  argv[2] = Boolean::New(env->isolate(), enc != 0);

  Local<Value> ret;
  if (!node::MakeCallback(
          env->isolate(),
          sc->object(),
          env->ticketkeycallback_string(),
          arraysize(argv),
          argv,
          {0, 0}).ToLocal(&ret) ||
      !ret->IsArray()) {
    return -1;
  }
  Local<Array> arr = ret.As<Array>();

  Local<Value> val;
  if (!arr->Get(env->context(), kTicketKeyReturnIndex).ToLocal(&val) ||
      !val->IsInt32()) {
    return -1;
  }

  int r = val.As<Int32>()->Value();
  if (r < 0)
    return r;

  Local<Value> hmac;
  Local<Value> aes;

  if (!arr->Get(env->context(), kTicketKeyHMACIndex).ToLocal(&hmac) ||
      !arr->Get(env->context(), kTicketKeyAESIndex).ToLocal(&aes) ||
      Buffer::Length(aes) != kTicketPartSize) {
    return -1;
  }

  if (enc) {
    Local<Value> name_val;
    Local<Value> iv_val;
    if (!arr->Get(env->context(), kTicketKeyNameIndex).ToLocal(&name_val) ||
        !arr->Get(env->context(), kTicketKeyIVIndex).ToLocal(&iv_val) ||
        Buffer::Length(name_val) != kTicketPartSize ||
        Buffer::Length(iv_val) != kTicketPartSize) {
      return -1;
    }

    name_val.As<ArrayBufferView>()->CopyContents(name, kTicketPartSize);
    iv_val.As<ArrayBufferView>()->CopyContents(iv, kTicketPartSize);
  }

  ArrayBufferViewContents<unsigned char> hmac_buf(hmac);
  HMAC_Init_ex(hctx,
               hmac_buf.data(),
               hmac_buf.length(),
               EVP_sha256(),
               nullptr);

  ArrayBufferViewContents<unsigned char> aes_key(aes.As<ArrayBufferView>());
  if (enc) {
    EVP_EncryptInit_ex(ectx,
                       EVP_aes_128_cbc(),
                       nullptr,
                       aes_key.data(),
                       iv);
  } else {
    EVP_DecryptInit_ex(ectx,
                       EVP_aes_128_cbc(),
                       nullptr,
                       aes_key.data(),
                       iv);
  }

  return r;
}

int SecureContext::TicketCompatibilityCallback(SSL* ssl,
                                               unsigned char* name,
                                               unsigned char* iv,
                                               EVP_CIPHER_CTX* ectx,
                                               HMAC_CTX* hctx,
                                               int enc) {
  SecureContext* sc = static_cast<SecureContext*>(
      SSL_CTX_get_app_data(SSL_get_SSL_CTX(ssl)));

  if (enc) {
    memcpy(name, sc->ticket_key_name_, sizeof(sc->ticket_key_name_));
    if (!ncrypto::CSPRNG(iv, 16) ||
        EVP_EncryptInit_ex(
            ectx, EVP_aes_128_cbc(), nullptr, sc->ticket_key_aes_, iv) <= 0 ||
        HMAC_Init_ex(hctx,
                     sc->ticket_key_hmac_,
                     sizeof(sc->ticket_key_hmac_),
                     EVP_sha256(),
                     nullptr) <= 0) {
      return -1;
    }
    return 1;
  }

  if (memcmp(name, sc->ticket_key_name_, sizeof(sc->ticket_key_name_)) != 0) {
    // The ticket key name does not match. Discard the ticket.
    return 0;
  }

  if (EVP_DecryptInit_ex(ectx, EVP_aes_128_cbc(), nullptr, sc->ticket_key_aes_,
                         iv) <= 0 ||
      HMAC_Init_ex(hctx, sc->ticket_key_hmac_, sizeof(sc->ticket_key_hmac_),
                   EVP_sha256(), nullptr) <= 0) {
    return -1;
  }
  return 1;
}

void SecureContext::CtxGetter(const FunctionCallbackInfo<Value>& info) {
  SecureContext* sc;
  ASSIGN_OR_RETURN_UNWRAP(&sc, info.This());
  Local<External> ext = External::New(info.GetIsolate(), sc->ctx_.get());
  info.GetReturnValue().Set(ext);
}

template <bool primary>
void SecureContext::GetCertificate(const FunctionCallbackInfo<Value>& args) {
  SecureContext* wrap;
  ASSIGN_OR_RETURN_UNWRAP(&wrap, args.This());
  Environment* env = wrap->env();
  X509* cert;

  if (primary)
    cert = wrap->cert_.get();
  else
    cert = wrap->issuer_.get();
  if (cert == nullptr)
    return args.GetReturnValue().SetNull();

  int size = i2d_X509(cert, nullptr);
  Local<Object> buff;
  if (!Buffer::New(env, size).ToLocal(&buff))
    return;
  unsigned char* serialized = reinterpret_cast<unsigned char*>(
      Buffer::Data(buff));
  i2d_X509(cert, &serialized);

  args.GetReturnValue().Set(buff);
}

// UseExtraCaCerts is called only once at the start of the Node.js process.
void UseExtraCaCerts(std::string_view file) {
  extra_root_certs_file = file;
}

}  // namespace crypto
}  // namespace node
