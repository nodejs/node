// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "include/v8-script.h"
#include "src/base/memory.h"
#include "src/codegen/compilation-cache.h"
#include "src/codegen/external-reference.h"
#include "src/heap/factory-inl.h"
#include "src/objects/bytecode-array.h"
#include "src/objects/code.h"
#include "src/roots/roots.h"
#include "src/snapshot/code-serializer.h"
#include "src/snapshot/serializer-deserializer.h"
#include "src/snapshot/snapshot-source-sink.h"
#include "src/snapshot/snapshot-utils.h"
#include "test/common/flag-utils.h"
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

constexpr char kSource[] = "function f() { return 42; }; f();";

// Exposes the bytecodes of the serializer.
struct Bytecodes : public SerializerDeserializer {
  using SerializerDeserializer::Bytecode;
  using enum Bytecode;
  using SerializerDeserializer::IsTrustedBytecode;
  using SerializerDeserializer::NewObject;
};

void SetHeaderValue(std::vector<uint8_t>* data, uint32_t offset,
                    uint32_t value) {
  base::WriteLittleEndianValue<uint32_t>(
      reinterpret_cast<Address>(data->data() + offset), value);
}

uint32_t UntrustedSectionOffset(const std::vector<uint8_t>& data) {
  return SerializedCodeData::kHeaderSize +
         base::ReadLittleEndianValue<uint32_t>(reinterpret_cast<Address>(
             data.data() + SerializedCodeData::kTrustedPayloadLengthOffset));
}

// Updates the payload length and the checksum after changing the payload.
void UpdateHeader(std::vector<uint8_t>* data) {
  base::Vector<const uint8_t> payload =
      base::VectorOf(*data).SubVectorFrom(SerializedCodeData::kHeaderSize);
  SetHeaderValue(data, SerializedCodeData::kPayloadLengthOffset,
                 static_cast<uint32_t>(payload.size()));
  SetHeaderValue(data, SerializedCodeData::kChecksumOffset, Checksum(payload));
}

// Returns a BytecodeWrapper that refers to the trusted export at |index|, as
// the serializer encodes it.
std::vector<uint8_t> BytecodeWrapperWithExport(uint32_t index) {
  SnapshotByteSink object;
  object.Put(Bytecodes::NewObject::Encode(SnapshotSpace::kOld), "");
  object.Put(Bytecodes::kRootArray, "");
  object.PutUint30(static_cast<uint32_t>(RootIndex::kBytecodeWrapperMap), "");
  object.Put(Bytecodes::kTrustedPointerPrefix, "");
  object.Put(Bytecodes::kTrustedExportRef, "");
  object.PutUint30(index, "");
  return *object.data();
}

}  // namespace

// Corrupted in-sandbox objects must not make the code serializer put arbitrary
// trusted objects or external pointers into the trusted section.
class CodeCacheTrustedSectionTest : public TestWithContext {
 protected:
  // Compiles kSource. The script name is an in-sandbox object that the code
  // serializer visits.
  void Compile() {
    v8::ScriptOrigin origin(NewString(std::string(256, 'n').c_str()));
    v8::ScriptCompiler::Source source(NewString(kSource), origin);
    sfi_ = indirect_handle(
        Utils::OpenDirectHandle(
            *v8::ScriptCompiler::CompileUnboundScript(isolate(), &source)
                 .ToLocalChecked()),
        i_isolate());
    name_ = handle(Cast<HeapObject>(Cast<Script>(sfi_->script())->name()),
                   i_isolate());
  }

  ScriptCompiler::CachedData* Serialize() {
    return CodeSerializer::Serialize(i_isolate(), sfi_);
  }

  // Serializes with the script name disguised as an object with |map|.
  ScriptCompiler::CachedData* SerializeWithNameMap(Tagged<Map> map) {
    name_->set_map_word(map, kRelaxedStore);
    return Serialize();
  }

  Handle<SharedFunctionInfo> sfi_;
  Handle<HeapObject> name_;
};

TEST_F(CodeCacheTrustedSectionTest, InSandboxBaselineCode) {
  Compile();
  ASSERT_DEATH_IF_SUPPORTED(
      {
        name_->WriteField<uint32_t>(
            Code::kFlagsOffset, Code::KindField::encode(CodeKind::BASELINE));
        SerializeWithNameMap(ReadOnlyRoots(i_isolate()).code_map());
      },
      EXPECTED_CHECK_WITH_MSG("OutsideSandbox"));
}

