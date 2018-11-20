// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_STRING_STREAM_H_
#define V8_STRING_STREAM_H_

#include "src/allocation.h"
#include "src/handles.h"
#include "src/vector.h"

namespace v8 {
namespace internal {

// Forward declarations.
class ByteArray;

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
class HeapStringAllocator final : public StringAllocator {
 public:
  ~HeapStringAllocator() { DeleteArray(space_); }
  char* allocate(unsigned bytes) override;
  char* grow(unsigned* bytes) override;

 private:
  char* space_;
};


class FixedStringAllocator final : public StringAllocator {
 public:
  FixedStringAllocator(char* buffer, unsigned length)
      : buffer_(buffer), length_(length) {}
  ~FixedStringAllocator() override{};
  char* allocate(unsigned bytes) override;
  char* grow(unsigned* bytes) override;

 private:
  char* buffer_;
  unsigned length_;
  DISALLOW_COPY_AND_ASSIGN(FixedStringAllocator);
};

class StringStream final {
  class FmtElm final {
   public:
    FmtElm(int value) : FmtElm(INT) {  // NOLINT
      data_.u_int_ = value;
    }
    explicit FmtElm(double value) : FmtElm(DOUBLE) {  // NOLINT
      data_.u_double_ = value;
    }
    FmtElm(const char* value) : FmtElm(C_STR) {  // NOLINT
      data_.u_c_str_ = value;
    }
    FmtElm(const Vector<const uc16>& value) : FmtElm(LC_STR) {  // NOLINT
      data_.u_lc_str_ = &value;
    }
    FmtElm(Object* value) : FmtElm(OBJ) {  // NOLINT
      data_.u_obj_ = value;
    }
    FmtElm(Handle<Object> value) : FmtElm(HANDLE) {  // NOLINT
      data_.u_handle_ = value.location();
    }
    FmtElm(void* value) : FmtElm(POINTER) {  // NOLINT
      data_.u_pointer_ = value;
    }

   private:
    friend class StringStream;
    enum Type { INT, DOUBLE, C_STR, LC_STR, OBJ, HANDLE, POINTER };

#ifdef DEBUG
    Type type_;
    explicit FmtElm(Type type) : type_(type) {}
#else
    explicit FmtElm(Type) {}
#endif

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

 public:
  enum ObjectPrintMode { kPrintObjectConcise, kPrintObjectVerbose };
  StringStream(StringAllocator* allocator,
               ObjectPrintMode object_print_mode = kPrintObjectVerbose)
      : allocator_(allocator),
        object_print_mode_(object_print_mode),
        capacity_(kInitialCapacity),
        length_(0),
        buffer_(allocator_->allocate(kInitialCapacity)) {
    buffer_[0] = 0;
  }

  bool Put(char c);
  bool Put(String* str);
  bool Put(String* str, int start, int end);
  void Add(const char* format) { Add(CStrVector(format)); }
  void Add(Vector<const char> format) { Add(format, Vector<FmtElm>()); }

  template <typename... Args>
  void Add(const char* format, Args... args) {
    Add(CStrVector(format), args...);
  }

  template <typename... Args>
  void Add(Vector<const char> format, Args... args) {
    FmtElm elems[]{args...};
    Add(format, ArrayVector(elems));
  }

  // Getting the message out.
  void OutputToFile(FILE* out);
  void OutputToStdOut() { OutputToFile(stdout); }
  void Log(Isolate* isolate);
  Handle<String> ToString(Isolate* isolate);
  std::unique_ptr<char[]> ToCString() const;
  int length() const { return length_; }

  // Object printing support.
  void PrintName(Object* o);
  void PrintFixedArray(FixedArray* array, unsigned int limit);
  void PrintByteArray(ByteArray* ba);
  void PrintUsingMap(JSObject* js_object);
  void PrintPrototype(JSFunction* fun, Object* receiver);
  void PrintSecurityTokenIfChanged(JSFunction* function);
  // NOTE: Returns the code in the output parameter.
  void PrintFunction(JSFunction* function, Object* receiver, Code** code);

  // Reset the stream.
  void Reset() {
    length_ = 0;
    buffer_[0] = 0;
  }

  // Mentioned object cache support.
  void PrintMentionedObjectCache(Isolate* isolate);
  static void ClearMentionedObjectCache(Isolate* isolate);
#ifdef DEBUG
  bool IsMentionedObjectCacheClear(Isolate* isolate);
#endif

  static const int kInitialCapacity = 16;

 private:
  void Add(Vector<const char> format, Vector<FmtElm> elms);
  void PrintObject(Object* obj);

  StringAllocator* allocator_;
  ObjectPrintMode object_print_mode_;
  unsigned capacity_;
  unsigned length_;  // does not include terminating 0-character
  char* buffer_;

  bool full() const { return (capacity_ - length_) == 1; }
  int space() const { return capacity_ - length_; }

  DISALLOW_IMPLICIT_CONSTRUCTORS(StringStream);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_STRING_STREAM_H_
