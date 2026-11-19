#ifndef SRC_NODE_JSON_H_
#define SRC_NODE_JSON_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace node {
namespace json {

enum class ValueType : uint8_t {
  kNull,
  kBool,
  kNumber,
  kString,
  kArray,
  kObject,
};

class Document;

// A read-only handle to one value of a parsed Document. A default-constructed
// Value is empty: it is what lookups return for a missing key or index, and
// every typed getter on it fails.
class Value {
 public:
  Value() = default;

  explicit operator bool() const { return document_ != nullptr; }

  ValueType type() const;
  bool IsObject() const { return *this && type() == ValueType::kObject; }
  bool IsArray() const { return *this && type() == ValueType::kArray; }

  bool GetString(std::string_view* out) const;
  // Objects and arrays: the exact source text of the value. It points into the
  // buffer passed to Document::Parse and is only valid while that buffer is.
  bool GetRawJson(std::string_view* out) const;
  bool GetBool(bool* out) const;
  bool GetDouble(double* out) const;
  bool GetInt64(int64_t* out) const;
  bool GetUint64(uint64_t* out) const;

  // Objects: the first field named `key`, or an empty Value.
  Value Find(std::string_view key) const;

  // Objects and arrays: the number of fields or elements.
  size_t size() const;
  // Objects and arrays: the field or element at `index`, in source order.
  Value At(size_t index) const;
  // Objects: the unescaped key of the field at `index`.
  std::string_view KeyAt(size_t index) const;

 private:
  friend class Document;
  Value(const Document* document, uint32_t index)
      : document_(document), index_(index) {}

  const Document* document_ = nullptr;
  uint32_t index_ = 0;
};

// A small DOM for node's internal JSON consumers. Parsing is done by the
// Jsonifier tier selected for this CPU, which validates the whole document.
class Document {
 public:
  // Replaces any previous contents. Returns false if `json` is not valid JSON,
  // in which case error() names the reason.
  bool Parse(std::string_view json);
  Value root() const;
  std::string_view error() const { return error_; }

 private:
  friend class Value;
  friend class DocumentBuilder;

  struct Node {
    ValueType type = ValueType::kNull;
    bool boolean = false;
    double number = 0;
    std::string text;
    std::string_view raw;
    std::vector<uint32_t> children;
    std::vector<std::string> keys;
  };

  std::vector<Node> nodes_;
  std::string_view error_;
};

}  // namespace json
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_NODE_JSON_H_
