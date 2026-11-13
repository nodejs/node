// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/base/json.h"

#include <cinttypes>
#include <cmath>

#include "experimental/redline/base/log.h"

namespace redline {

namespace {

// Length of the well-formed UTF-8 sequence of 2 to 4 bytes that starts at
// text[i], or 0 if there is none (RFC 3629: no overlong forms, no
// surrogates, nothing above U+10FFFF).
size_t Utf8SequenceLength(std::string_view text, size_t i) {
  const unsigned int lead = static_cast<unsigned char>(text[i]);
  size_t length = 0;
  unsigned int low = 0x80, high = 0xbf;  // range of the second byte
  if (lead >= 0xc2 && lead <= 0xdf) {
    length = 2;
  } else if (lead >= 0xe0 && lead <= 0xef) {
    length = 3;
    low = lead == 0xe0 ? 0xa0 : 0x80;
    high = lead == 0xed ? 0x9f : 0xbf;
  } else if (lead >= 0xf0 && lead <= 0xf4) {
    length = 4;
    low = lead == 0xf0 ? 0x90 : 0x80;
    high = lead == 0xf4 ? 0x8f : 0xbf;
  } else {
    return 0;
  }
  if (i + length > text.size()) {
    return 0;
  }
  for (size_t k = 1; k < length; k++) {
    const unsigned int c = static_cast<unsigned char>(text[i + k]);
    if (c < (k == 1 ? low : 0x80u) || c > (k == 1 ? high : 0xbfu)) {
      return 0;
    }
  }
  return length;
}

}  // namespace

std::string JsonQuote(std::string_view text) {
  std::string out;
  out.reserve(text.size() + 2);
  out.push_back('"');
  for (size_t i = 0; i < text.size(); i++) {
    const unsigned char c = static_cast<unsigned char>(text[i]);
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      default:
        if (c < 0x20 || c == 0x7f) {
          char buffer[8];
          snprintf(buffer, sizeof(buffer), "\\u%04x", c);
          out += buffer;
        } else if (c < 0x80) {
          out.push_back(static_cast<char>(c));
        } else if (size_t length = Utf8SequenceLength(text, i); length > 0) {
          out.append(text.substr(i, length));
          i += length - 1;
        } else {
          // Not UTF-8 (for example a lone surrogate from a string
          // literal): the output must stay valid JSON text.
          out += "\\ufffd";
        }
    }
  }
  out.push_back('"');
  return out;
}

std::string JsonPretty(std::string_view json, int indent) {
  std::string out;
  out.reserve(json.size() * 2);
  int depth = 0;
  int array_depth = 0;  // > 0 while inside an array: stay on one line
  bool in_string = false;
  auto newline = [&] {
    out.push_back('\n');
    out.append(static_cast<size_t>(depth) * static_cast<size_t>(indent), ' ');
  };
  for (size_t i = 0; i < json.size(); i++) {
    const char c = json[i];
    if (in_string) {
      out.push_back(c);
      if (c == '\\' && i + 1 < json.size()) {
        out.push_back(json[++i]);  // escaped character, including \"
      } else if (c == '"') {
        in_string = false;
      }
      continue;
    }
    switch (c) {
      case '"':
        in_string = true;
        out.push_back(c);
        break;
      case '{':
        out.push_back(c);
        if (i + 1 < json.size() && json[i + 1] == '}') {
          out.push_back(json[++i]);  // empty object stays "{}"
          break;
        }
        depth++;
        newline();
        break;
      case '}':
        depth--;
        newline();
        out.push_back(c);
        break;
      case '[':
        array_depth++;
        out.push_back(c);
        break;
      case ']':
        array_depth--;
        out.push_back(c);
        break;
      case ',':
        out.push_back(c);
        if (array_depth > 0) {
          out.push_back(' ');
        } else {
          newline();
        }
        break;
      case ':':
        out += ": ";
        break;
      case ' ':
      case '\n':
      case '\t':
      case '\r':
        break;  // the compact form has none; drop any anyway
      default:
        out.push_back(c);
    }
  }
  return out;
}

