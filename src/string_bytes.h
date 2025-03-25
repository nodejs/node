// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef SRC_STRING_BYTES_H_
#define SRC_STRING_BYTES_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "v8.h"
#include "env-inl.h"
#include <string>
#include <string_view>
#include <memory>
#include <optional>

namespace node {

// Forward declarations
enum class Encoding : uint8_t {
  ASCII,
  UTF8,
  BASE64,
  UCS2,
  BINARY,
  HEX,
  BUFFER,
  LATIN1 = BINARY
};

/**
 * @brief Handles conversion between V8 strings/buffers and raw bytes
 * 
 * This class provides utilities for encoding and decoding between V8's string
 * representations and various byte encodings. It handles proper memory management
 * and encoding validation.
 */
class StringBytes {
 public:
  /**
   * @brief Inline decoder for efficient string-to-bytes conversion
   * 
   * Uses stack buffer when possible, automatically falls back to heap for larger strings.
   * Provides RAII-compliant memory management.
   */
  class InlineDecoder : public MaybeStackBuffer<char> {
   public:
    /**
     * @brief Decodes a V8 string to bytes using specified encoding
     * 
     * @param env The V8 environment
     * @param string The source string to decode
     * @param enc The target encoding
     * @return v8::Maybe<void> Success/failure of the operation
     */
    inline v8::Maybe<void> Decode(Environment* env,
                                 v8::Local<v8::String> string,
                                 Encoding enc) {
      size_t storage;
      if (!StringBytes::StorageSize(env->isolate(), string, enc).To(&storage))
        return v8::Nothing<void>();
        
      AllocateSufficientStorage(storage);
      const size_t length =
          StringBytes::Write(env->isolate(), out(), storage, string, enc);

      SetLength(length);
      return v8::JustVoid();
    }

    [[nodiscard]] inline size_t size() const noexcept { return length(); }
  };

  /**
   * @brief Calculates required storage size for encoding conversion
   * 
   * Fast but may overestimate by up to:
   * - 2 bytes for Base64
   * - 3x for UTF-8 strings <= 65536 chars
   */
  [[nodiscard]] static v8::Maybe<size_t> StorageSize(
      v8::Isolate* isolate,
      v8::Local<v8::Value> val,
      Encoding enc) noexcept;

  /**
   * @brief Calculates exact byte count needed
   * 
   * More precise but slower than StorageSize, especially for:
   * - Base64 encoding
   * - UTF-8 encoding
   */
  [[nodiscard]] static v8::Maybe<size_t> Size(
      v8::Isolate* isolate,
      v8::Local<v8::Value> val,
      Encoding enc) noexcept;

  /**
   * @brief Writes bytes from string/buffer to char buffer
   * 
   * @return Actual number of bytes written (always <= buflen)
   */
  static size_t Write(v8::Isolate* isolate,
                     char* buf,
                     size_t buflen,
                     v8::Local<v8::Value> val,
                     Encoding enc);

  /**
   * @brief Encodes raw bytes into V8 Buffer or String
   */
  [[nodiscard]] static v8::MaybeLocal<v8::Value> Encode(
      v8::Isolate* isolate,
      std::string_view buf,
      Encoding encoding,
      v8::Local<v8::Value>* error);

  /**
   * @brief Encodes UTF-16 data into V8 value
   * @warning Reverses endianness on BE platforms
   */
  [[nodiscard]] static v8::MaybeLocal<v8::Value> Encode(
      v8::Isolate* isolate,
      const uint16_t* buf,
      size_t buflen,
      v8::Local<v8::Value>* error);

 private:
  static size_t WriteUCS2(v8::Isolate* isolate,
                         char* buf,
                         size_t buflen,
                         v8::Local<v8::String> str,
                         int flags);

  // Prevent instantiation - this is a utility class
  StringBytes() = delete;
  ~StringBytes() = delete;
  StringBytes(const StringBytes&) = delete;
  StringBytes& operator=(const StringBytes&) = delete;
  StringBytes(StringBytes&&) = delete;
  StringBytes& operator=(StringBytes&&) = delete;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_STRING_BYTES_H_
