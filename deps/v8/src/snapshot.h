// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "isolate.h"

#ifndef V8_SNAPSHOT_H_
#define V8_SNAPSHOT_H_

namespace v8 {
namespace internal {

class Snapshot {
 public:
  // Initialize the VM from the given snapshot file. If snapshot_file is
  // NULL, use the internal snapshot instead. Returns false if no snapshot
  // could be found.
  static bool Initialize(const char* snapshot_file = NULL);

  static bool HaveASnapshotToStartFrom();

  // Create a new context using the internal partial snapshot.
  static Handle<Context> NewContextFromSnapshot();

  // Returns whether or not the snapshot is enabled.
  static bool IsEnabled() { return size_ != 0; }

  // Write snapshot to the given file. Returns true if snapshot was written
  // successfully.
  static bool WriteToFile(const char* snapshot_file);

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
  static const int context_new_space_used_;
  static const int context_pointer_space_used_;
  static const int context_data_space_used_;
  static const int context_code_space_used_;
  static const int context_map_space_used_;
  static const int context_cell_space_used_;
  static const int context_property_cell_space_used_;
  static const int size_;
  static const int raw_size_;
  static const int context_size_;
  static const int context_raw_size_;

  static void ReserveSpaceForLinkedInSnapshot(Deserializer* deserializer);

  DISALLOW_IMPLICIT_CONSTRUCTORS(Snapshot);
};

} }  // namespace v8::internal

#endif  // V8_SNAPSHOT_H_
