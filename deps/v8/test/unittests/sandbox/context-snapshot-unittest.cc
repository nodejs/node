// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "src/base/memory.h"
#include "src/base/numerics/safe_conversions.h"
#include "src/base/vector.h"
#include "src/heap/factory.h"
#include "src/objects/js-objects-inl.h"
#include "src/roots/roots.h"
#include "src/snapshot/context-deserializer.h"
#include "src/snapshot/serializer-deserializer.h"
#include "src/snapshot/snapshot-data.h"
#include "src/snapshot/snapshot-source-sink.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#ifdef V8_ENABLE_SANDBOX

namespace v8::internal {

// In official builds we drop the message in CHECK so we cannot check for it in
// these tests.
#ifdef OFFICIAL_BUILD
#define EXPECTED_CHECK_WITH_MSG(msg) ""
#else
#define EXPECTED_CHECK_WITH_MSG(msg) msg
#endif

namespace {

// Exposes the bytecodes of the serializer.
struct SnapshotBytecodes : public SerializerDeserializer {
  using SerializerDeserializer::Bytecode;
  using enum Bytecode;
  using SerializerDeserializer::IsTrustedBytecode;
  using SerializerDeserializer::NewObject;
};

// Pads |sink| as Serializer::Pad does.
void Pad(SnapshotByteSink* sink) {
  for (int i = 0; i < 3; i++) sink->Put(SnapshotBytecodes::kNop, "Padding");
  while (!IsAligned(sink->Position(), kPointerAlignment)) {
    sink->Put(SnapshotBytecodes::kNop, "Padding");
  }
}

// Returns the bytes of a context snapshot whose untrusted section is
// |untrusted|, and whose trusted section has no external references, exports
// or embedder fields.
std::vector<uint8_t> MakeContextSnapshot(SnapshotByteSink* untrusted) {
  // |trusted| also holds the external reference count, since the trusted
  // section is padded as a whole.
  SnapshotByteSink trusted;
  trusted.PutUint30(0, "ExternalReferenceCount");
  trusted.Put(SnapshotBytecodes::kSynchronize, "EndOfTrustedExports");
  Pad(&trusted);
  Pad(untrusted);

  constexpr uint32_t kHeaderSize =
      SnapshotData::kTrustedPayloadLengthOffset + kUInt32Size;
  const uint32_t trusted_length =
      base::checked_cast<uint32_t>(trusted.data()->size());
  const uint32_t payload_length =
      trusted_length + base::checked_cast<uint32_t>(untrusted->data()->size());
  std::vector<uint8_t> bytes(kHeaderSize + payload_length);
  auto set_header = [&bytes](uint32_t offset, uint32_t value) {
    base::WriteLittleEndianValue<uint32_t>(
        reinterpret_cast<Address>(bytes.data() + offset), value);
  };
  set_header(SnapshotData::kMagicNumberOffset, SnapshotData::kMagicNumber);
  set_header(SnapshotData::kPayloadLengthOffset, payload_length);
  set_header(SnapshotData::kTrustedPayloadLengthOffset, trusted_length);
  std::copy(trusted.data()->begin(), trusted.data()->end(),
            bytes.begin() + kHeaderSize);
  std::copy(untrusted->data()->begin(), untrusted->data()->end(),
            bytes.begin() + kHeaderSize + trusted_length);
  return bytes;
}

}  // namespace

// The context deserializer can run while JavaScript runs, so it reads the
// untrusted section of a context snapshot like that of a code cache.
class ContextSnapshotUntrustedSectionTest : public TestWithContext {
 protected:
  void Deserialize(SnapshotByteSink* untrusted) {
    std::vector<uint8_t> bytes = MakeContextSnapshot(untrusted);
    SnapshotData snapshot(base::VectorOf(bytes));
    DirectHandle<JSGlobalProxy> global_proxy =
        i_isolate()->factory()->NewUninitializedJSGlobalProxy(
            JSGlobalProxy::SizeWithEmbedderFields(0));
    USE(ContextDeserializer::DeserializeContext(
        i_isolate(), &snapshot, 0, false, global_proxy,
        DeserializeEmbedderFieldsCallback()));
  }
};

TEST_F(ContextSnapshotUntrustedSectionTest, TrustedBytecodes) {
  for (int i = 0; i <= kMaxUInt8; i++) {
    SnapshotBytecodes::Bytecode bytecode =
        static_cast<SnapshotBytecodes::Bytecode>(i);
    if (!SnapshotBytecodes::IsTrustedBytecode(bytecode)) continue;
    SnapshotByteSink untrusted;
    untrusted.Put(bytecode, "");
    ASSERT_DEATH_IF_SUPPORTED(Deserialize(&untrusted),
                              EXPECTED_CHECK_WITH_MSG("InTrustedSection"));
  }
}

TEST_F(ContextSnapshotUntrustedSectionTest, DisallowedSpaces) {
  for (SnapshotSpace space : {SnapshotSpace::kTrusted, SnapshotSpace::kCode}) {
    SnapshotByteSink object;
    object.Put(SnapshotBytecodes::NewObject::Encode(space), "");
    object.Put(SnapshotBytecodes::kRootArray, "");
    object.PutUint30(static_cast<uint32_t>(RootIndex::kFixedArrayMap), "");
    object.PutUint30(2, "");
    ASSERT_DEATH_IF_SUPPORTED(Deserialize(&object),
                              EXPECTED_CHECK_WITH_MSG("AllocationType::kOld"));
  }
  // Allocated in read-only space.
  SnapshotByteSink meta_map;
  meta_map.Put(SnapshotBytecodes::kNewContextlessMetaMap, "");
  meta_map.PutUint30(MAP_TYPE, "");
  ASSERT_DEATH_IF_SUPPORTED(Deserialize(&meta_map),
                            EXPECTED_CHECK_WITH_MSG("AllocationType::kOld"));
}

}  // namespace v8::internal

#endif  // V8_ENABLE_SANDBOX
