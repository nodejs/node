// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/d8/mock-dom/strings.h"

#include <algorithm>

#include "absl/container/flat_hash_map.h"
#include "include/v8-isolate.h"
#include "include/v8-persistent-handle.h"
#include "include/v8-value.h"
#include "src/base/logging.h"

namespace v8 {
namespace mock_dom {

namespace {

using AtomTable8 = absl::flat_hash_map<std::string_view, StringImpl*>;
using AtomTable16 = absl::flat_hash_map<std::u16string_view, StringImpl*>;

AtomTable8& Atoms8() {
  static AtomTable8* table = new AtomTable8();
  return *table;
}
AtomTable16& Atoms16() {
  static AtomTable16* table = new AtomTable16();
  return *table;
}

bool IsAllLatin1(std::u16string_view s) {
  for (char16_t c : s) {
    if (c > 0xFF) return false;
  }
  return true;
}

std::string Narrow(std::u16string_view s) {
  std::string out(s.size(), '\0');
  for (size_t i = 0; i < s.size(); ++i) out[i] = static_cast<char>(s[i]);
  return out;
}

// The external string resources hand the StringImpl's own buffer to V8.
class Resource8 final : public v8::String::ExternalOneByteStringResource {
 public:
  explicit Resource8(StringImpl* impl) : string_(impl) {}
  const char* data() const override { return string_.impl()->view8().data(); }
  size_t length() const override { return string_.length(); }
  void Dispose() override;
  StringImpl* impl() const { return string_.impl(); }

 private:
  String string_;
};

class Resource16 final : public v8::String::ExternalStringResource {
 public:
  explicit Resource16(StringImpl* impl) : string_(impl) {}
  const uint16_t* data() const override {
    return reinterpret_cast<const uint16_t*>(string_.impl()->view16().data());
  }
  size_t length() const override { return string_.length(); }
  void Dispose() override;
  StringImpl* impl() const { return string_.impl(); }

