// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "json_std_string_writer.h"
#include <cassert>
#include <stack>

namespace inspector_protocol {
namespace {
// Prints |value| to |out| with 4 hex digits, most significant chunk first.
void PrintHex(uint16_t value, std::string* out) {
  for (int ii = 3; ii >= 0; --ii) {
    int four_bits = 0xf & (value >> (4 * ii));
    out->append(1, four_bits + ((four_bits <= 9) ? '0' : ('a' - 10)));
  }
}

// In the writer below, we maintain a stack of State instances.
// It is just enough to emit the appropriate delimiters and brackets
// in JSON.
enum class Container {
  // Used for the top-level, initial state.
  NONE,
  // Inside a JSON object.
  OBJECT,
  // Inside a JSON array.
  ARRAY
};
class State {
 public:
  explicit State(Container container) : container_(container) {}
  void StartElement(std::string* out) {
    assert(container_ != Container::NONE || size_ == 0);
    if (size_ != 0) {
      char delim = (!(size_ & 1) || container_ == Container::ARRAY) ? ',' : ':';
      out->append(1, delim);
    }
    ++size_;
  }
  Container container() const { return container_; }

 private:
  Container container_ = Container::NONE;
  int size_ = 0;
};

constexpr char kBase64Table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz0123456789+/";

void Base64Encode(const std::vector<uint8_t>& in, std::string* out) {
  // The following three cases are based on the tables in the example
  // section in https://en.wikipedia.org/wiki/Base64. We process three
  // input bytes at a time, emitting 4 output bytes at a time.
  std::size_t ii = 0;

  // While possible, process three input bytes.
  for (; ii + 3 <= in.size(); ii += 3) {
    uint32_t twentyfour_bits = (in[ii] << 16) | (in[ii + 1] << 8) | in[ii + 2];
    out->push_back(kBase64Table[(twentyfour_bits >> 18)]);
    out->push_back(kBase64Table[(twentyfour_bits >> 12) & 0x3f]);
    out->push_back(kBase64Table[(twentyfour_bits >> 6) & 0x3f]);
    out->push_back(kBase64Table[twentyfour_bits & 0x3f]);
  }
  if (ii + 2 <= in.size()) {  // Process two input bytes.
    uint32_t twentyfour_bits = (in[ii] << 16) | (in[ii + 1] << 8);
    out->push_back(kBase64Table[(twentyfour_bits >> 18)]);
    out->push_back(kBase64Table[(twentyfour_bits >> 12) & 0x3f]);
    out->push_back(kBase64Table[(twentyfour_bits >> 6) & 0x3f]);
    out->push_back('=');  // Emit padding.
    return;
  }
  if (ii + 1 <= in.size()) {  // Process a single input byte.
    uint32_t twentyfour_bits = (in[ii] << 16);
    out->push_back(kBase64Table[(twentyfour_bits >> 18)]);
    out->push_back(kBase64Table[(twentyfour_bits >> 12) & 0x3f]);
    out->push_back('=');  // Emit padding.
    out->push_back('=');  // Emit padding.
  }
}

// Implements a handler for JSON parser events to emit a JSON string.
class Writer : public JSONParserHandler {
 public:
  Writer(Platform* platform, std::string* out, Status* status)
      : platform_(platform), out_(out), status_(status) {
    *status_ = Status();
    state_.emplace(Container::NONE);
  }

  void HandleObjectBegin() override {
    if (!status_->ok()) return;
    assert(!state_.empty());
    state_.top().StartElement(out_);
    state_.emplace(Container::OBJECT);
    out_->append("{");
  }

  void HandleObjectEnd() override {
    if (!status_->ok()) return;
    assert(state_.size() >= 2 && state_.top().container() == Container::OBJECT);
    state_.pop();
    out_->append("}");
  }

  void HandleArrayBegin() override {
    if (!status_->ok()) return;
    state_.top().StartElement(out_);
    state_.emplace(Container::ARRAY);
    out_->append("[");
  }

  void HandleArrayEnd() override {
    if (!status_->ok()) return;
    assert(state_.size() >= 2 && state_.top().container() == Container::ARRAY);
    state_.pop();
    out_->append("]");
  }

  void HandleString16(std::vector<uint16_t> chars) override {
    if (!status_->ok()) return;
    state_.top().StartElement(out_);
    out_->append("\"");
    for (const uint16_t ch : chars) {
      if (ch == '"') {
        out_->append("\\\"");
      } else if (ch == '\\') {
        out_->append("\\\\");
      } else if (ch == '\b') {
        out_->append("\\b");
      } else if (ch == '\f') {
        out_->append("\\f");
      } else if (ch == '\n') {
        out_->append("\\n");
      } else if (ch == '\r') {
        out_->append("\\r");
      } else if (ch == '\t') {
        out_->append("\\t");
      } else if (ch >= 32 && ch <= 126) {
        out_->append(1, ch);
      } else {
        out_->append("\\u");
        PrintHex(ch, out_);
      }
    }
    out_->append("\"");
  }

  void HandleBinary(std::vector<uint8_t> bytes) override {
    if (!status_->ok()) return;
    state_.top().StartElement(out_);
    out_->append("\"");
    Base64Encode(bytes, out_);
    out_->append("\"");
  }

  void HandleDouble(double value) override {
    if (!status_->ok()) return;
    state_.top().StartElement(out_);
    std::unique_ptr<char[]> str_value = platform_->DToStr(value);

    // DToStr may fail to emit a 0 before the decimal dot. E.g. this is
    // the case in base::NumberToString in Chromium (which is based on
    // dmg_fp). So, much like
    // https://cs.chromium.org/chromium/src/base/json/json_writer.cc
    // we probe for this and emit the leading 0 anyway if necessary.
    const char* chars = str_value.get();
    if (chars[0] == '.') {
      out_->append("0");
    } else if (chars[0] == '-' && chars[1] == '.') {
      out_->append("-0");
      ++chars;
    }
    out_->append(chars);
  }

  void HandleInt32(int32_t value) override {
    if (!status_->ok()) return;
    state_.top().StartElement(out_);
    out_->append(std::to_string(value));
  }

  void HandleBool(bool value) override {
    if (!status_->ok()) return;
    state_.top().StartElement(out_);
    out_->append(value ? "true" : "false");
  }

  void HandleNull() override {
    if (!status_->ok()) return;
    state_.top().StartElement(out_);
    out_->append("null");
  }

  void HandleError(Status error) override {
    assert(!error.ok());
    *status_ = error;
    out_->clear();
  }

 private:
  Platform* platform_;
  std::string* out_;
  Status* status_;
  std::stack<State> state_;
};
}  // namespace

std::unique_ptr<JSONParserHandler> NewJSONWriter(Platform* platform,
                                                 std::string* out,
                                                 Status* status) {
  return std::unique_ptr<Writer>(new Writer(platform, out, status));
}
}  // namespace inspector_protocol
