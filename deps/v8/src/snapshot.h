// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/isolate.h"

#ifndef V8_SNAPSHOT_H_
#define V8_SNAPSHOT_H_

namespace v8 {
namespace internal {

class Snapshot : public AllStatic {
 public:
  // Initialize the Isolate from the internal snapshot. Returns false if no
  // snapshot could be found.
  static bool Initialize(Isolate* isolate);
  // Create a new context using the internal partial snapshot.
  static Handle<Context> NewContextFromSnapshot(Isolate* isolate);

  static bool HaveASnapshotToStartFrom();

  // To be implemented by the snapshot source.
  static const v8::StartupData SnapshotBlob();

  static v8::StartupData CreateSnapshotBlob(
      const Vector<const byte> startup_data,
      const Vector<const byte> context_data);

  static Vector<const byte> ExtractStartupData(const v8::StartupData* data);
  static Vector<const byte> ExtractContextData(const v8::StartupData* data);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Snapshot);
};

#ifdef V8_USE_EXTERNAL_STARTUP_DATA
void SetSnapshotFromFile(StartupData* snapshot_blob);
#endif

} }  // namespace v8::internal

#endif  // V8_SNAPSHOT_H_