 private:
  String string_;
};

struct StringCache {
  Isolate* isolate = nullptr;
  // DOM string -> the V8 string that was created for it. Weak, so that V8
  // strings die when JS drops them.
  absl::flat_hash_map<StringImpl*, Global<v8::String>> v8_strings;
  // External resources created by us, so that a V8 string coming back from JS
  // can be mapped to its StringImpl without copying. Other embedder code (d8
  // itself) creates external strings too, so the resource type cannot be
  // assumed.
  absl::flat_hash_map<const void*, StringImpl*> resources;
};

StringCache& Cache() {
  static StringCache* cache = new StringCache();
  return *cache;
}

void Resource8::Dispose() {
  Cache().resources.erase(this);
  delete this;
}

void Resource16::Dispose() {
  Cache().resources.erase(this);
  delete this;
}

void StringDied(const WeakCallbackInfo<StringImpl>& info) {
  Cache().v8_strings.erase(info.GetParameter());
}

}  // namespace

// --- StringImpl --------------------------------------------------------------

StringImpl* StringImpl::Create8(std::string_view latin1) {
  return new StringImpl(latin1);
}

StringImpl* StringImpl::Create16(std::u16string_view utf16) {
  return new StringImpl(utf16);
}

StringImpl* StringImpl::CreateFromUtf8(std::string_view utf8) {
  bool ascii = true;
  for (char c : utf8) {
    if (static_cast<unsigned char>(c) >= 0x80) {
      ascii = false;
      break;
    }
  }
  if (ascii) return Create8(utf8);
  std::u16string out;
  out.reserve(utf8.size());
  bool latin1 = true;
  size_t i = 0;
  while (i < utf8.size()) {
    unsigned char c = static_cast<unsigned char>(utf8[i]);
    uint32_t cp;
    size_t extra;
    if (c < 0x80) {
      cp = c;
      extra = 0;
    } else if ((c & 0xE0) == 0xC0) {
      cp = c & 0x1F;
      extra = 1;
    } else if ((c & 0xF0) == 0xE0) {
      cp = c & 0x0F;
      extra = 2;
    } else if ((c & 0xF8) == 0xF0) {
      cp = c & 0x07;
      extra = 3;
    } else {
      cp = 0xFFFD;
      extra = 0;
    }
    ++i;
    for (size_t k = 0; k < extra; ++k) {
      if (i >= utf8.size() ||
          (static_cast<unsigned char>(utf8[i]) & 0xC0) != 0x80) {
        cp = 0xFFFD;
        break;
      }
      cp = (cp << 6) | (static_cast<unsigned char>(utf8[i]) & 0x3F);
      ++i;
    }
    if (cp > 0xFF) latin1 = false;
    if (cp >= 0x10000) {
      cp -= 0x10000;
      out.push_back(static_cast<char16_t>(0xD800 + (cp >> 10)));
      out.push_back(static_cast<char16_t>(0xDC00 + (cp & 0x3FF)));
    } else {
      out.push_back(static_cast<char16_t>(cp));
    }
  }
  if (latin1) return Create8(Narrow(out));
  return Create16(out);
}

void StringImpl::Destroy() {
  if (is_atomic_) AtomicString::Remove(this);
  delete this;
}

void AppendUtf8(std::string* out, uint32_t cp) {
  if (cp < 0x80) {
    out->push_back(static_cast<char>(cp));
  } else if (cp < 0x800) {
    out->push_back(static_cast<char>(0xC0 | (cp >> 6)));
    out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else if (cp < 0x10000) {
    out->push_back(static_cast<char>(0xE0 | (cp >> 12)));
    out->push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  } else {
    out->push_back(static_cast<char>(0xF0 | (cp >> 18)));
    out->push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
    out->push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
    out->push_back(static_cast<char>(0x80 | (cp & 0x3F)));
  }
}

std::string StringImpl::ToUtf8() const {
  std::string out;
  if (is_8bit_) {
    bool ascii = true;
    for (char c : chars8_) {
      if (static_cast<unsigned char>(c) >= 0x80) {
        ascii = false;
        break;
      }
    }
    if (ascii) return chars8_;
    out.reserve(chars8_.size() * 2);
    for (char c : chars8_) AppendUtf8(&out, static_cast<unsigned char>(c));
    return out;
  }
  out.reserve(chars16_.size() * 3);
  for (size_t i = 0; i < chars16_.size(); ++i) {
    uint32_t cp = chars16_[i];
    if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < chars16_.size() &&
        chars16_[i + 1] >= 0xDC00 && chars16_[i + 1] <= 0xDFFF) {
      cp = 0x10000 + ((cp - 0xD800) << 10) + (chars16_[i + 1] - 0xDC00);
      ++i;
    }
    AppendUtf8(&out, cp);
  }
  return out;
}

// --- String / AtomicString ---------------------------------------------------

const String& String::Empty() {
  static String* empty = new String(StringImpl::Create8(""));
  return *empty;
}

AtomicString::AtomicString(std::string_view latin1) {
  AtomTable8& table = Atoms8();
  auto it = table.find(latin1);
  if (it != table.end()) {
    impl_ = it->second;
  } else {
    impl_ = StringImpl::Create8(latin1);
    impl_->is_atomic_ = true;
    table.emplace(impl_->view8(), impl_);
  }
  impl_->AddRef();
}

AtomicString::AtomicString(std::u16string_view utf16) {
  if (IsAllLatin1(utf16)) {
    *this = AtomicString(std::string_view(Narrow(utf16)));
    return;
  }
  AtomTable16& table = Atoms16();
  auto it = table.find(utf16);
  if (it != table.end()) {
    impl_ = it->second;
  } else {
    impl_ = StringImpl::Create16(utf16);
    impl_->is_atomic_ = true;
    table.emplace(impl_->view16(), impl_);
  }
  impl_->AddRef();
}

AtomicString::AtomicString(const String& string) {
  StringImpl* impl = string.impl();
  if (!impl) return;
  if (impl->is_atomic()) {
    impl_ = impl;
    impl_->AddRef();
    return;
  }
  if (impl->is_8bit()) {
    *this = AtomicString(impl->view8());
  } else {
    *this = AtomicString(impl->view16());
  }
}

const AtomicString& AtomicString::EmptyAtom() {
  static AtomicString* empty = new AtomicString("");
  return *empty;
}

void AtomicString::Remove(StringImpl* impl) {
  if (impl->is_8bit()) {
    Atoms8().erase(impl->view8());
  } else {
    Atoms16().erase(impl->view16());
  }
}

std::string AsciiLower(std::string_view s) {
  std::string out(s);
  for (char& c : out) c = ToAsciiLower(c);
  return out;
}

std::string AsciiUpper(std::string_view s) {
  std::string out(s);
  for (char& c : out) c = ToAsciiUpper(c);
  return out;
}

bool EqualsIgnoringAsciiCase(std::string_view a, std::string_view b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (ToAsciiLower(a[i]) != ToAsciiLower(b[i])) return false;
  }
  return true;
}

std::string_view StripHtmlSpace(std::string_view s) {
  size_t begin = 0;
  size_t end = s.size();
  while (begin < end && IsHtmlSpace(s[begin])) ++begin;
  while (end > begin && IsHtmlSpace(s[end - 1])) --end;
  return s.substr(begin, end - begin);
}

// --- V8 conversion -----------------------------------------------------------

Local<v8::String> ToV8String(Isolate* isolate, StringImpl* impl) {
  if (!impl || impl->empty()) return v8::String::Empty(isolate);
  StringCache& cache = Cache();
  DCHECK(cache.isolate == nullptr || cache.isolate == isolate);
  cache.isolate = isolate;
  auto it = cache.v8_strings.find(impl);
  if (it != cache.v8_strings.end()) return it->second.Get(isolate);

  Local<v8::String> result;
  if (impl->is_8bit()) {
    Resource8* resource = new Resource8(impl);
    if (!v8::String::NewExternalOneByte(isolate, resource).ToLocal(&result)) {
      delete resource;
      std::string_view view = impl->view8();
      return v8::String::NewFromOneByte(
                 isolate, reinterpret_cast<const uint8_t*>(view.data()),
                 NewStringType::kNormal, static_cast<int>(view.size()))
          .ToLocalChecked();
    }
    cache.resources.emplace(resource, impl);
  } else {
    Resource16* resource = new Resource16(impl);
    if (!v8::String::NewExternalTwoByte(isolate, resource).ToLocal(&result)) {
      delete resource;
      std::u16string_view view = impl->view16();
      return v8::String::NewFromTwoByte(
                 isolate, reinterpret_cast<const uint16_t*>(view.data()),
                 NewStringType::kNormal, static_cast<int>(view.size()))
          .ToLocalChecked();
    }
    cache.resources.emplace(resource, impl);
  }
  Global<v8::String> weak(isolate, result);
  weak.SetWeak(impl, StringDied, WeakCallbackType::kParameter);
  cache.v8_strings.emplace(impl, std::move(weak));
  return result;
}

Local<v8::String> ToV8String(Isolate* isolate, const String& string) {
  return ToV8String(isolate, string.impl());
}

Local<v8::String> ToV8StringFromUtf8(Isolate* isolate, std::string_view utf8) {
  return v8::String::NewFromUtf8(isolate, utf8.data(), NewStringType::kNormal,
                                 static_cast<int>(utf8.size()))
      .ToLocalChecked();
}

Local<Value> ToV8StringOrNull(Isolate* isolate, const String& string) {
  if (string.is_null()) return Null(isolate);
  return ToV8String(isolate, string);
}

namespace {

StringImpl* ExternalImpl(Local<v8::String> string) {
  const void* resource = nullptr;
  if (string->IsExternalOneByte()) {
    resource = string->GetExternalOneByteStringResource();
  } else if (string->IsExternalTwoByte()) {
    resource = string->GetExternalStringResource();
  } else {
    return nullptr;
  }
  StringCache& cache = Cache();
  auto it = cache.resources.find(resource);
  return it == cache.resources.end() ? nullptr : it->second;
}

// Turns |string| into an external string backed by |impl|, so that the next
// conversion of the same V8 string finds the DOM string without copying.
// Blink does the same for strings passed into the DOM. V8 refuses for
// strings that are too short or live in read-only or shared space.
void Externalize(Isolate* isolate, Local<v8::String> string, StringImpl* impl) {
  StringCache& cache = Cache();
  if (impl->is_8bit()) {
    if (!string->IsOneByte() ||
        !string->CanMakeExternal(v8::String::ONE_BYTE_ENCODING)) {
      return;
    }
    Resource8* resource = new Resource8(impl);
    if (!string->MakeExternal(isolate, resource)) {
      delete resource;
      return;
    }
    cache.resources.emplace(resource, impl);
  } else {
    if (string->IsOneByte() ||
        !string->CanMakeExternal(v8::String::TWO_BYTE_ENCODING)) {
      return;
    }
    Resource16* resource = new Resource16(impl);
    if (!string->MakeExternal(isolate, resource)) {
      delete resource;
      return;
    }
    cache.resources.emplace(resource, impl);
  }
  cache.isolate = isolate;
  if (cache.v8_strings.find(impl) == cache.v8_strings.end()) {
    Global<v8::String> weak(isolate, string);
    weak.SetWeak(impl, StringDied, WeakCallbackType::kParameter);
    cache.v8_strings.emplace(impl, std::move(weak));
  }
}

}  // namespace

String ToDOMString(Isolate* isolate, Local<v8::String> string) {
  uint32_t length = static_cast<uint32_t>(string->Length());
  if (length == 0) return String::Empty();
  if (StringImpl* impl = ExternalImpl(string)) return String(impl);
  if (string->IsOneByte()) {
    std::string buffer(length, '\0');
    string->WriteOneByte(isolate, 0, length,
                         reinterpret_cast<uint8_t*>(buffer.data()));
    String result(StringImpl::Create8(buffer));
    Externalize(isolate, string, result.impl());
    return result;
  }
  std::u16string buffer(length, u'\0');
  string->Write(isolate, 0, length, reinterpret_cast<uint16_t*>(buffer.data()));
  if (IsAllLatin1(buffer)) return String(StringImpl::Create8(Narrow(buffer)));
  String result(StringImpl::Create16(buffer));
  Externalize(isolate, string, result.impl());
  return result;
}

AtomicString ToAtomicString(Isolate* isolate, Local<v8::String> string) {
  uint32_t length = static_cast<uint32_t>(string->Length());
  if (length == 0) return AtomicString::EmptyAtom();
  if (StringImpl* impl = ExternalImpl(string)) {
    return AtomicString(String(impl));
  }
  // Names and short values are looked up in the atom table straight from a
  // stack buffer, without allocating a StringImpl first.
  constexpr uint32_t kStackBufferSize = 128;
  if (length <= kStackBufferSize && string->IsOneByte()) {
    uint8_t buffer[kStackBufferSize];
    string->WriteOneByte(isolate, 0, length, buffer);
    AtomicString atom(
        std::string_view(reinterpret_cast<const char*>(buffer), length));
    Externalize(isolate, string, atom.impl());
    return atom;
  }
  return AtomicString(ToDOMString(isolate, string));
}

std::string ToUtf8(Isolate* isolate, Local<v8::String> string) {
  size_t length = string->Utf8Length(isolate);
  std::string out(length, '\0');
  string->WriteUtf8(isolate, out.data(), length);
  return out;
}

void DisposeStringCache(Isolate* isolate) {
  StringCache& cache = Cache();
  if (cache.isolate != isolate) return;
  cache.v8_strings.clear();
  cache.isolate = nullptr;
}

}  // namespace mock_dom
}  // namespace v8
