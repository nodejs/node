// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_STRING_STREAM_H_
#define V8_STRING_STREAM_H_

#include "handles.h"

namespace v8 {
namespace internal {

class StringAllocator {
 public:
  virtual ~StringAllocator() { }
  // Allocate a number of bytes.
  virtual char* allocate(unsigned bytes) = 0;
  // Allocate a larger number of bytes and copy the old buffer to the new one.
  // bytes is an input and output parameter passing the old size of the buffer
  // and returning the new size.  If allocation fails then we return the old
  // buffer and do not increase the size.
  virtual char* grow(unsigned* bytes) = 0;
};


// Normal allocator uses new[] and delete[].
class HeapStringAllocator V8_FINAL : public StringAllocator {
 public:
  ~HeapStringAllocator() { DeleteArray(space_); }
  virtual char* allocate(unsigned bytes) V8_OVERRIDE;
  virtual char* grow(unsigned* bytes) V8_OVERRIDE;

 private:
  char* space_;
};


// Allocator for use when no new c++ heap allocation is allowed.
// Given a preallocated buffer up front and does no allocation while
// building message.
class NoAllocationStringAllocator V8_FINAL : public StringAllocator {
 public:
  NoAllocationStringAllocator(char* memory, unsigned size);
  virtual char* allocate(unsigned bytes) V8_OVERRIDE { return space_; }
  virtual char* grow(unsigned* bytes) V8_OVERRIDE;

 private:
  unsigned size_;
  char* space_;
};


class FmtElm V8_FINAL {
 public:
  FmtElm(int value) : type_(INT) {  // NOLINT
    data_.u_int_ = value;
  }
  explicit FmtElm(double value) : type_(DOUBLE) {
    data_.u_double_ = value;
  }
  FmtElm(const char* value) : type_(C_STR) {  // NOLINT
    data_.u_c_str_ = value;
  }
  FmtElm(const Vector<const uc16>& value) : type_(LC_STR) {  // NOLINT
    data_.u_lc_str_ = &value;
  }
  FmtElm(Object* value) : type_(OBJ) {  // NOLINT
    data_.u_obj_ = value;
  }
  FmtElm(Handle<Object> value) : type_(HANDLE) {  // NOLINT
    data_.u_handle_ = value.location();
  }
  FmtElm(void* value) : type_(POINTER) {  // NOLINT
    data_.u_pointer_ = value;
  }

 private:
  friend class StringStream;
  enum Type { INT, DOUBLE, C_STR, LC_STR, OBJ, HANDLE, POINTER };
  Type type_;
  union {
    int u_int_;
    double u_double_;
    const char* u_c_str_;
    const Vector<const uc16>* u_lc_str_;
    Object* u_obj_;
    Object** u_handle_;
    void* u_pointer_;
  } data_;
};


class StringStream V8_FINAL {
 public:
  explicit StringStream(StringAllocator* allocator):
    allocator_(allocator),
    capacity_(kInitialCapacity),
    length_(0),
    buffer_(allocator_->allocate(kInitialCapacity)) {
    buffer_[0] = 0;
  }

  bool Put(char c);
  bool Put(String* str);
  bool Put(String* str, int start, int end);
  void Add(Vector<const char> format, Vector<FmtElm> elms);
  void Add(const char* format);
  void Add(Vector<const char> format);
  void Add(const char* format, FmtElm arg0);
  void Add(const char* format, FmtElm arg0, FmtElm arg1);
  void Add(const char* format, FmtElm arg0, FmtElm arg1, FmtElm arg2);
  void Add(const char* format,
           FmtElm arg0,
           FmtElm arg1,
           FmtElm arg2,
           FmtElm arg3);
  void Add(const char* format,
           FmtElm arg0,
           FmtElm arg1,
           FmtElm arg2,
           FmtElm arg3,
           FmtElm arg4);

  // Getting the message out.
  void OutputToFile(FILE* out);
  void OutputToStdOut() { OutputToFile(stdout); }
  void Log(Isolate* isolate);
  Handle<String> ToString(Isolate* isolate);
  SmartArrayPointer<const char> ToCString() const;
  int length() const { return length_; }

  // Object printing support.
  void PrintName(Object* o);
  void PrintFixedArray(FixedArray* array, unsigned int limit);
  void PrintByteArray(ByteArray* ba);
  void PrintUsingMap(JSObject* js_object);
  void PrintPrototype(JSFunction* fun, Object* receiver);
  void PrintSecurityTokenIfChanged(Object* function);
  // NOTE: Returns the code in the output parameter.
  void PrintFunction(Object* function, Object* receiver, Code** code);

  // Reset the stream.
  void Reset() {
    length_ = 0;
    buffer_[0] = 0;
  }

  // Mentioned object cache support.
  void PrintMentionedObjectCache(Isolate* isolate);
  static void ClearMentionedObjectCache(Isolate* isolate);
#ifdef DEBUG
  static bool IsMentionedObjectCacheClear(Isolate* isolate);
#endif

  static const int kInitialCapacity = 16;

 private:
  void PrintObject(Object* obj);

  StringAllocator* allocator_;
  unsigned capacity_;
  unsigned length_;  // does not include terminating 0-character
  char* buffer_;

  bool full() const { return (capacity_ - length_) == 1; }
  int space() const { return capacity_ - length_; }

  DISALLOW_IMPLICIT_CONSTRUCTORS(StringStream);
};


// Utility class to print a list of items to a stream, divided by a separator.
class SimpleListPrinter V8_FINAL {
 public:
  explicit SimpleListPrinter(StringStream* stream, char separator = ',') {
    separator_ = separator;
    stream_ = stream;
    first_ = true;
  }

  void Add(const char* str) {
    if (first_) {
      first_ = false;
    } else {
      stream_->Put(separator_);
    }
    stream_->Add(str);
  }

 private:
  bool first_;
  char separator_;
  StringStream* stream_;
};

} }  // namespace v8::internal

#endif  // V8_STRING_STREAM_H_