TEST_F(CodeCacheTrustedSectionTest, InSandboxBuiltinCode) {
  Compile();
  ASSERT_DEATH_IF_SUPPORTED(
      {
        name_->WriteField<uint32_t>(Code::kFlagsOffset,
                                    Code::KindField::encode(CodeKind::BUILTIN));
        SerializeWithNameMap(ReadOnlyRoots(i_isolate()).code_map());
      },
      EXPECTED_CHECK_WITH_MSG("OutsideSandbox"));
}

TEST_F(CodeCacheTrustedSectionTest, InSandboxInterpreterData) {
  FlagScope<bool> flag_scope(&v8_flags.interpreted_frames_native_stack, true);
  Compile();
  ASSERT_DEATH_IF_SUPPORTED(
      SerializeWithNameMap(ReadOnlyRoots(i_isolate()).interpreter_data_map()),
      EXPECTED_CHECK_WITH_MSG("OutsideSandbox"));
}

TEST_F(CodeCacheTrustedSectionTest, InSandboxExposedTrustedObject) {
  Compile();
  ASSERT_DEATH_IF_SUPPORTED(
      SerializeWithNameMap(ReadOnlyRoots(i_isolate()).bytecode_array_map()),
      EXPECTED_CHECK_WITH_MSG("OutsideSandbox"));
}

// The address of the Foreign is a known external reference, but its tag must
// not be in a code cache.
TEST_F(CodeCacheTrustedSectionTest, NonSerializableExternalPointerTag) {
  Compile();
  ASSERT_DEATH_IF_SUPPORTED(
      {
        Cast<Script>(sfi_->script())
            ->set_name(*i_isolate()->factory()->NewForeign<kGenericForeignTag>(
                ExternalReference::isolate_address(i_isolate()).address()));
        Serialize();
      },
      EXPECTED_CHECK_WITH_MSG("kSerializableExternalPointerTags"));
}

// Out-of-bounds indices into the deserializer's vectors are caught by the
// libc++ hardening, which crashes without a message.
class CodeCacheUntrustedSectionTest : public TestWithContext {
 protected:
  std::vector<uint8_t> ProduceCodeCache() {
    i_isolate()->compilation_cache()->DisableScriptAndEval();
    v8::ScriptCompiler::Source source(NewString(kSource));
    std::unique_ptr<v8::ScriptCompiler::CachedData> cache(
        v8::ScriptCompiler::CreateCodeCache(
            v8::ScriptCompiler::CompileUnboundScript(isolate(), &source)
                .ToLocalChecked()));
    return std::vector<uint8_t>(cache->data, cache->data + cache->length);
  }

  // Returns a code cache for kSource whose untrusted section is replaced by
  // |root|.
  std::vector<uint8_t> MakeCodeCache(const SnapshotByteSink& root) {
    std::vector<uint8_t> data = ProduceCodeCache();
    data.resize(UntrustedSectionOffset(data));
    data.insert(data.end(), root.data()->begin(), root.data()->end());
    // End of the root's deferred objects and of the untrusted section.
    data.insert(data.end(), 2, Bytecodes::kSynchronize);
    // Padding as added by Serializer::Pad.
    data.insert(data.end(), 3, Bytecodes::kNop);
    while (!IsAligned(data.size(), kPointerAlignment)) {
      data.push_back(Bytecodes::kNop);
    }
    UpdateHeader(&data);
    return data;
  }

  void Consume(const std::vector<uint8_t>& data) {
    v8::ScriptCompiler::Source source(
        NewString(kSource), new v8::ScriptCompiler::CachedData(
                                data.data(), static_cast<int>(data.size())));
    USE(v8::ScriptCompiler::CompileUnboundScript(
        isolate(), &source, v8::ScriptCompiler::kConsumeCodeCache));
  }
};

TEST_F(CodeCacheUntrustedSectionTest, RootIndexOutOfBounds) {
  SnapshotByteSink untrusted;
  untrusted.Put(Bytecodes::kRootArray, "");
  untrusted.PutUint30(RootsTable::kEntriesCount, "");
  std::vector<uint8_t> cache = MakeCodeCache(untrusted);
  ASSERT_DEATH_IF_SUPPORTED(Consume(cache),
                            EXPECTED_CHECK_WITH_MSG("kEntriesCount"));
}

