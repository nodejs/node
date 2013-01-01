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


static void ReserveSpaceForSnapshot(Deserializer* deserializer,
                                    const char* file_name) {
  int file_name_length = StrLength(file_name) + 10;
  Vector<char> name = Vector<char>::New(file_name_length + 1);
  OS::SNPrintF(name, "%s.size", file_name);
  FILE* fp = OS::FOpen(name.start(), "r");
  CHECK_NE(NULL, fp);
  int new_size, pointer_size, data_size, code_size, map_size, cell_size;
#ifdef _MSC_VER
  // Avoid warning about unsafe fscanf from MSVC.
  // Please note that this is only fine if %c and %s are not being used.
#define fscanf fscanf_s
#endif
  CHECK_EQ(1, fscanf(fp, "new %d\n", &new_size));
  CHECK_EQ(1, fscanf(fp, "pointer %d\n", &pointer_size));
  CHECK_EQ(1, fscanf(fp, "data %d\n", &data_size));
  CHECK_EQ(1, fscanf(fp, "code %d\n", &code_size));
  CHECK_EQ(1, fscanf(fp, "map %d\n", &map_size));
  CHECK_EQ(1, fscanf(fp, "cell %d\n", &cell_size));
#ifdef _MSC_VER
#undef fscanf
#endif
  fclose(fp);
  deserializer->set_reservation(NEW_SPACE, new_size);
  deserializer->set_reservation(OLD_POINTER_SPACE, pointer_size);
  deserializer->set_reservation(OLD_DATA_SPACE, data_size);
  deserializer->set_reservation(CODE_SPACE, code_size);
  deserializer->set_reservation(MAP_SPACE, map_size);
  deserializer->set_reservation(CELL_SPACE, cell_size);
  name.Dispose();
}


void Snapshot::ReserveSpaceForLinkedInSnapshot(Deserializer* deserializer) {
  deserializer->set_reservation(NEW_SPACE, new_space_used_);
  deserializer->set_reservation(OLD_POINTER_SPACE, pointer_space_used_);
  deserializer->set_reservation(OLD_DATA_SPACE, data_space_used_);
  deserializer->set_reservation(CODE_SPACE, code_space_used_);
  deserializer->set_reservation(MAP_SPACE, map_space_used_);
  deserializer->set_reservation(CELL_SPACE, cell_space_used_);
}


bool Snapshot::Initialize(const char* snapshot_file) {
  if (snapshot_file) {
    int len;
    byte* str = ReadBytes(snapshot_file, &len);
    if (!str) return false;
    bool success;
    {
      SnapshotByteSource source(str, len);
      Deserializer deserializer(&source);
      ReserveSpaceForSnapshot(&deserializer, snapshot_file);
      success = V8::Initialize(&deserializer);
    }
    DeleteArray(str);
    return success;
  } else if (size_ > 0) {
    SnapshotByteSource source(raw_data_, raw_size_);
    Deserializer deserializer(&source);
    ReserveSpaceForLinkedInSnapshot(&deserializer);
    return V8::Initialize(&deserializer);
  }
  return false;
}


bool Snapshot::HaveASnapshotToStartFrom() {
  return size_ != 0;
}


Handle<Context> Snapshot::NewContextFromSnapshot() {
  if (context_size_ == 0) {
    return Handle<Context>();
  }
  SnapshotByteSource source(context_raw_data_,
                            context_raw_size_);
  Deserializer deserializer(&source);
  Object* root;
  deserializer.set_reservation(NEW_SPACE, context_new_space_used_);
  deserializer.set_reservation(OLD_POINTER_SPACE, context_pointer_space_used_);
  deserializer.set_reservation(OLD_DATA_SPACE, context_data_space_used_);
  deserializer.set_reservation(CODE_SPACE, context_code_space_used_);
  deserializer.set_reservation(MAP_SPACE, context_map_space_used_);
  deserializer.set_reservation(CELL_SPACE, context_cell_space_used_);
  deserializer.DeserializePartial(&root);
  CHECK(root->IsContext());
  return Handle<Context>(Context::cast(root));
}

} }  // namespace v8::internal
