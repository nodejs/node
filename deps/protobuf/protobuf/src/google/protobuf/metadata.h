// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
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

// This header file defines an internal class that encapsulates internal message
// metadata (Unknown-field set, Arena pointer, ...) and allows its
// representation to be made more space-efficient via various optimizations.
//
// Note that this is distinct from google::protobuf::Metadata, which encapsulates
// Descriptor and Reflection pointers.

#ifndef GOOGLE_PROTOBUF_METADATA_H__
#define GOOGLE_PROTOBUF_METADATA_H__

#include <google/protobuf/metadata_lite.h>
#include <google/protobuf/unknown_field_set.h>

#ifdef SWIG
#error "You cannot SWIG proto headers"
#endif

namespace google {
namespace protobuf {
namespace internal {

class InternalMetadataWithArena
    : public InternalMetadataWithArenaBase<UnknownFieldSet,
                                           InternalMetadataWithArena> {
 public:
  InternalMetadataWithArena() {}
  explicit InternalMetadataWithArena(Arena* arena)
      : InternalMetadataWithArenaBase<UnknownFieldSet,
                                      InternalMetadataWithArena>(arena) {}

  void DoSwap(UnknownFieldSet* other) { mutable_unknown_fields()->Swap(other); }

  void DoMergeFrom(const UnknownFieldSet& other) {
    mutable_unknown_fields()->MergeFrom(other);
  }

  void DoClear() { mutable_unknown_fields()->Clear(); }

  static const UnknownFieldSet& default_instance() {
    return *UnknownFieldSet::default_instance();
  }
};

}  // namespace internal
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_METADATA_H__
