#ifndef SRC_STRING_BYTES_H_
#define SRC_STRING_BYTES_H_

// Decodes a v8::Handle<v8::String> or Buffer to a raw char*

#include "v8.h"
#include "node.h"
#include "env.h"
#include "env-inl.h"

namespace node {

class StringBytes {
 public:
  class InlineDecoder {
   public:
    InlineDecoder() : out_(nullptr) {
    }

    ~InlineDecoder() {
      if (out_ != out_st_)
        delete[] out_;
      out_ = nullptr;
    }

    inline bool Decode(Environment* env,
                       v8::Handle<v8::String> string,
                       v8::Handle<v8::Value> encoding,
                       enum encoding _default) {
      enum encoding enc = ParseEncoding(env->isolate(), encoding, _default);
      if (!StringBytes::IsValidString(env->isolate(), string, enc)) {
        env->ThrowTypeError("Bad input string");
        return false;
      }

      size_t buflen = StringBytes::StorageSize(env->isolate(), string, enc);
      if (buflen > sizeof(out_st_))
        out_ = new char[buflen];
      else
        out_ = out_st_;
      size_ = StringBytes::Write(env->isolate(),
                                 out_,
                                 buflen,
                                 string,
                                 enc);
      return true;
    }

    inline const char* out() const { return out_; }
    inline size_t size() const { return size_; }

   private:
    static const int kStorageSize = 1024;

    char out_st_[kStorageSize];
    char* out_;
    size_t size_;
  };

  // Does the string match the encoding? Quick but non-exhaustive.
  // Example: a HEX string must have a length that's a multiple of two.
  // FIXME(bnoordhuis) IsMaybeValidString()? Naming things is hard...
  static bool IsValidString(v8::Isolate* isolate,
                            v8::Handle<v8::String> string,
                            enum encoding enc);

  // Fast, but can be 2 bytes oversized for Base64, and
  // as much as triple UTF-8 strings <= 65536 chars in length
  static size_t StorageSize(v8::Isolate* isolate,
                            v8::Handle<v8::Value> val,
                            enum encoding enc);

  // Precise byte count, but slightly slower for Base64 and
  // very much slower for UTF-8
  static size_t Size(v8::Isolate* isolate,
                     v8::Handle<v8::Value> val,
                     enum encoding enc);

  // If the string is external then assign external properties to data and len,
  // then return true. If not return false.
  static bool GetExternalParts(v8::Isolate* isolate,
                               v8::Handle<v8::Value> val,
                               const char** data,
                               size_t* len);

  // Write the bytes from the string or buffer into the char*
  // returns the number of bytes written, which will always be
  // <= buflen.  Use StorageSize/Size first to know how much
  // memory to allocate.
  static size_t Write(v8::Isolate* isolate,
                      char* buf,
                      size_t buflen,
                      v8::Handle<v8::Value> val,
                      enum encoding enc,
                      int* chars_written = nullptr);

  // Take the bytes in the src, and turn it into a Buffer or String.
  // Don't call with encoding=UCS2.
  static v8::Local<v8::Value> Encode(v8::Isolate* isolate,
                                     const char* buf,
                                     size_t buflen,
                                     enum encoding encoding);

  // The input buffer should be in host endianness.
  static v8::Local<v8::Value> Encode(v8::Isolate* isolate,
                                     const uint16_t* buf,
                                     size_t buflen);

  // Deprecated legacy interface

  NODE_DEPRECATED("Use IsValidString(isolate, ...)",
                  static inline bool IsValidString(
      v8::Handle<v8::String> string,
      enum encoding enc) {
    return IsValidString(v8::Isolate::GetCurrent(), string, enc);
  })

  NODE_DEPRECATED("Use StorageSize(isolate, ...)",
                  static inline size_t StorageSize(v8::Handle<v8::Value> val,
                                                  enum encoding enc) {
    return StorageSize(v8::Isolate::GetCurrent(), val, enc);
  })

  NODE_DEPRECATED("Use Size(isolate, ...)",
                  static inline size_t Size(v8::Handle<v8::Value> val,
                                            enum encoding enc) {
    return Size(v8::Isolate::GetCurrent(), val, enc);
  })

  NODE_DEPRECATED("Use GetExternalParts(isolate, ...)",
                  static inline bool GetExternalParts(v8::Handle<v8::Value> val,
                                                      const char** data,
                                                      size_t* len) {
    return GetExternalParts(v8::Isolate::GetCurrent(), val, data, len);
  })

  NODE_DEPRECATED("Use Write(isolate, ...)",
                  static inline size_t Write(char* buf,
                                             size_t buflen,
                                             v8::Handle<v8::Value> val,
                                             enum encoding enc,
                                             int* chars_written = nullptr) {
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    return Write(isolate, buf, buflen, val, enc, chars_written);
  })

  NODE_DEPRECATED("Use Encode(isolate, ...)",
                  static inline v8::Local<v8::Value> Encode(
      const char* buf,
      size_t buflen,
      enum encoding encoding) {
    return Encode(v8::Isolate::GetCurrent(), buf, buflen, encoding);
  })
};

}  // namespace node

#endif  // SRC_STRING_BYTES_H_
