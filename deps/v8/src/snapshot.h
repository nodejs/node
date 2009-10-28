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
  static bool Initialize2(const char* snapshot_file = NULL);

  // Returns whether or not the snapshot is enabled.
  static bool IsEnabled() { return size_ != 0; }

  // Write snapshot to the given file. Returns true if snapshot was written
  // successfully.
  static bool WriteToFile(const char* snapshot_file);
  static bool WriteToFile2(const char* snapshot_file);

 private:
  static const byte data_[];
  static int size_;

  static bool Deserialize(const byte* content, int len);
  static bool Deserialize2(const byte* content, int len);

  DISALLOW_IMPLICIT_CONSTRUCTORS(Snapshot);
};

} }  // namespace v8::internal

#endif  // V8_SNAPSHOT_H_
