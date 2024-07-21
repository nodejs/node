#include "ncrypto.h"
#include <algorithm>
#include <cstring>
#include "openssl/bn.h"
#include "openssl/evp.h"
#if OPENSSL_VERSION_MAJOR >= 3
#include "openssl/provider.h"
#endif

namespace ncrypto {

// ============================================================================

ClearErrorOnReturn::ClearErrorOnReturn(CryptoErrorList* errors) : errors_(errors) {
  ERR_clear_error();
}

ClearErrorOnReturn::~ClearErrorOnReturn() {
  if (errors_ != nullptr) errors_->capture();
  ERR_clear_error();
}

int ClearErrorOnReturn::peeKError() { return ERR_peek_error(); }

MarkPopErrorOnReturn::MarkPopErrorOnReturn(CryptoErrorList* errors) : errors_(errors) {
  ERR_set_mark();
}

MarkPopErrorOnReturn::~MarkPopErrorOnReturn() {
  if (errors_ != nullptr) errors_->capture();
  ERR_pop_to_mark();
}

int MarkPopErrorOnReturn::peekError() { return ERR_peek_error(); }

CryptoErrorList::CryptoErrorList(CryptoErrorList::Option option) {
  if (option == Option::CAPTURE_ON_CONSTRUCT) capture();
}

void CryptoErrorList::capture() {
  errors_.clear();
  while(const auto err = ERR_get_error()) {
    char buf[256];
    ERR_error_string_n(err, buf, sizeof(buf));
    errors_.emplace_front(buf);
  }
}

void CryptoErrorList::add(std::string error) {
  errors_.push_back(error);
}

std::optional<std::string> CryptoErrorList::pop_back() {
  if (errors_.empty()) return std::nullopt;
  std::string error = errors_.back();
  errors_.pop_back();
  return error;
}

std::optional<std::string> CryptoErrorList::pop_front() {
  if (errors_.empty()) return std::nullopt;
  std::string error = errors_.front();
  errors_.pop_front();
  return error;
}

// ============================================================================
bool isFipsEnabled() {
#if OPENSSL_VERSION_MAJOR >= 3
  return EVP_default_properties_is_fips_enabled(nullptr) == 1;
#else
  return FIPS_mode() == 1;
#endif
}

bool setFipsEnabled(bool enable, CryptoErrorList* errors) {
  if (isFipsEnabled() == enable) return true;
  ClearErrorOnReturn clearErrorOnReturn(errors);
#if OPENSSL_VERSION_MAJOR >= 3
  return EVP_default_properties_enable_fips(nullptr, enable ? 1 : 0) == 1;
#else
  return FIPS_mode_set(enable ? 1 : 0) == 1;
#endif
}

bool testFipsEnabled() {
#if OPENSSL_VERSION_MAJOR >= 3
  OSSL_PROVIDER* fips_provider = nullptr;
  if (OSSL_PROVIDER_available(nullptr, "fips")) {
    fips_provider = OSSL_PROVIDER_load(nullptr, "fips");
  }
  const auto enabled = fips_provider == nullptr ? 0 :
      OSSL_PROVIDER_self_test(fips_provider) ? 1 : 0;
#else
#ifdef OPENSSL_FIPS
  const auto enabled = FIPS_selftest() ? 1 : 0;
#else  // OPENSSL_FIPS
  const auto enabled = 0;
#endif  // OPENSSL_FIPS
#endif

  return enabled;
}

// ============================================================================
// Bignum
BignumPointer::BignumPointer(BIGNUM* bignum) : bn_(bignum) {}

BignumPointer::BignumPointer(BignumPointer&& other) noexcept
    : bn_(other.release()) {}

BignumPointer& BignumPointer::operator=(BignumPointer&& other) noexcept {
  if (this == &other) return *this;
  this->~BignumPointer();
  return *new (this) BignumPointer(std::move(other));
}

BignumPointer::~BignumPointer() { reset(); }

void BignumPointer::reset(BIGNUM* bn) {
  bn_.reset(bn);
}

BIGNUM* BignumPointer::release() {
  return bn_.release();
}

size_t BignumPointer::byteLength() {
  if (bn_ == nullptr) return 0;
  return BN_num_bytes(bn_.get());
}

std::vector<uint8_t> BignumPointer::encode() {
  return encodePadded(bn_.get(), byteLength());
}

std::vector<uint8_t> BignumPointer::encodePadded(size_t size) {
  return encodePadded(bn_.get(), size);
}

std::vector<uint8_t> BignumPointer::encode(const BIGNUM* bn) {
  return encodePadded(bn, bn != nullptr ? BN_num_bytes(bn) : 0);
}

std::vector<uint8_t> BignumPointer::encodePadded(const BIGNUM* bn, size_t s) {
  if (bn == nullptr) return std::vector<uint8_t>(0);
  size_t size = std::max(s, static_cast<size_t>(BN_num_bytes(bn)));
  std::vector<uint8_t> buf(size);
  BN_bn2binpad(bn, buf.data(), size);
  return buf;
}

bool BignumPointer::operator==(const BignumPointer& other) noexcept {
  if (bn_ == nullptr && other.bn_ != nullptr) return false;
  if (bn_ != nullptr && other.bn_ == nullptr) return false;
  if (bn_ == nullptr && other.bn_ == nullptr) return true;
  return BN_cmp(bn_.get(), other.bn_.get()) == 0;
}

bool BignumPointer::operator==(const BIGNUM* other) noexcept {
  if (bn_ == nullptr && other != nullptr) return false;
  if (bn_ != nullptr && other == nullptr) return false;
  if (bn_ == nullptr && other == nullptr) return true;
  return BN_cmp(bn_.get(), other) == 0;
}

// ============================================================================
// Utility methods

bool CSPRNG(void* buffer, size_t length) {
  auto buf = reinterpret_cast<unsigned char*>(buffer);
  do {
    if (1 == RAND_status()) {
#if OPENSSL_VERSION_MAJOR >= 3
      if (1 == RAND_bytes_ex(nullptr, buf, length, 0)) {
        return true;
      }
#else
      while (length > INT_MAX && 1 == RAND_bytes(buf, INT_MAX)) {
        buf += INT_MAX;
        length -= INT_MAX;
      }
      if (length <= INT_MAX && 1 == RAND_bytes(buf, static_cast<int>(length)))
        return true;
#endif
    }
#if OPENSSL_VERSION_MAJOR >= 3
    const auto code = ERR_peek_last_error();
    // A misconfigured OpenSSL 3 installation may report 1 from RAND_poll()
    // and RAND_status() but fail in RAND_bytes() if it cannot look up
    // a matching algorithm for the CSPRNG.
    if (ERR_GET_LIB(code) == ERR_LIB_RAND) {
      const auto reason = ERR_GET_REASON(code);
      if (reason == RAND_R_ERROR_INSTANTIATING_DRBG ||
          reason == RAND_R_UNABLE_TO_FETCH_DRBG ||
          reason == RAND_R_UNABLE_TO_CREATE_DRBG) {
        return false;
      }
    }
#endif
  } while (1 == RAND_poll());

  return false;
}

int NoPasswordCallback(char* buf, int size, int rwflag, void* u) {
  return 0;
}

int PasswordCallback(char* buf, int size, int rwflag, void* u) {
  auto passphrase = static_cast<const Buffer<char>*>(u);
  if (passphrase != nullptr) {
    size_t buflen = static_cast<size_t>(size);
    size_t len = passphrase->len;
    if (buflen < len)
      return -1;
    memcpy(buf, reinterpret_cast<const char*>(passphrase->data), len);
    return len;
  }

  return -1;
}

// ============================================================================
// SPKAC

bool VerifySpkac(const char* input, size_t length) {
#ifdef OPENSSL_IS_BORINGSSL
  // OpenSSL uses EVP_DecodeBlock, which explicitly removes trailing characters,
  // while BoringSSL uses EVP_DecodedLength and EVP_DecodeBase64, which do not.
  // As such, we trim those characters here for compatibility.
  //
  // find_last_not_of can return npos, which is the maximum value of size_t.
  // The + 1 will force a roll-ver to 0, which is the correct value. in that
  // case.
  length = std::string_view(input, length).find_last_not_of(" \n\r\t") + 1;
#endif
  NetscapeSPKIPointer spki(
      NETSCAPE_SPKI_b64_decode(input, length));
  if (!spki)
    return false;

  EVPKeyPointer pkey(X509_PUBKEY_get(spki->spkac->pubkey));
  return pkey ? NETSCAPE_SPKI_verify(spki.get(), pkey.get()) > 0 : false;
}

BIOPointer ExportPublicKey(const char* input, size_t length) {
  BIOPointer bio(BIO_new(BIO_s_mem()));
  if (!bio) return {};

#ifdef OPENSSL_IS_BORINGSSL
  // OpenSSL uses EVP_DecodeBlock, which explicitly removes trailing characters,
  // while BoringSSL uses EVP_DecodedLength and EVP_DecodeBase64, which do not.
  // As such, we trim those characters here for compatibility.
  length = std::string_view(input, length).find_last_not_of(" \n\r\t") + 1;
#endif
  NetscapeSPKIPointer spki(
      NETSCAPE_SPKI_b64_decode(input, length));
  if (!spki) return {};

  EVPKeyPointer pkey(NETSCAPE_SPKI_get_pubkey(spki.get()));
  if (!pkey) return {};

  if (PEM_write_bio_PUBKEY(bio.get(), pkey.get()) <= 0) return { };

  return std::move(bio);
}

Buffer<char> ExportChallenge(const char* input, size_t length) {
#ifdef OPENSSL_IS_BORINGSSL
  // OpenSSL uses EVP_DecodeBlock, which explicitly removes trailing characters,
  // while BoringSSL uses EVP_DecodedLength and EVP_DecodeBase64, which do not.
  // As such, we trim those characters here for compatibility.
  length = std::string_view(input, length).find_last_not_of(" \n\r\t") + 1;
#endif
  NetscapeSPKIPointer sp(
      NETSCAPE_SPKI_b64_decode(input, length));
  if (!sp) return {};

  unsigned char* buf = nullptr;
  int buf_size = ASN1_STRING_to_UTF8(&buf, sp->spkac->challenge);
  if (buf_size >= 0) {
    return {
      .data = reinterpret_cast<char*>(buf),
      .len = static_cast<size_t>(buf_size),
    };
  }

  return {};
}

}  // namespace ncrypto
