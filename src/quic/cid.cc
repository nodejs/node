#if HAVE_OPENSSL && HAVE_QUIC
#include "guard.h"
#ifndef OPENSSL_NO_QUIC
#include <crypto/crypto_util.h>
#include <memory_tracker-inl.h>
#include <node_mutex.h>
#include <string_bytes.h>
#include "cid.h"
#include "defs.h"
#include "nbytes.h"
#include "ncrypto.h"

namespace node::quic {

// ============================================================================
// CID

CID::CID() : ptr_(&cid_) {
  cid_.datalen = 0;
}

CID::CID(const ngtcp2_cid& cid) : CID(cid.data, cid.datalen) {}

CID::CID(const uint8_t* data, size_t len) : CID() {
  DCHECK_LE(len, kMaxLength);
  ngtcp2_cid_init(&cid_, data, len);
}

CID::CID(const ngtcp2_cid* cid) : ptr_(cid) {
  CHECK_NOT_NULL(cid);
  DCHECK_LE(cid->datalen, kMaxLength);
}

CID::CID(const CID& other) : ptr_(&cid_) {
  CHECK_NOT_NULL(other.ptr_);
  ngtcp2_cid_init(&cid_, other.ptr_->data, other.ptr_->datalen);
}

CID& CID::operator=(const CID& other) {
  CHECK_NOT_NULL(other.ptr_);
  ptr_ = &cid_;
  ngtcp2_cid_init(&cid_, other.ptr_->data, other.ptr_->datalen);
  return *this;
}

bool CID::operator==(const CID& other) const noexcept {
  if (this == &other || (length() == 0 && other.length() == 0)) return true;
  if (length() != other.length()) return false;
  return memcmp(ptr_->data, other.ptr_->data, ptr_->datalen) == 0;
}

bool CID::operator!=(const CID& other) const noexcept {
  return !(*this == other);
}

CID::operator const uint8_t*() const {
  return ptr_->data;
}
CID::operator const ngtcp2_cid&() const {
  return *ptr_;
}
CID::operator const ngtcp2_cid*() const {
  return ptr_;
}
CID::operator bool() const {
  return ptr_->datalen >= kMinLength;
}

size_t CID::length() const {
  return ptr_->datalen;
}

std::string CID::ToString() const {
  char dest[kMaxLength * 2];
  size_t written = nbytes::HexEncode(reinterpret_cast<const char*>(ptr_->data),
                                     ptr_->datalen,
                                     dest,
                                     arraysize(dest));
  return std::string(dest, written);
}

const CID CID::kInvalid{};

// ============================================================================
// CID::Hash

size_t CID::Hash::operator()(const CID& cid) const {
  // Uses the Boost hash_combine strategy: XOR each byte with the golden
  // ratio constant 0x9e3779b9 (derived from the fractional part of the
  // golden ratio, (sqrt(5)-1)/2 * 2^32) plus bit-shifted accumulator
  // state. This provides good avalanche properties for short byte
  // sequences like connection IDs (1-20 bytes).
  size_t hash = 0;
  for (size_t n = 0; n < cid.length(); n++) {
    hash ^= cid.ptr_->data[n] + 0x9e3779b9 + (hash << 6) + (hash >> 2);
  }
  return hash;
}

// ============================================================================
// CID::Factory

namespace {
class RandomCIDFactory final : public CID::Factory {
 public:
  RandomCIDFactory() = default;
  DISALLOW_COPY_AND_MOVE(RandomCIDFactory)

  const CID Generate(size_t length_hint) const override {
    DCHECK_GE(length_hint, CID::kMinLength);
    DCHECK_LE(length_hint, CID::kMaxLength);
    Mutex::ScopedLock lock(mutex_);
    maybe_refresh_pool(length_hint);
    auto start = pool_ + pos_;
    pos_ += length_hint;
    return CID(start, length_hint);
  }

  const CID GenerateInto(ngtcp2_cid* cid,
                         size_t length_hint = CID::kMaxLength) const override {
    DCHECK_GE(length_hint, CID::kMinLength);
    DCHECK_LE(length_hint, CID::kMaxLength);
    Mutex::ScopedLock lock(mutex_);
    maybe_refresh_pool(length_hint);
    auto start = pool_ + pos_;
    pos_ += length_hint;
    ngtcp2_cid_init(cid, start, length_hint);
    return CID(cid);
  }

 private:
  void maybe_refresh_pool(size_t length_hint) const {
    // We generate a pool of random data kPoolSize in length
    // and pull our random CID from that. If we don't have
    // enough random random remaining in the pool to generate
    // a CID of the requested size, we regenerate the pool
    // and reset it to zero.
    if (pos_ + length_hint > kPoolSize) {
      CHECK(ncrypto::CSPRNG(pool_, kPoolSize));
      pos_ = 0;
    }
  }

  static constexpr int kPoolSize = 1024 * 16;
  mutable int pos_ = kPoolSize;
  mutable uint8_t pool_[kPoolSize];
  mutable Mutex mutex_;
};
}  // namespace

const CID::Factory& CID::Factory::random() {
  static RandomCIDFactory instance;
  return instance;
}

}  // namespace node::quic
#endif  // OPENSSL_NO_QUIC
#endif  // HAVE_OPENSSL && HAVE_QUIC
