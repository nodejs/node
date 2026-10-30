// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_BASE_JSON_H_
#define V8_EXPERIMENTAL_REDLINE_BASE_JSON_H_

#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace redline {

// Escapes `text` as the contents of a JSON string literal (quotes included).
std::string JsonQuote(std::string_view text);

// Re-indents compact JSON for humans: one member per line, `indent` spaces
// per level; arrays stay on one line. The input must be valid JSON.
std::string JsonPretty(std::string_view json, int indent = 2);

// Builds a single flat JSON object. Values are emitted in insertion order.
// Example:
//   JsonObject o;
//   o.Add("file", name).Add("edges", 42);
//   std::string line = o.ToString();  // {"file":"a.js","edges":42}
class JsonObject {
 public:
  JsonObject& Add(std::string_view key, std::string_view value);
  JsonObject& Add(std::string_view key, const char* value);
  JsonObject& Add(std::string_view key, int64_t value);
  JsonObject& Add(std::string_view key, uint64_t value);
  JsonObject& Add(std::string_view key, int value);
  JsonObject& Add(std::string_view key, unsigned value);
  JsonObject& Add(std::string_view key, double value);
  JsonObject& Add(std::string_view key, bool value);
  // Adds an already serialized JSON value (array, object, ...).
  JsonObject& AddRaw(std::string_view key, std::string_view json);
  JsonObject& AddArray(std::string_view key, const std::vector<uint32_t>& v);
  JsonObject& AddArray(std::string_view key, const std::vector<std::string>& v);

  std::string ToString() const;

 private:
  void AppendKey(std::string_view key);
  std::string buffer_;
};

// Appends one JSON object per line to a file ("JSON Lines"). Thread-safe.
// Every redline command that produces data writes it through this class so
// that runs can be compared and replayed from their event logs.
class JsonlWriter {
 public:
  JsonlWriter() = default;
  ~JsonlWriter();
  JsonlWriter(const JsonlWriter&) = delete;
  JsonlWriter& operator=(const JsonlWriter&) = delete;

  // Opens `path` for appending (or truncating when `truncate` is set).
  // Returns false and leaves the writer disabled on failure.
  bool Open(const std::string& path, bool truncate);
  bool is_open() const { return file_ != nullptr; }

  void Write(const JsonObject& object);
  void WriteLine(std::string_view json_line);
  void Flush();

 private:
  std::mutex mutex_;
  FILE* file_ = nullptr;
};

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_BASE_JSON_H_
