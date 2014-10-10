// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/isolate.h"

#ifndef V8_SNAPSHOT_H_
#define V8_SNAPSHOT_H_

namespace v8 {
namespace internal {

class Snapshot {
 public:
  // Initialize the Isolate from the internal snapshot. Returns false if no
  // snapshot could be found.
  static bool Initialize(Isolate* isolate);

  static bool HaveASnapshotToStartFrom();

  // Create a new context using the internal partial snapshot.
  static Handle<Context> NewContextFromSnapshot(Isolate* isolate);

  // These methods support COMPRESS_STARTUP_DATA_BZ2.
  static const byte* data() { return data_; }
  static int size() { return size_; }
  static int raw_size() { return raw_size_; }
  static void set_raw_data(const byte* raw_data) {
    raw_data_ = raw_data;
  }
  static const byte* context_data() { return context_data_; }
  static int context_size() { return context_size_; }
  static int context_raw_size() { return context_raw_size_; }
  static void set_context_raw_data(
      const byte* context_raw_data) {
    context_raw_data_ = context_raw_data;
  }

 private:
  static const byte data_[];
  static const byte* raw_data_;
  static const byte context_data_[];
  static const byte* context_raw_data_;
  static const int new_space_used_;
  static const int pointer_space_used_;
  static const int data_space_used_;
  static const int code_space_used_;
  static const int map_space_used_;
  static const int cell_space_used_;
  static const int property_cell_space_used_;
  static const int lo_space_used_;
  static const int context_new_space_used_;
  static const int context_pointer_space_used_;
  static const int context_data_space_used_;
  static const int context_code_space_used_;
  static const int context_map_space_used_;
  static const int context_cell_space_used_;
  static const int context_property_cell_space_used_;
  static const int context_lo_space_used_;
  static const int size_;
  static const int raw_size_;
  static const int context_size_;
  static const int context_raw_size_;

  static void ReserveSpaceForLinkedInSnapshot(Deserializer* deserializer);

  DISALLOW_IMPLICIT_CONSTRUCTORS(Snapshot);
};

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
void SetSnapshotFromFile(StartupData* snapshot_blob);
#endif

} }  // namespace v8::internal

#endif  // V8_SNAPSHOT_H_
