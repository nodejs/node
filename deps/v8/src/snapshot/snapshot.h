// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_SNAPSHOT_H_
#define V8_SNAPSHOT_SNAPSHOT_H_

#include "src/snapshot/serialize.h"

namespace v8 {
namespace internal {

// Forward declarations.
class Isolate;
class PartialSerializer;
class StartupSerializer;

class Snapshot : public AllStatic {
 public:
  class Metadata {
   public:
    explicit Metadata(uint32_t data = 0) : data_(data) {}
    bool embeds_script() { return EmbedsScriptBits::decode(data_); }
    void set_embeds_script(bool v) {
      data_ = EmbedsScriptBits::update(data_, v);
    }

    uint32_t& RawValue() { return data_; }

   private:
    class EmbedsScriptBits : public BitField<bool, 0, 1> {};
    uint32_t data_;
  };

  // Initialize the Isolate from the internal snapshot. Returns false if no
  // snapshot could be found.
  static bool Initialize(Isolate* isolate);
  // Create a new context using the internal partial snapshot.
  static MaybeHandle<Context> NewContextFromSnapshot(
      Isolate* isolate, Handle<JSGlobalProxy> global_proxy);

  static bool HaveASnapshotToStartFrom(Isolate* isolate);

  static bool EmbedsScript(Isolate* isolate);

  static uint32_t SizeOfFirstPage(Isolate* isolate, AllocationSpace space);


  // To be implemented by the snapshot source.
  static const v8::StartupData* DefaultSnapshotBlob();

  static v8::StartupData CreateSnapshotBlob(
      const StartupSerializer& startup_ser,
      const PartialSerializer& context_ser, Snapshot::Metadata metadata);

#ifdef DEBUG
  static bool SnapshotIsValid(v8::StartupData* snapshot_blob);
#endif  // DEBUG

 private:
  static Vector<const byte> ExtractStartupData(const v8::StartupData* data);
  static Vector<const byte> ExtractContextData(const v8::StartupData* data);
  static Metadata ExtractMetadata(const v8::StartupData* data);

  // Snapshot blob layout:
  // [0] metadata
  // [1 - 6] pre-calculated first page sizes for paged spaces
  // [7] serialized start up data length
  // ... serialized start up data
  // ... serialized context data

  static const int kNumPagedSpaces = LAST_PAGED_SPACE - FIRST_PAGED_SPACE + 1;

  static const int kMetadataOffset = 0;
  static const int kFirstPageSizesOffset = kMetadataOffset + kInt32Size;
  static const int kStartupLengthOffset =
      kFirstPageSizesOffset + kNumPagedSpaces * kInt32Size;
  static const int kStartupDataOffset = kStartupLengthOffset + kInt32Size;

  static int ContextOffset(int startup_length) {
    return kStartupDataOffset + startup_length;
  }

  DISALLOW_IMPLICIT_CONSTRUCTORS(Snapshot);
};

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
void SetSnapshotFromFile(StartupData* snapshot_blob);
#endif

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_SNAPSHOT_H_
