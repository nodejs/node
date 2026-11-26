// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_D8_MOCK_DOM_STRINGS_H_
#define V8_EXPERIMENTAL_D8_MOCK_DOM_STRINGS_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "include/v8-local-handle.h"
#include "include/v8-primitive.h"

namespace v8 {
class Isolate;
class Value;

namespace mock_dom {

// An immutable, ref-counted string that is either Latin-1 or UTF-16, modelled
// on WTF::StringImpl. Strings are only ever touched on the isolate's thread,
// so the ref count is not atomic.
class StringImpl {
 public:
  static StringImpl* Create8(std::string_view latin1);
  static StringImpl* Create16(std::u16string_view utf16);
  // Picks the narrowest representation that can hold the decoded text.
  static StringImpl* CreateFromUtf8(std::string_view utf8);

  void AddRef() { ++ref_count_; }
  void Release() {
    if (--ref_count_ == 0) Destroy();
  }

  bool is_8bit() const { return is_8bit_; }
  bool is_atomic() const { return is_atomic_; }
  uint32_t length() const {
    return static_cast<uint32_t>(is_8bit_ ? chars8_.size() : chars16_.size());
  }
  bool empty() const { return length() == 0; }
  // Only valid for 8-bit strings.
  std::string_view view8() const { return chars8_; }
  // Only valid for 16-bit strings.
  std::u16string_view view16() const { return chars16_; }
  uint16_t at(uint32_t i) const {
    return is_8bit_ ? static_cast<uint8_t>(chars8_[i]) : chars16_[i];
  }

  std::string ToUtf8() const;
  bool EqualsAscii(std::string_view ascii) const {
    return is_8bit_ && chars8_ == ascii;
  }
  bool Equals(const StringImpl* other) const {
    if (this == other) return true;
    if (is_8bit_ != other->is_8bit_) return false;
    return is_8bit_ ? chars8_ == other->chars8_ : chars16_ == other->chars16_;
  }

 private:
  friend class AtomicString;
  explicit StringImpl(std::string_view s) : is_8bit_(true), chars8_(s) {}
  explicit StringImpl(std::u16string_view s) : is_8bit_(false), chars16_(s) {}
  void Destroy();

  uint32_t ref_count_ = 0;
  bool is_8bit_;
  bool is_atomic_ = false;
  std::string chars8_;
  std::u16string chars16_;
};

// A nullable reference to a StringImpl.
class String {
 public:
  String() = default;
  explicit String(StringImpl* impl) : impl_(impl) {
    if (impl_) impl_->AddRef();
  }
  String(const String& other) : String(other.impl_) {}
  String(String&& other) noexcept : impl_(other.impl_) {
    other.impl_ = nullptr;
  }
  ~String() {
    if (impl_) impl_->Release();
  }
  String& operator=(const String& other) {
    if (other.impl_) other.impl_->AddRef();
    if (impl_) impl_->Release();
    impl_ = other.impl_;
    return *this;
  }
  String& operator=(String&& other) noexcept {
    std::swap(impl_, other.impl_);
    return *this;
  }

  static String FromUtf8(std::string_view utf8) {
    return String(StringImpl::CreateFromUtf8(utf8));
  }
  static String FromLatin1(std::string_view latin1) {
    return String(StringImpl::Create8(latin1));
  }
  static const String& Empty();

  StringImpl* impl() const { return impl_; }
  bool is_null() const { return impl_ == nullptr; }
  bool empty() const { return !impl_ || impl_->empty(); }
  uint32_t length() const { return impl_ ? impl_->length() : 0; }
  bool is_8bit() const { return !impl_ || impl_->is_8bit(); }
  std::string ToUtf8() const { return impl_ ? impl_->ToUtf8() : std::string(); }
  // Fast path for the common case of ASCII-only content: returns the
  // characters without transcoding. Callers must check is_8bit() first.
  std::string_view view8() const {
    return impl_ ? impl_->view8() : std::string_view();
  }
  bool EqualsAscii(std::string_view ascii) const {
    return impl_ ? impl_->EqualsAscii(ascii) : ascii.empty();
  }
  bool operator==(const String& other) const {
    if (impl_ == other.impl_) return true;
    if (!impl_ || !other.impl_) return empty() && other.empty();
    return impl_->Equals(other.impl_);
  }
  bool operator!=(const String& other) const { return !(*this == other); }

 protected:
  StringImpl* impl_ = nullptr;
};

// An interned string. Two AtomicStrings are equal iff their impls are the same
// pointer, which is what makes tag, attribute-name and class comparisons
// cheap. The null atom and the empty atom are distinct, as in Blink.
class AtomicString : public String {
 public:
  AtomicString() = default;
  // NOLINTNEXTLINE(runtime/explicit)
  AtomicString(const char* ascii)
      : AtomicString(std::string_view(ascii ? ascii : "")) {}
  // NOLINTNEXTLINE(runtime/explicit)
  AtomicString(std::string_view latin1);
  explicit AtomicString(std::u16string_view utf16);
  explicit AtomicString(const String& string);

  static AtomicString FromUtf8(std::string_view utf8) {
    return AtomicString(String::FromUtf8(utf8));
  }
  static const AtomicString& EmptyAtom();

  bool operator==(const AtomicString& other) const {
    return impl_ == other.impl_;
  }
  bool operator!=(const AtomicString& other) const {
    return impl_ != other.impl_;
  }
  bool operator==(const char* ascii) const { return EqualsAscii(ascii); }

  struct Hash {
    size_t operator()(const AtomicString& s) const {
      return std::hash<const void*>()(s.impl());
    }
  };

 private:
  friend class StringImpl;
  static void Remove(StringImpl* impl);
};

// ASCII helpers used throughout the DOM implementation.
inline char ToAsciiLower(char c) {
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
}
inline char ToAsciiUpper(char c) {
  return (c >= 'a' && c <= 'z') ? static_cast<char>(c - ('a' - 'A')) : c;
}
inline bool IsHtmlSpace(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}
std::string AsciiLower(std::string_view s);
std::string AsciiUpper(std::string_view s);
bool EqualsIgnoringAsciiCase(std::string_view a, std::string_view b);
std::string_view StripHtmlSpace(std::string_view s);
void AppendUtf8(std::string* out, uint32_t code_point);

// --- Conversion between V8 strings and DOM strings ---------------------------

// Returns a V8 string for |string|. Non-empty strings are handed to V8 as
// external strings that share the StringImpl's buffer, and the resulting V8
// string is cached per isolate so that repeated reads of the same DOM string
// (attribute values, tag names, text) return the same V8 string, as Blink's
// string cache does.
Local<v8::String> ToV8String(Isolate* isolate, const String& string);
Local<v8::String> ToV8String(Isolate* isolate, StringImpl* impl);
Local<v8::String> ToV8StringFromUtf8(Isolate* isolate, std::string_view utf8);
// Returns null for a null string, like a nullable DOMString attribute.
Local<Value> ToV8StringOrNull(Isolate* isolate, const String& string);

// Converts a V8 string into a DOM string. If the V8 string is one of our own
// external strings, the underlying StringImpl is reused without copying.
String ToDOMString(Isolate* isolate, Local<v8::String> string);
AtomicString ToAtomicString(Isolate* isolate, Local<v8::String> string);
std::string ToUtf8(Isolate* isolate, Local<v8::String> string);

// Drops the per-isolate string cache. Must be called before the isolate is
// disposed.
void DisposeStringCache(Isolate* isolate);

}  // namespace mock_dom
}  // namespace v8

#endif  // V8_EXPERIMENTAL_D8_MOCK_DOM_STRINGS_H_
