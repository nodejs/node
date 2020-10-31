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

#ifndef GOOGLE_PROTOBUF_UTIL_CONVERTER_JSON_OBJECTWRITER_H__
#define GOOGLE_PROTOBUF_UTIL_CONVERTER_JSON_OBJECTWRITER_H__

#include <memory>
#include <string>

#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/util/internal/structured_objectwriter.h>
#include <google/protobuf/stubs/bytestream.h>

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {
namespace util {
namespace converter {


// An ObjectWriter implementation that outputs JSON. This ObjectWriter
// supports writing a compact form or a pretty printed form.
//
// Sample usage:
//   string output;
//   StringOutputStream* str_stream = new StringOutputStream(&output);
//   CodedOutputStream* out_stream = new CodedOutputStream(str_stream);
//   JsonObjectWriter* ow = new JsonObjectWriter("  ", out_stream);
//   ow->StartObject("")
//       ->RenderString("name", "value")
//       ->RenderString("emptystring", string())
//       ->StartObject("nested")
//         ->RenderInt64("light", 299792458);
//         ->RenderDouble("pi", 3.141592653589793);
//       ->EndObject()
//       ->StartList("empty")
//       ->EndList()
//     ->EndObject();
//
// And then the output string would become:
// {
//   "name": "value",
//   "emptystring": "",
//   "nested": {
//     "light": "299792458",
//     "pi": 3.141592653589793
//   },
//   "empty": []
// }
//
// JsonObjectWriter does not validate if calls actually result in valid JSON.
// For example, passing an empty name when one would be required won't result
// in an error, just an invalid output.
//
// Note that all int64 and uint64 are rendered as strings instead of numbers.
// This is because JavaScript parses numbers as 64-bit float thus int64 and
// uint64 would lose precision if rendered as numbers.
//
// JsonObjectWriter is thread-unsafe.
class PROTOBUF_EXPORT JsonObjectWriter : public StructuredObjectWriter {
 public:
  JsonObjectWriter(StringPiece indent_string, io::CodedOutputStream* out)
      : element_(new Element(/*parent=*/nullptr, /*is_json_object=*/false)),
        stream_(out),
        sink_(out),
        indent_string_(indent_string),
        use_websafe_base64_for_bytes_(false) {}
  virtual ~JsonObjectWriter();

  // ObjectWriter methods.
  virtual JsonObjectWriter* StartObject(StringPiece name);
  virtual JsonObjectWriter* EndObject();
  virtual JsonObjectWriter* StartList(StringPiece name);
  virtual JsonObjectWriter* EndList();
  virtual JsonObjectWriter* RenderBool(StringPiece name, bool value);
  virtual JsonObjectWriter* RenderInt32(StringPiece name, int32 value);
  virtual JsonObjectWriter* RenderUint32(StringPiece name, uint32 value);
  virtual JsonObjectWriter* RenderInt64(StringPiece name, int64 value);
  virtual JsonObjectWriter* RenderUint64(StringPiece name, uint64 value);
  virtual JsonObjectWriter* RenderDouble(StringPiece name, double value);
  virtual JsonObjectWriter* RenderFloat(StringPiece name, float value);
  virtual JsonObjectWriter* RenderString(StringPiece name,
                                         StringPiece value);
  virtual JsonObjectWriter* RenderBytes(StringPiece name, StringPiece value);
  virtual JsonObjectWriter* RenderNull(StringPiece name);
  virtual JsonObjectWriter* RenderNullAsEmpty(StringPiece name);

  void set_use_websafe_base64_for_bytes(bool value) {
    use_websafe_base64_for_bytes_ = value;
  }

 protected:
  class PROTOBUF_EXPORT Element : public BaseElement {
   public:
    Element(Element* parent, bool is_json_object)
        : BaseElement(parent),
          is_first_(true),
          is_json_object_(is_json_object) {}

    // Called before each field of the Element is to be processed.
    // Returns true if this is the first call (processing the first field).
    bool is_first() {
      if (is_first_) {
        is_first_ = false;
        return true;
      }
      return false;
    }

    // Whether we are currently renderring inside a JSON object (i.e., between
    // StartObject() and EndObject()).
    bool is_json_object() const { return is_json_object_; }

   private:
    bool is_first_;
    bool is_json_object_;

    GOOGLE_DISALLOW_IMPLICIT_CONSTRUCTORS(Element);
  };

  Element* element() override { return element_.get(); }

 private:
  class PROTOBUF_EXPORT ByteSinkWrapper : public strings::ByteSink {
   public:
    explicit ByteSinkWrapper(io::CodedOutputStream* stream) : stream_(stream) {}
    ~ByteSinkWrapper() override {}

    // ByteSink methods.
    void Append(const char* bytes, size_t n) override {
      stream_->WriteRaw(bytes, n);
    }

   private:
    io::CodedOutputStream* stream_;

    GOOGLE_DISALLOW_EVIL_CONSTRUCTORS(ByteSinkWrapper);
  };

  // Renders a simple value as a string. By default all non-string Render
  // methods convert their argument to a string and call this method. This
  // method can then be used to render the simple value without escaping it.
  JsonObjectWriter* RenderSimple(StringPiece name,
                                 const std::string& value) {
    WritePrefix(name);
    stream_->WriteString(value);
    return this;
  }

  // Pushes a new JSON array element to the stack.
  void PushArray() {
    element_.reset(new Element(element_.release(), /*is_json_object=*/false));
  }

  // Pushes a new JSON object element to the stack.
  void PushObject() {
    element_.reset(new Element(element_.release(), /*is_json_object=*/true));
  }

  // Pops an element off of the stack and deletes the popped element.
  void Pop() {
    bool needs_newline = !element_->is_first();
    element_.reset(element_->pop<Element>());
    if (needs_newline) NewLine();
  }

  // If pretty printing is enabled, this will write a newline to the output,
  // followed by optional indentation. Otherwise this method is a noop.
  void NewLine() {
    if (!indent_string_.empty()) {
      WriteChar('\n');
      for (int i = 0; i < element()->level(); i++) {
        stream_->WriteString(indent_string_);
      }
    }
  }

  // Writes a prefix. This will write out any pretty printing and
  // commas that are required, followed by the name and a ':' if
  // the name is not null.
  void WritePrefix(StringPiece name);

  // Writes an individual character to the output.
  void WriteChar(const char c) { stream_->WriteRaw(&c, sizeof(c)); }

  std::unique_ptr<Element> element_;
  io::CodedOutputStream* stream_;
  ByteSinkWrapper sink_;
  const std::string indent_string_;

  // Whether to use regular or websafe base64 encoding for byte fields. Defaults
  // to regular base64 encoding.
  bool use_websafe_base64_for_bytes_;

  GOOGLE_DISALLOW_IMPLICIT_CONSTRUCTORS(JsonObjectWriter);
};

}  // namespace converter
}  // namespace util
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_UTIL_CONVERTER_JSON_OBJECTWRITER_H__
