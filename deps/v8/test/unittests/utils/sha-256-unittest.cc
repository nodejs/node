// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/utils/sha-256.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "src/base/strings.h"
#include "src/base/vector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {

namespace {

std::string DigestToHex(const uint8_t digest[kSizeOfSha256Digest]) {
  char formatted[kSizeOfFormattedSha256Digest];
  for (size_t i = 0; i < kSizeOfSha256Digest; ++i) {
    base::SNPrintF(base::Vector<char>(formatted + i * 2, 3), "%02x", digest[i]);
  }
  return std::string(formatted, kSizeOfSha256Digest * 2);
}

void ExpectSha256(const void* data, size_t len, const char* expected_hex) {
  uint8_t digest[kSizeOfSha256Digest];
  EXPECT_EQ(digest, Sha256Hash(data, len, digest));
  EXPECT_EQ(expected_hex, DigestToHex(digest));

  uint8_t stream_digest[kSizeOfSha256Digest];
  Sha256Stream stream;
  stream.Update(data, len);
  stream.Final(stream_digest);
  EXPECT_EQ(expected_hex, DigestToHex(stream_digest));
}

}  // namespace

TEST(Sha256Test, EmptyAndNullInput) {
  constexpr const char* kEmptySha256 =
      "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";

  ExpectSha256("", 0, kEmptySha256);
  ExpectSha256(nullptr, 0, kEmptySha256);

  // Finalizing without calling Update() produces the empty digest.
  uint8_t digest[kSizeOfSha256Digest];
  Sha256Stream empty_stream;
  empty_stream.Final(digest);
  EXPECT_EQ(kEmptySha256, DigestToHex(digest));

  // Interleaving empty (nullptr, 0) updates does not perturb state.
  Sha256Stream interleaved;
  interleaved.Update(nullptr, 0);
  interleaved.Update("a", 1);
  interleaved.Update(nullptr, 0);
  interleaved.Update("bc", 2);
  interleaved.Update(nullptr, 0);
  interleaved.Final(digest);
  EXPECT_EQ("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
            DigestToHex(digest));
}

TEST(Sha256Test, NistVectors) {
  ExpectSha256(
      "abc", 3,
      "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

  // 56 bytes (55+1 crosses the length-padding threshold into a second block).
  constexpr const char* k56Bytes =
      "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
  ExpectSha256(
      k56Bytes, strlen(k56Bytes),
      "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");

  // Exact 64-byte single block.
  constexpr const char* k64Bytes =
      "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
  ExpectSha256(
      k64Bytes, strlen(k64Bytes),
      "a8ae6e6ee929abea3afcfc5258c8ccd6f85273e0d4626d26c7279f3250f77c8e");

  // 112 bytes (multi-block NIST FIPS 180-4 vector).
  constexpr const char* k112Bytes =
      "abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnop"
      "jklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu";
  ExpectSha256(
      k112Bytes, strlen(k112Bytes),
      "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1");
}

TEST(Sha256Test, StreamingAcrossBlockBoundaries) {
  std::vector<uint8_t> payload(300);
  for (size_t i = 0; i < payload.size(); ++i) {
    payload[i] = static_cast<uint8_t>(i * 31 + 7);
  }

  uint8_t expected[kSizeOfSha256Digest];
  Sha256Hash(payload.data(), payload.size(), expected);
  EXPECT_EQ("4ebe2a8bd5ece93fb899b68e8a5fe64464b2058a5ddca6c079bc907930aa3003",
            DigestToHex(expected));

  for (size_t chunk_size :
       {size_t{1}, size_t{7}, size_t{31}, size_t{55}, size_t{56}, size_t{63},
        size_t{64}, size_t{65}, size_t{128}, payload.size()}) {
    Sha256Stream stream;
    for (size_t offset = 0; offset < payload.size(); offset += chunk_size) {
      size_t n = std::min(chunk_size, payload.size() - offset);
      stream.Update(payload.data() + offset, n);
    }
    uint8_t actual[kSizeOfSha256Digest];
    stream.Final(actual);
    EXPECT_EQ(0, memcmp(expected, actual, kSizeOfSha256Digest))
        << "chunk_size=" << chunk_size;
  }
}

}  // namespace internal
}  // namespace v8
