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

#include <google/protobuf/implicit_weak_message.h>

#include <google/protobuf/parse_context.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <google/protobuf/stubs/once.h>
#include <google/protobuf/wire_format_lite.h>

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {
namespace internal {

#if GOOGLE_PROTOBUF_ENABLE_EXPERIMENTAL_PARSER
const char* ImplicitWeakMessage::_InternalParse(const char* ptr,
                                                ParseContext* ctx) {
  return ctx->AppendString(ptr, &data_);
}
#else
bool ImplicitWeakMessage::MergePartialFromCodedStream(
    io::CodedInputStream* input) {
  io::StringOutputStream string_stream(&data_);
  io::CodedOutputStream coded_stream(&string_stream, false);
  return WireFormatLite::SkipMessage(input, &coded_stream);
}
#endif

ExplicitlyConstructed<ImplicitWeakMessage>
    implicit_weak_message_default_instance;
internal::once_flag implicit_weak_message_once_init_;

void InitImplicitWeakMessageDefaultInstance() {
  implicit_weak_message_default_instance.DefaultConstruct();
}

const ImplicitWeakMessage* ImplicitWeakMessage::default_instance() {
  internal::call_once(implicit_weak_message_once_init_,
                      InitImplicitWeakMessageDefaultInstance);
  return &implicit_weak_message_default_instance.get();
}

}  // namespace internal
}  // namespace protobuf
}  // namespace google