TEST_F(CodeCacheUntrustedSectionTest, ExternalReferenceIndexOutOfBounds) {
  // kSource has no external references.
  SnapshotByteSink untrusted;
  untrusted.Put(Bytecodes::kTrustedExternalRef, "");
  untrusted.PutUint30(0, "");
  std::vector<uint8_t> cache = MakeCodeCache(untrusted);
  ASSERT_DEATH_IF_SUPPORTED(Consume(cache), "");
}

TEST_F(CodeCacheUntrustedSectionTest, BackingStoreLongerThanSection) {
  SnapshotByteSink untrusted;
  untrusted.Put(Bytecodes::kOffHeapBackingStore, "");
  untrusted.PutUint32(1024, "");
  std::vector<uint8_t> cache = MakeCodeCache(untrusted);
  ASSERT_DEATH_IF_SUPPORTED(Consume(cache),
                            EXPECTED_CHECK_WITH_MSG("byte_length"));
}

TEST_F(CodeCacheUntrustedSectionTest, TrustedExportIndexOutOfBounds) {
  std::vector<uint8_t> cache = ProduceCodeCache();
  // The wrapper of the top-level bytecode, which is export 0.
  std::vector<uint8_t> wrapper = BytecodeWrapperWithExport(0);
  auto it = std::search(cache.begin() + UntrustedSectionOffset(cache),
                        cache.end(), wrapper.begin(), wrapper.end());
  CHECK(it != cache.end());
  // Out of bounds, and encoded in one byte like 0.
  std::vector<uint8_t> out_of_bounds = BytecodeWrapperWithExport(63);
  CHECK_EQ(out_of_bounds.size(), wrapper.size());
  std::copy(out_of_bounds.begin(), out_of_bounds.end(), it);
  UpdateHeader(&cache);
  ASSERT_DEATH_IF_SUPPORTED(Consume(cache), "");
}

// Untrusted references are only valid in the trusted section, so there are no
// objects to refer to yet.
TEST_F(CodeCacheUntrustedSectionTest, UntrustedRefInUntrustedSection) {
  SnapshotByteSink untrusted;
  untrusted.Put(Bytecodes::kUntrustedRef, "");
  untrusted.PutUint30(0, "");
  std::vector<uint8_t> cache = MakeCodeCache(untrusted);
  ASSERT_DEATH_IF_SUPPORTED(Consume(cache), "");
}

TEST_F(CodeCacheUntrustedSectionTest, DisallowedSpaces) {
  std::vector<std::vector<uint8_t>> caches;
  for (SnapshotSpace space : {SnapshotSpace::kTrusted, SnapshotSpace::kCode}) {
    SnapshotByteSink object;
    object.Put(Bytecodes::NewObject::Encode(space), "");
    object.Put(Bytecodes::kRootArray, "");
    object.PutUint30(static_cast<uint32_t>(RootIndex::kFixedArrayMap), "");
    object.PutUint30(2, "");
    caches.push_back(MakeCodeCache(object));
  }
  // Allocated in read-only space.
  SnapshotByteSink meta_map;
  meta_map.Put(Bytecodes::kNewContextlessMetaMap, "");
  meta_map.PutUint30(MAP_TYPE, "");
  caches.push_back(MakeCodeCache(meta_map));
  for (const std::vector<uint8_t>& cache : caches) {
    ASSERT_DEATH_IF_SUPPORTED(Consume(cache),
                              EXPECTED_CHECK_WITH_MSG("AllocationType::kOld"));
  }
}

TEST_F(CodeCacheUntrustedSectionTest, TrustedBytecodes) {
  for (int i = 0; i <= kMaxUInt8; i++) {
    Bytecodes::Bytecode bytecode = static_cast<Bytecodes::Bytecode>(i);
    if (!Bytecodes::IsTrustedBytecode(bytecode)) continue;
    SnapshotByteSink untrusted;
    untrusted.Put(bytecode, "");
    std::vector<uint8_t> cache = MakeCodeCache(untrusted);
    ASSERT_DEATH_IF_SUPPORTED(Consume(cache),
                              EXPECTED_CHECK_WITH_MSG("InTrustedSection"));
  }
}

}  // namespace v8::internal

#endif  // V8_ENABLE_SANDBOX
