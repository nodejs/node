#include "node_json.h"
#include "node_json_events.h"

#include <charconv>
#include <utility>

namespace node {
namespace json {

class DocumentBuilder final : public JsonEventSink {
 public:
  explicit DocumentBuilder(Document* document) : document_(document) {}

  bool complete() const { return stack_.empty() && !document_->nodes_.empty(); }

  void StartObject() override { stack_.push_back(Add(ValueType::kObject)); }
  void EndObject(std::string_view raw) override { End(raw); }
  void StartArray() override { stack_.push_back(Add(ValueType::kArray)); }
  void EndArray(std::string_view raw) override { End(raw); }
  void Key(std::string_view key) override { pending_key_.assign(key); }

  void String(std::string_view value) override {
    document_->nodes_[Add(ValueType::kString)].text.assign(value);
  }

  void Number(std::string_view text, double value) override {
    Document::Node& node = document_->nodes_[Add(ValueType::kNumber)];
    node.text.assign(text);
    node.number = value;
  }

  void Bool(bool value) override {
    document_->nodes_[Add(ValueType::kBool)].boolean = value;
  }

  void Null() override { Add(ValueType::kNull); }

 private:
  void End(std::string_view raw) {
    document_->nodes_[stack_.back()].raw = raw;
    stack_.pop_back();
  }

  uint32_t Add(ValueType type) {
    const uint32_t index = static_cast<uint32_t>(document_->nodes_.size());
    document_->nodes_.emplace_back();
    document_->nodes_[index].type = type;
    if (!stack_.empty()) {
      Document::Node& parent = document_->nodes_[stack_.back()];
      parent.children.push_back(index);
      if (parent.type == ValueType::kObject) {
        parent.keys.push_back(std::move(pending_key_));
        pending_key_.clear();
      }
    }
    return index;
  }

  Document* document_;
  std::vector<uint32_t> stack_;
  std::string pending_key_;
};

static bool OnlyWhitespaceAfter(std::string_view json, std::string_view root) {
  const char* position = root.data() + root.size();
  const char* end = json.data() + json.size();
  for (; position < end; ++position) {
    const char c = *position;
    if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
      return false;
    }
  }
  return true;
}

bool Document::Parse(std::string_view json) {
  nodes_.clear();
  error_ = {};
  DocumentBuilder builder(this);
  error_ = WalkJson(json, &builder);
  if (error_.empty() && !builder.complete()) {
    error_ = "incomplete_document";
  }
  std::string_view root_raw;
  if (error_.empty() && root().GetRawJson(&root_raw) &&
      !OnlyWhitespaceAfter(json, root_raw)) {
    error_ = "trailing_content";
  }
  if (!error_.empty()) {
    nodes_.clear();
    return false;
  }
  return true;
}

Value Document::root() const {
  return nodes_.empty() ? Value() : Value(this, 0);
}

ValueType Value::type() const {
  return document_->nodes_[index_].type;
}

bool Value::GetString(std::string_view* out) const {
  if (!*this || type() != ValueType::kString) {
    return false;
  }
  *out = document_->nodes_[index_].text;
  return true;
}

bool Value::GetRawJson(std::string_view* out) const {
  if (!IsObject() && !IsArray()) {
    return false;
  }
  *out = document_->nodes_[index_].raw;
  return true;
}

bool Value::GetBool(bool* out) const {
  if (!*this || type() != ValueType::kBool) {
    return false;
  }
  *out = document_->nodes_[index_].boolean;
  return true;
}

bool Value::GetDouble(double* out) const {
  if (!*this || type() != ValueType::kNumber) {
    return false;
  }
  *out = document_->nodes_[index_].number;
  return true;
}

template <typename T>
static bool ParseInteger(const std::string& text, T* out) {
  T value{};
  const char* end = text.data() + text.size();
  const auto [ptr, error] = std::from_chars(text.data(), end, value);
  if (error != std::errc() || ptr != end) {
    return false;
  }
  *out = value;
  return true;
}

bool Value::GetInt64(int64_t* out) const {
  if (!*this || type() != ValueType::kNumber) {
    return false;
  }
  return ParseInteger(document_->nodes_[index_].text, out);
}

bool Value::GetUint64(uint64_t* out) const {
  if (!*this || type() != ValueType::kNumber) {
    return false;
  }
  return ParseInteger(document_->nodes_[index_].text, out);
}

Value Value::Find(std::string_view key) const {
  if (!IsObject()) {
    return Value();
  }
  const Document::Node& node = document_->nodes_[index_];
  for (size_t i = 0; i < node.keys.size(); ++i) {
    if (node.keys[i] == key) {
      return Value(document_, node.children[i]);
    }
  }
  return Value();
}

size_t Value::size() const {
  if (!IsObject() && !IsArray()) {
    return 0;
  }
  return document_->nodes_[index_].children.size();
}

Value Value::At(size_t index) const {
  if (index >= size()) {
    return Value();
  }
  return Value(document_, document_->nodes_[index_].children[index]);
}

std::string_view Value::KeyAt(size_t index) const {
  if (!IsObject() || index >= size()) {
    return {};
  }
  return document_->nodes_[index_].keys[index];
}

}  // namespace json
}  // namespace node