void JsonObject::AppendKey(std::string_view key) {
  buffer_ += buffer_.empty() ? "{" : ",";
  buffer_ += JsonQuote(key);
  buffer_.push_back(':');
}

JsonObject& JsonObject::Add(std::string_view key, std::string_view value) {
  AppendKey(key);
  buffer_ += JsonQuote(value);
  return *this;
}

JsonObject& JsonObject::Add(std::string_view key, const char* value) {
  return Add(key, std::string_view(value));
}

JsonObject& JsonObject::Add(std::string_view key, int64_t value) {
  AppendKey(key);
  buffer_ += StringPrintf("%" PRId64, value);
  return *this;
}

JsonObject& JsonObject::Add(std::string_view key, uint64_t value) {
  AppendKey(key);
  buffer_ += StringPrintf("%" PRIu64, value);
  return *this;
}

JsonObject& JsonObject::Add(std::string_view key, int value) {
  return Add(key, static_cast<int64_t>(value));
}

JsonObject& JsonObject::Add(std::string_view key, unsigned value) {
  return Add(key, static_cast<uint64_t>(value));
}

JsonObject& JsonObject::Add(std::string_view key, double value) {
  AppendKey(key);
  if (std::isfinite(value)) {
    buffer_ += StringPrintf("%.6g", value);
  } else {
    buffer_ += "null";
  }
  return *this;
}

JsonObject& JsonObject::Add(std::string_view key, bool value) {
  AppendKey(key);
  buffer_ += value ? "true" : "false";
  return *this;
}

JsonObject& JsonObject::AddRaw(std::string_view key, std::string_view json) {
  AppendKey(key);
  buffer_ += json;
  return *this;
}

JsonObject& JsonObject::AddArray(std::string_view key,
                                 const std::vector<uint32_t>& v) {
  AppendKey(key);
  buffer_.push_back('[');
  for (size_t i = 0; i < v.size(); i++) {
    if (i > 0) {
      buffer_.push_back(',');
    }
    buffer_ += StringPrintf("%u", v[i]);
  }
  buffer_.push_back(']');
  return *this;
}

JsonObject& JsonObject::AddArray(std::string_view key,
                                 const std::vector<std::string>& v) {
  AppendKey(key);
  buffer_.push_back('[');
  for (size_t i = 0; i < v.size(); i++) {
    if (i > 0) {
      buffer_.push_back(',');
    }
    buffer_ += JsonQuote(v[i]);
  }
  buffer_.push_back(']');
  return *this;
}

std::string JsonObject::ToString() const {
  if (buffer_.empty()) {
    return "{}";
  }
  return buffer_ + "}";
}

JsonlWriter::~JsonlWriter() {
  if (file_ != nullptr) {
    fclose(file_);
  }
}

bool JsonlWriter::Open(const std::string& path, bool truncate) {
  std::scoped_lock<std::mutex> guard(mutex_);
  if (file_ != nullptr) {
    fclose(file_);
  }
  file_ = fopen(path.c_str(), truncate ? "w" : "a");
  if (file_ == nullptr) {
    REDLINE_ERROR("jsonl", "Cannot open %s for writing", path.c_str());
    return false;
  }
  return true;
}

void JsonlWriter::Write(const JsonObject& object) {
  WriteLine(object.ToString());
}

void JsonlWriter::WriteLine(std::string_view json_line) {
  std::scoped_lock<std::mutex> guard(mutex_);
  if (file_ == nullptr) {
    return;
  }
  fwrite(json_line.data(), 1, json_line.size(), file_);
  fputc('\n', file_);
}

void JsonlWriter::Flush() {
  std::scoped_lock<std::mutex> guard(mutex_);
  if (file_ != nullptr) {
    fflush(file_);
  }
}

}  // namespace redline
