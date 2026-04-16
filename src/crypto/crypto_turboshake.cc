#include "crypto/crypto_turboshake.h"
#include "async_wrap-inl.h"
#include "node_internals.h"
#include "threadpoolwork-inl.h"

#include <cstring>
#include <vector>

namespace node::crypto {

using v8::FunctionCallbackInfo;
using v8::JustVoid;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Nothing;
using v8::Object;
using v8::Uint32;
using v8::Value;

// ============================================================================
// Keccak-p[1600, n_r=12] permutation
// Reference: FIPS 202, Section 3.3 and 3.4; RFC 9861 Section 2.2
// Adapted from OpenSSL's keccak1600.c (KECCAK_REF variant)
// ============================================================================
namespace {

inline uint64_t ROL64(uint64_t val, int offset) {
  DCHECK(offset >= 0 && offset < 64);
  if (offset == 0) return val;
  return (val << offset) | (val >> (64 - offset));
}

// Load/store 64-bit lanes in little-endian byte order.
// The Keccak state uses LE lane encoding (FIPS 202 Section 1, B.1).
// These helpers ensure correctness on both LE and BE platforms.
inline uint64_t LoadLE64(const uint8_t* src) {
  return static_cast<uint64_t>(src[0]) | (static_cast<uint64_t>(src[1]) << 8) |
         (static_cast<uint64_t>(src[2]) << 16) |
         (static_cast<uint64_t>(src[3]) << 24) |
         (static_cast<uint64_t>(src[4]) << 32) |
         (static_cast<uint64_t>(src[5]) << 40) |
         (static_cast<uint64_t>(src[6]) << 48) |
         (static_cast<uint64_t>(src[7]) << 56);
}

inline void StoreLE64(uint8_t* dst, uint64_t val) {
  dst[0] = static_cast<uint8_t>(val);
  dst[1] = static_cast<uint8_t>(val >> 8);
  dst[2] = static_cast<uint8_t>(val >> 16);
  dst[3] = static_cast<uint8_t>(val >> 24);
  dst[4] = static_cast<uint8_t>(val >> 32);
  dst[5] = static_cast<uint8_t>(val >> 40);
  dst[6] = static_cast<uint8_t>(val >> 48);
  dst[7] = static_cast<uint8_t>(val >> 56);
}

static const unsigned char rhotates[5][5] = {
    {0, 1, 62, 28, 27},
    {36, 44, 6, 55, 20},
    {3, 10, 43, 25, 39},
    {41, 45, 15, 21, 8},
    {18, 2, 61, 56, 14},
};

// Round constants for Keccak-f[1600].
// TurboSHAKE uses the last 12 rounds (indices 12..23).
static const uint64_t iotas[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL,
    0x8000000080008000ULL, 0x000000000000808bULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008aULL,
    0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
    0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL,
    0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800aULL, 0x800000008000000aULL, 0x8000000080008081ULL,
    0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL,
};

// Keccak-p[1600, 12]: the reduced-round permutation used by TurboSHAKE.
void KeccakP1600_12(uint64_t A[5][5]) {
  for (size_t round = 12; round < 24; round++) {
    // Theta
    uint64_t C[5];
    for (size_t x = 0; x < 5; x++) {
      C[x] = A[0][x] ^ A[1][x] ^ A[2][x] ^ A[3][x] ^ A[4][x];
    }
    uint64_t D[5];
    for (size_t x = 0; x < 5; x++) {
      D[x] = C[(x + 4) % 5] ^ ROL64(C[(x + 1) % 5], 1);
    }
    for (size_t y = 0; y < 5; y++) {
      for (size_t x = 0; x < 5; x++) {
        A[y][x] ^= D[x];
      }
    }

    // Rho
    for (size_t y = 0; y < 5; y++) {
      for (size_t x = 0; x < 5; x++) {
        A[y][x] = ROL64(A[y][x], rhotates[y][x]);
      }
    }

    // Pi
    uint64_t T[5][5];
    memcpy(T, A, sizeof(T));
    for (size_t y = 0; y < 5; y++) {
      for (size_t x = 0; x < 5; x++) {
        A[y][x] = T[x][(3 * y + x) % 5];
      }
    }

    // Chi
    for (size_t y = 0; y < 5; y++) {
      uint64_t row[5];
      for (size_t x = 0; x < 5; x++) {
        row[x] = A[y][x] ^ (~A[y][(x + 1) % 5] & A[y][(x + 2) % 5]);
      }
      memcpy(A[y], row, sizeof(row));
    }

    // Iota
    A[0][0] ^= iotas[round];
  }
}

// ============================================================================
// TurboSHAKE sponge construction
// RFC 9861 Section 2.2, Appendix A.2/A.3
// ============================================================================

// TurboSHAKE128 rate = 168 bytes (1344 bits), capacity = 256 bits
// TurboSHAKE256 rate = 136 bytes (1088 bits), capacity = 512 bits
static constexpr size_t kTurboSHAKE128Rate = 168;
static constexpr size_t kTurboSHAKE256Rate = 136;

void TurboSHAKE(const uint8_t* input,
                size_t input_len,
                size_t rate,
                uint8_t domain_sep,
                uint8_t* output,
                size_t output_len) {
  uint64_t A[5][5] = {};
  // Both rates (168, 136) are multiples of 8
  size_t lane_count = rate / 8;

  size_t offset = 0;

  // Absorb complete blocks from input
  while (offset + rate <= input_len) {
    for (size_t i = 0; i < lane_count; i++) {
      A[i / 5][i % 5] ^= LoadLE64(input + offset + i * 8);
    }
    KeccakP1600_12(A);
    offset += rate;
  }

  // Absorb last (partial) block: remaining input bytes + domain_sep + padding
  size_t remaining = input_len - offset;
  uint8_t pad[168] = {};  // sized for max rate (TurboSHAKE128)
  if (remaining > 0) {
    memcpy(pad, input + offset, remaining);
  }
  pad[remaining] ^= domain_sep;
  pad[rate - 1] ^= 0x80;

  for (size_t i = 0; i < lane_count; i++) {
    A[i / 5][i % 5] ^= LoadLE64(pad + i * 8);
  }
  KeccakP1600_12(A);

  // Squeeze output
  size_t out_offset = 0;
  while (out_offset < output_len) {
    size_t block = output_len - out_offset;
    if (block > rate) block = rate;
    size_t full_lanes = block / 8;
    for (size_t i = 0; i < full_lanes; i++) {
      StoreLE64(output + out_offset + i * 8, A[i / 5][i % 5]);
    }
    size_t rem = block % 8;
    if (rem > 0) {
      uint8_t tmp[8];
      StoreLE64(tmp, A[full_lanes / 5][full_lanes % 5]);
      memcpy(output + out_offset + full_lanes * 8, tmp, rem);
    }
    out_offset += block;
    if (out_offset < output_len) {
      KeccakP1600_12(A);
    }
  }
}

// Convenience wrappers
void TurboSHAKE128(const uint8_t* input,
                   size_t input_len,
                   uint8_t domain_sep,
                   uint8_t* output,
                   size_t output_len) {
  TurboSHAKE(
      input, input_len, kTurboSHAKE128Rate, domain_sep, output, output_len);
}

void TurboSHAKE256(const uint8_t* input,
                   size_t input_len,
                   uint8_t domain_sep,
                   uint8_t* output,
                   size_t output_len) {
  TurboSHAKE(
      input, input_len, kTurboSHAKE256Rate, domain_sep, output, output_len);
}

// ============================================================================
// KangarooTwelve tree hashing (RFC 9861 Section 3)
// ============================================================================

static constexpr size_t kChunkSize = 8192;

// length_encode(x): RFC 9861 Section 3.3
// Returns byte string x_(n-1) || ... || x_0 || n
// where x = sum of 256^i * x_i, n is smallest such that x < 256^n
std::vector<uint8_t> LengthEncode(size_t x) {
  if (x == 0) {
    return {0x00};
  }

  std::vector<uint8_t> result;
  size_t val = x;
  while (val > 0) {
    result.push_back(static_cast<uint8_t>(val & 0xFF));
    val >>= 8;
  }

  // Reverse to get big-endian: x_(n-1) || ... || x_0
  size_t n = result.size();
  for (size_t i = 0; i < n / 2; i++) {
    std::swap(result[i], result[n - 1 - i]);
  }

  // Append n (the length of the encoding)
  result.push_back(static_cast<uint8_t>(n));
  return result;
}

using TurboSHAKEFn = void (*)(const uint8_t* input,
                              size_t input_len,
                              uint8_t domain_sep,
                              uint8_t* output,
                              size_t output_len);

void KangarooTwelve(const uint8_t* message,
                    size_t msg_len,
                    const uint8_t* customization,
                    size_t custom_len,
                    uint8_t* output,
                    size_t output_len,
                    TurboSHAKEFn turboshake,
                    size_t cv_len) {
  // Build S = M || C || length_encode(|C|)
  auto len_enc = LengthEncode(custom_len);
  size_t s_len = msg_len + custom_len + len_enc.size();

  // Short message path: |S| <= 8192
  if (s_len <= kChunkSize) {
    // Build S in a contiguous buffer
    std::vector<uint8_t> s(s_len);
    size_t pos = 0;
    if (msg_len > 0) {
      memcpy(s.data() + pos, message, msg_len);
      pos += msg_len;
    }
    if (custom_len > 0) {
      memcpy(s.data() + pos, customization, custom_len);
      pos += custom_len;
    }
    memcpy(s.data() + pos, len_enc.data(), len_enc.size());

    turboshake(s.data(), s_len, 0x07, output, output_len);
    return;
  }

  // Long message path: tree hashing
  // We need to process S in chunks, but S is virtual (M || C || length_encode)
  // Build a helper to read from this virtual concatenation.

  // First chunk is S[0:8192], compute chaining values for rest
  // FinalNode = S[0:8192] || 0x03 || 0x00^7

  // We need to read from S = M || C || length_encode(|C|)
  // Helper lambda to copy from virtual S
  auto read_s = [&](size_t s_offset, uint8_t* buf, size_t len) {
    size_t copied = 0;
    // Part 1: message
    if (s_offset < msg_len && copied < len) {
      size_t avail = msg_len - s_offset;
      size_t to_copy = avail < (len - copied) ? avail : (len - copied);
      memcpy(buf + copied, message + s_offset, to_copy);
      copied += to_copy;
      s_offset += to_copy;
    }
    // Part 2: customization
    size_t custom_start = msg_len;
    if (s_offset < custom_start + custom_len && copied < len) {
      size_t off_in_custom = s_offset - custom_start;
      size_t avail = custom_len - off_in_custom;
      size_t to_copy = avail < (len - copied) ? avail : (len - copied);
      memcpy(buf + copied, customization + off_in_custom, to_copy);
      copied += to_copy;
      s_offset += to_copy;
    }
    // Part 3: length_encode
    size_t le_start = msg_len + custom_len;
    if (s_offset < le_start + len_enc.size() && copied < len) {
      size_t off_in_le = s_offset - le_start;
      size_t avail = len_enc.size() - off_in_le;
      size_t to_copy = avail < (len - copied) ? avail : (len - copied);
      memcpy(buf + copied, len_enc.data() + off_in_le, to_copy);
      copied += to_copy;
    }
  };

  // Start building FinalNode
  // FinalNode = S_0 || 0x03 0x00^7 || CV_1 || CV_2 || ... || CV_(n-1)
  //           || length_encode(n-1) || 0xFF 0xFF

  // Read first chunk S_0
  std::vector<uint8_t> first_chunk(kChunkSize);
  read_s(0, first_chunk.data(), kChunkSize);

  // Start FinalNode with S_0 || 0x03 || 0x00^7
  std::vector<uint8_t> final_node;
  final_node.reserve(kChunkSize + 8 + ((s_len / kChunkSize) * cv_len) + 16);
  final_node.insert(final_node.end(), first_chunk.begin(), first_chunk.end());
  final_node.push_back(0x03);
  final_node.insert(final_node.end(), 7, 0x00);

  // Process remaining chunks
  size_t offset = kChunkSize;
  size_t num_blocks = 0;
  std::vector<uint8_t> chunk(kChunkSize);
  std::vector<uint8_t> cv(cv_len);

  while (offset < s_len) {
    size_t block_size = s_len - offset;
    if (block_size > kChunkSize) block_size = kChunkSize;

    chunk.resize(block_size);
    read_s(offset, chunk.data(), block_size);

    // CV = TurboSHAKE(chunk, 0x0B, cv_len)
    turboshake(chunk.data(), block_size, 0x0B, cv.data(), cv_len);

    final_node.insert(final_node.end(), cv.begin(), cv.end());
    num_blocks++;
    offset += block_size;
  }

  // Append length_encode(num_blocks) || 0xFF 0xFF
  auto num_blocks_enc = LengthEncode(num_blocks);
  final_node.insert(
      final_node.end(), num_blocks_enc.begin(), num_blocks_enc.end());
  final_node.push_back(0xFF);
  final_node.push_back(0xFF);

  // Final hash
  turboshake(final_node.data(), final_node.size(), 0x06, output, output_len);
}

void KT128(const uint8_t* message,
           size_t msg_len,
           const uint8_t* customization,
           size_t custom_len,
           uint8_t* output,
           size_t output_len) {
  KangarooTwelve(message,
                 msg_len,
                 customization,
                 custom_len,
                 output,
                 output_len,
                 TurboSHAKE128,
                 32);
}

void KT256(const uint8_t* message,
           size_t msg_len,
           const uint8_t* customization,
           size_t custom_len,
           uint8_t* output,
           size_t output_len) {
  KangarooTwelve(message,
                 msg_len,
                 customization,
                 custom_len,
                 output,
                 output_len,
                 TurboSHAKE256,
                 64);
}

}  // anonymous namespace

// ============================================================================
// TurboShake bindings
// ============================================================================

TurboShakeConfig::TurboShakeConfig(TurboShakeConfig&& other) noexcept
    : job_mode(other.job_mode),
      variant(other.variant),
      output_length(other.output_length),
      domain_separation(other.domain_separation),
      data(std::move(other.data)) {}

TurboShakeConfig& TurboShakeConfig::operator=(
    TurboShakeConfig&& other) noexcept {
  if (&other == this) return *this;
  this->~TurboShakeConfig();
  return *new (this) TurboShakeConfig(std::move(other));
}

void TurboShakeConfig::MemoryInfo(MemoryTracker* tracker) const {
  if (job_mode == kCryptoJobAsync) {
    // TODO(addaleax): Implement MemoryRetainer protocol for ByteSource
    tracker->TrackFieldWithSize("data", data.size());
  }
}

Maybe<void> TurboShakeTraits::AdditionalConfig(
    CryptoJobMode mode,
    const FunctionCallbackInfo<Value>& args,
    unsigned int offset,
    TurboShakeConfig* params) {
  Environment* env = Environment::GetCurrent(args);

  params->job_mode = mode;

  // args[offset + 0] = algorithm name (string)
  CHECK(args[offset]->IsString());
  Utf8Value algorithm_name(env->isolate(), args[offset]);
  std::string_view alg = algorithm_name.ToStringView();

  if (alg == "TurboSHAKE128") {
    params->variant = TurboShakeVariant::TurboSHAKE128;
  } else if (alg == "TurboSHAKE256") {
    params->variant = TurboShakeVariant::TurboSHAKE256;
  } else {
    UNREACHABLE();
  }

  // args[offset + 1] = domain separation byte (uint32)
  CHECK(args[offset + 1]->IsUint32());
  params->domain_separation =
      static_cast<uint8_t>(args[offset + 1].As<Uint32>()->Value());
  CHECK_GE(params->domain_separation, 0x01);
  CHECK_LE(params->domain_separation, 0x7F);

  // args[offset + 2] = output length in bytes (uint32)
  CHECK(args[offset + 2]->IsUint32());
  params->output_length = args[offset + 2].As<Uint32>()->Value();

  // args[offset + 3] = data (ArrayBuffer/View)
  ArrayBufferOrViewContents<char> data(args[offset + 3]);
  if (!data.CheckSizeInt32()) [[unlikely]] {
    THROW_ERR_OUT_OF_RANGE(env, "data is too big");
    return Nothing<void>();
  }
  params->data = mode == kCryptoJobAsync ? data.ToCopy() : data.ToByteSource();

  return JustVoid();
}

bool TurboShakeTraits::DeriveBits(Environment* env,
                                  const TurboShakeConfig& params,
                                  ByteSource* out,
                                  CryptoJobMode mode,
                                  CryptoErrorStore* errors) {
  CHECK_GT(params.output_length, 0);
  char* buf = MallocOpenSSL<char>(params.output_length);

  const uint8_t* input = reinterpret_cast<const uint8_t*>(params.data.data());
  size_t input_len = params.data.size();

  switch (params.variant) {
    case TurboShakeVariant::TurboSHAKE128:
      TurboSHAKE128(input,
                    input_len,
                    params.domain_separation,
                    reinterpret_cast<uint8_t*>(buf),
                    params.output_length);
      break;
    case TurboShakeVariant::TurboSHAKE256:
      TurboSHAKE256(input,
                    input_len,
                    params.domain_separation,
                    reinterpret_cast<uint8_t*>(buf),
                    params.output_length);
      break;
  }

  *out = ByteSource::Allocated(buf, params.output_length);
  return true;
}

MaybeLocal<Value> TurboShakeTraits::EncodeOutput(Environment* env,
                                                 const TurboShakeConfig& params,
                                                 ByteSource* out) {
  return out->ToArrayBuffer(env);
}

// ============================================================================
// KangarooTwelve bindings
// ============================================================================

KangarooTwelveConfig::KangarooTwelveConfig(
    KangarooTwelveConfig&& other) noexcept
    : job_mode(other.job_mode),
      variant(other.variant),
      output_length(other.output_length),
      data(std::move(other.data)),
      customization(std::move(other.customization)) {}

KangarooTwelveConfig& KangarooTwelveConfig::operator=(
    KangarooTwelveConfig&& other) noexcept {
  if (&other == this) return *this;
  this->~KangarooTwelveConfig();
  return *new (this) KangarooTwelveConfig(std::move(other));
}

void KangarooTwelveConfig::MemoryInfo(MemoryTracker* tracker) const {
  if (job_mode == kCryptoJobAsync) {
    // TODO(addaleax): Implement MemoryRetainer protocol for ByteSource
    tracker->TrackFieldWithSize("data", data.size());
    tracker->TrackFieldWithSize("customization", customization.size());
  }
}

Maybe<void> KangarooTwelveTraits::AdditionalConfig(
    CryptoJobMode mode,
    const FunctionCallbackInfo<Value>& args,
    unsigned int offset,
    KangarooTwelveConfig* params) {
  Environment* env = Environment::GetCurrent(args);

  params->job_mode = mode;

  // args[offset + 0] = algorithm name (string)
  CHECK(args[offset]->IsString());
  Utf8Value algorithm_name(env->isolate(), args[offset]);
  std::string_view alg = algorithm_name.ToStringView();

  if (alg == "KT128") {
    params->variant = KangarooTwelveVariant::KT128;
  } else if (alg == "KT256") {
    params->variant = KangarooTwelveVariant::KT256;
  } else {
    UNREACHABLE();
  }

  // args[offset + 1] = customization (BufferSource or undefined)
  if (!args[offset + 1]->IsUndefined()) {
    ArrayBufferOrViewContents<char> customization(args[offset + 1]);
    if (!customization.CheckSizeInt32()) [[unlikely]] {
      THROW_ERR_OUT_OF_RANGE(env, "customization is too big");
      return Nothing<void>();
    }
    params->customization = mode == kCryptoJobAsync
                                ? customization.ToCopy()
                                : customization.ToByteSource();
  }

  // args[offset + 2] = output length in bytes (uint32)
  CHECK(args[offset + 2]->IsUint32());
  params->output_length = args[offset + 2].As<Uint32>()->Value();

  // args[offset + 3] = data (ArrayBuffer/View)
  ArrayBufferOrViewContents<char> data(args[offset + 3]);
  if (!data.CheckSizeInt32()) [[unlikely]] {
    THROW_ERR_OUT_OF_RANGE(env, "data is too big");
    return Nothing<void>();
  }
  params->data = mode == kCryptoJobAsync ? data.ToCopy() : data.ToByteSource();

  return JustVoid();
}

bool KangarooTwelveTraits::DeriveBits(Environment* env,
                                      const KangarooTwelveConfig& params,
                                      ByteSource* out,
                                      CryptoJobMode mode,
                                      CryptoErrorStore* errors) {
  CHECK_GT(params.output_length, 0);

  const uint8_t* input = reinterpret_cast<const uint8_t*>(params.data.data());
  size_t input_len = params.data.size();

  const uint8_t* custom =
      reinterpret_cast<const uint8_t*>(params.customization.data());
  size_t custom_len = params.customization.size();

  // Guard against size_t overflow in KangarooTwelve's s_len computation:
  //   s_len = msg_len + custom_len + LengthEncode(custom_len).size()
  // LengthEncode produces at most sizeof(size_t) + 1 bytes.
  static constexpr size_t kMaxLengthEncodeSize = sizeof(size_t) + 1;
  if (input_len > SIZE_MAX - custom_len ||
      input_len + custom_len > SIZE_MAX - kMaxLengthEncodeSize) {
    errors->Insert(NodeCryptoError::DERIVING_BITS_FAILED);
    return false;
  }

  char* buf = MallocOpenSSL<char>(params.output_length);

  switch (params.variant) {
    case KangarooTwelveVariant::KT128:
      KT128(input,
            input_len,
            custom,
            custom_len,
            reinterpret_cast<uint8_t*>(buf),
            params.output_length);
      break;
    case KangarooTwelveVariant::KT256:
      KT256(input,
            input_len,
            custom,
            custom_len,
            reinterpret_cast<uint8_t*>(buf),
            params.output_length);
      break;
  }

  *out = ByteSource::Allocated(buf, params.output_length);
  return true;
}

MaybeLocal<Value> KangarooTwelveTraits::EncodeOutput(
    Environment* env, const KangarooTwelveConfig& params, ByteSource* out) {
  return out->ToArrayBuffer(env);
}

// ============================================================================
// Registration
// ============================================================================

void TurboShake::Initialize(Environment* env, Local<Object> target) {
  TurboShakeJob::Initialize(env, target);
  KangarooTwelveJob::Initialize(env, target);
}

void TurboShake::RegisterExternalReferences(
    ExternalReferenceRegistry* registry) {
  TurboShakeJob::RegisterExternalReferences(registry);
  KangarooTwelveJob::RegisterExternalReferences(registry);
}

}  // namespace node::crypto
