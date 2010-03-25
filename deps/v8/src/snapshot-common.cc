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

// The common functionality when building with or without snapshots.

#include "v8.h"

#include "api.h"
#include "serialize.h"
#include "snapshot.h"
#include "platform.h"

namespace v8 {
namespace internal {

bool Snapshot::Deserialize(const byte* content, int len) {
  SnapshotByteSource source(content, len);
  Deserializer deserializer(&source);
  return V8::Initialize(&deserializer);
}


bool Snapshot::Initialize(const char* snapshot_file) {
  if (snapshot_file) {
    int len;
    byte* str = ReadBytes(snapshot_file, &len);
    if (!str) return false;
    Deserialize(str, len);
    DeleteArray(str);
    return true;
  } else if (size_ > 0) {
    Deserialize(data_, size_);
    return true;
  }
  return false;
}


Handle<Context> Snapshot::NewContextFromSnapshot() {
  if (context_size_ == 0) {
    return Handle<Context>();
  }
  Heap::ReserveSpace(new_space_used_,
                     pointer_space_used_,
                     data_space_used_,
                     code_space_used_,
                     map_space_used_,
                     cell_space_used_,
                     large_space_used_);
  SnapshotByteSource source(context_data_, context_size_);
  Deserializer deserializer(&source);
  Object* root;
  deserializer.DeserializePartial(&root);
  CHECK(root->IsContext());
  return Handle<Context>(Context::cast(root));
}

} }  // namespace v8::internal
