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

#include <google/protobuf/util/internal/json_objectwriter.h>

#include <math.h>

#include <google/protobuf/stubs/casts.h>
#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/util/internal/utility.h>

#include <google/protobuf/util/internal/json_escaping.h>
#include <google/protobuf/stubs/strutil.h>
#include <google/protobuf/stubs/mathlimits.h>

namespace google {
namespace protobuf {
namespace util {
namespace converter {

using strings::ArrayByteSource;
;

JsonObjectWriter::~JsonObjectWriter() {
  if (element_ && !element_->is_root()) {
    GOOGLE_LOG(WARNING) << "JsonObjectWriter was not fully closed.";
  }
}

JsonObjectWriter* JsonObjectWriter::StartObject(StringPiece name) {
  WritePrefix(name);
  WriteChar('{');
  PushObject();
  return this;
}

JsonObjectWriter* JsonObjectWriter::EndObject() {
  Pop();
  WriteChar('}');
  if (element() && element()->is_root()) NewLine();
  return this;
}

JsonObjectWriter* JsonObjectWriter::StartList(StringPiece name) {
  WritePrefix(name);
  WriteChar('[');
  PushArray();
  return this;
}

JsonObjectWriter* JsonObjectWriter::EndList() {
  Pop();
  WriteChar(']');
  if (element()->is_root()) NewLine();
  return this;
}

JsonObjectWriter* JsonObjectWriter::RenderBool(StringPiece name,
                                               bool value) {
  return RenderSimple(name, value ? "true" : "false");
}

JsonObjectWriter* JsonObjectWriter::RenderInt32(StringPiece name,
                                                int32 value) {
  return RenderSimple(name, StrCat(value));
}

JsonObjectWriter* JsonObjectWriter::RenderUint32(StringPiece name,
                                                 uint32 value) {
  return RenderSimple(name, StrCat(value));
}

JsonObjectWriter* JsonObjectWriter::RenderInt64(StringPiece name,
                                                int64 value) {
  WritePrefix(name);
  WriteChar('"');
  stream_->WriteString(StrCat(value));
  WriteChar('"');
  return this;
}

JsonObjectWriter* JsonObjectWriter::RenderUint64(StringPiece name,
                                                 uint64 value) {
  WritePrefix(name);
  WriteChar('"');
  stream_->WriteString(StrCat(value));
  WriteChar('"');
  return this;
}

JsonObjectWriter* JsonObjectWriter::RenderDouble(StringPiece name,
                                                 double value) {
  if (MathLimits<double>::IsFinite(value)) {
    return RenderSimple(name, SimpleDtoa(value));
  }

  // Render quoted with NaN/Infinity-aware DoubleAsString.
  return RenderString(name, DoubleAsString(value));
}

JsonObjectWriter* JsonObjectWriter::RenderFloat(StringPiece name,
                                                float value) {
  if (MathLimits<float>::IsFinite(value)) {
    return RenderSimple(name, SimpleFtoa(value));
  }

  // Render quoted with NaN/Infinity-aware FloatAsString.
  return RenderString(name, FloatAsString(value));
}

JsonObjectWriter* JsonObjectWriter::RenderString(StringPiece name,
                                                 StringPiece value) {
  WritePrefix(name);
  WriteChar('"');
  ArrayByteSource source(value);
  JsonEscaping::Escape(&source, &sink_);
  WriteChar('"');
  return this;
}

JsonObjectWriter* JsonObjectWriter::RenderBytes(StringPiece name,
                                                StringPiece value) {
  WritePrefix(name);
  std::string base64;

  if (use_websafe_base64_for_bytes_)
    WebSafeBase64EscapeWithPadding(value.ToString(), &base64);
  else
    Base64Escape(value, &base64);

  WriteChar('"');
  // TODO(wpoon): Consider a ByteSink solution that writes the base64 bytes
  //              directly to the stream, rather than first putting them
  //              into a string and then writing them to the stream.
  stream_->WriteRaw(base64.data(), base64.size());
  WriteChar('"');
  return this;
}

JsonObjectWriter* JsonObjectWriter::RenderNull(StringPiece name) {
  return RenderSimple(name, "null");
}

JsonObjectWriter* JsonObjectWriter::RenderNullAsEmpty(StringPiece name) {
  return RenderSimple(name, "");
}

void JsonObjectWriter::WritePrefix(StringPiece name) {
  bool not_first = !element()->is_first();
  if (not_first) WriteChar(',');
  if (not_first || !element()->is_root()) NewLine();
  if (!name.empty() || element()->is_json_object()) {
    WriteChar('"');
    if (!name.empty()) {
      ArrayByteSource source(name);
      JsonEscaping::Escape(&source, &sink_);
    }
    stream_->WriteString("\":");
    if (!indent_string_.empty()) WriteChar(' ');
  }
}

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google
