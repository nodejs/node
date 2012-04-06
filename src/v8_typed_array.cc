// V8 Typed Array implementation.
// (c) Dean McNamee <dean@gmail.com>, 2011.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#include <stdlib.h>  // calloc, etc
#include <string.h>  // memmove

#include <v8.h>

#include "v8_typed_array.h"

namespace {

v8::Handle<v8::Value> ThrowError(const char* msg) {
  return v8::ThrowException(v8::Exception::Error(v8::String::New(msg)));
}

v8::Handle<v8::Value> ThrowTypeError(const char* msg) {
  return v8::ThrowException(v8::Exception::TypeError(v8::String::New(msg)));
}

v8::Handle<v8::Value> ThrowRangeError(const char* msg) {
  return v8::ThrowException(v8::Exception::RangeError(v8::String::New(msg)));
}

struct BatchedMethods {
  const char* name;
  v8::Handle<v8::Value> (*func)(const v8::Arguments& args);
};

class ArrayBuffer {
 public:
  static v8::Persistent<v8::FunctionTemplate> GetTemplate() {
    static v8::Persistent<v8::FunctionTemplate> ft_cache;
    if (!ft_cache.IsEmpty())
      return ft_cache;

    v8::HandleScope scope;
    ft_cache = v8::Persistent<v8::FunctionTemplate>::New(
        v8::FunctionTemplate::New(&ArrayBuffer::V8New));
    ft_cache->SetClassName(v8::String::New("ArrayBuffer"));
    v8::Local<v8::ObjectTemplate> instance = ft_cache->InstanceTemplate();
    instance->SetInternalFieldCount(1);  // Buffer.

    return ft_cache;
  }

  static bool HasInstance(v8::Handle<v8::Value> value) {
    return GetTemplate()->HasInstance(value);
  }

 private:
  static void WeakCallback(v8::Persistent<v8::Value> value, void* data) {
    v8::Object* obj = v8::Object::Cast(*value);

    void* ptr = obj->GetIndexedPropertiesExternalArrayData();
    int element_size = v8_typed_array::SizeOfArrayElementForType(
        obj->GetIndexedPropertiesExternalArrayDataType());
    int size =
        obj->GetIndexedPropertiesExternalArrayDataLength() * element_size;

    v8::V8::AdjustAmountOfExternalAllocatedMemory(-size);

    value.ClearWeak();
    value.Dispose();

    free(ptr);
  }

  static v8::Handle<v8::Value> V8New(const v8::Arguments& args) {
    if (!args.IsConstructCall())
      return ThrowTypeError("Constructor cannot be called as a function.");

    // To match Chrome, we allow "new ArrayBuffer()".
    // if (args.Length() != 1)
    //   return ThrowError("Wrong number of arguments.");

    if (args[0]->Int32Value() < 0) {
      return ThrowRangeError("ArrayBufferView size is not a small enough "
                             "positive integer.");
    }

    size_t num_bytes = args[0]->Uint32Value();
    void* buf = calloc(num_bytes, 1);
    if (!buf)
      return ThrowError("Unable to allocate ArrayBuffer.");

    args.This()->SetPointerInInternalField(0, buf);

    args.This()->Set(v8::String::New("byteLength"),
                     v8::Integer::NewFromUnsigned(num_bytes),
                     (v8::PropertyAttribute)(v8::ReadOnly|v8::DontDelete));

    // NOTE(deanm): This is not in the spec, you shouldn't be able to index
    // the ArrayBuffer.  However, it currently simplifies some handling in our
    // implementation, so we make ArrayView operator[] act like an Uint8Array.
    // , This allows DataView to work with both ArrayBuffers and TypedArrays.
    args.This()->SetIndexedPropertiesToExternalArrayData(
        buf, v8::kExternalUnsignedByteArray, num_bytes);

    v8::V8::AdjustAmountOfExternalAllocatedMemory(num_bytes);

    v8::Persistent<v8::Object> persistent =
        v8::Persistent<v8::Object>::New(args.This());
    persistent.MakeWeak(NULL, &ArrayBuffer::WeakCallback);

    return args.This();
  }
};

static bool checkAlignment(unsigned int val, unsigned int bytes) {
  return (val & (bytes - 1)) == 0;  // Handles bytes == 0.
}

template <unsigned int TBytes, v8::ExternalArrayType TEAType>
class TypedArray {
 public:
  static v8::Persistent<v8::FunctionTemplate> GetTemplate() {
    static v8::Persistent<v8::FunctionTemplate> ft_cache;
    if (!ft_cache.IsEmpty())
      return ft_cache;

    v8::HandleScope scope;
    ft_cache = v8::Persistent<v8::FunctionTemplate>::New(
        v8::FunctionTemplate::New(&TypedArray<TBytes, TEAType>::V8New));
    ft_cache->SetClassName(v8::String::New(TypeName()));
    v8::Local<v8::ObjectTemplate> instance = ft_cache->InstanceTemplate();
    instance->SetInternalFieldCount(0);

    ft_cache->Set(v8::String::New("BYTES_PER_ELEMENT"),
                  v8::Uint32::New(TBytes), v8::ReadOnly);
    instance->Set(v8::String::New("BYTES_PER_ELEMENT"),
                  v8::Uint32::New(TBytes), v8::ReadOnly);

    v8::Local<v8::Signature> default_signature = v8::Signature::New(ft_cache);

    static BatchedMethods methods[] = {
      { "set", &TypedArray<TBytes, TEAType>::set },
      { "slice", &TypedArray<TBytes, TEAType>::subarray },
      { "subarray", &TypedArray<TBytes, TEAType>::subarray },
    };

    for (size_t i = 0; i < sizeof(methods) / sizeof(*methods); ++i) {
      instance->Set(v8::String::New(methods[i].name),
                    v8::FunctionTemplate::New(methods[i].func,
                                              v8::Handle<v8::Value>(),
                                              default_signature));
    }

    return ft_cache;
  }

  static bool HasInstance(v8::Handle<v8::Value> value) {
    return GetTemplate()->HasInstance(value);
  }

 private:
  static v8::Handle<v8::Value> V8New(const v8::Arguments& args) {
    if (!args.IsConstructCall())
      return ThrowTypeError("Constructor cannot be called as a function.");

    // To match Chrome, we allow "new Float32Array()".
    // if (args.Length() != 1)
    //   return ThrowError("Wrong number of arguments.");

    v8::Local<v8::Object> buffer;
    unsigned int length = 0;
    unsigned int byte_offset = 0;

    if (ArrayBuffer::HasInstance(args[0])) {  // ArrayBuffer constructor.
      buffer = v8::Local<v8::Object>::Cast(args[0]);
      unsigned int buflen =
          buffer->GetIndexedPropertiesExternalArrayDataLength();

      if (args[1]->Int32Value() < 0)
        return ThrowRangeError("Byte offset out of range.");
      byte_offset = args[1]->Uint32Value();

      if (!checkAlignment(byte_offset, TBytes))
        return ThrowRangeError("Byte offset is not aligned.");

      if (args.Length() > 2) {
        if (args[2]->Int32Value() < 0)
          return ThrowRangeError("Length out of range.");
        length = args[2]->Uint32Value();
      } else {
        if (buflen < byte_offset ||
            !checkAlignment(buflen - byte_offset, TBytes)) {
          return ThrowRangeError("Byte offset / length is not aligned.");
        }
        length = (buflen - byte_offset) / TBytes;
      }

      // NOTE(deanm): Sloppy integer overflow checks.
      if (byte_offset > buflen || byte_offset + length > buflen ||
          byte_offset + length * TBytes > buflen) {
        return ThrowRangeError("Length is out of range.");
      }

      // TODO(deanm): Error check.
      void* buf = buffer->GetPointerFromInternalField(0);
      args.This()->SetIndexedPropertiesToExternalArrayData(
          reinterpret_cast<char*>(buf) + byte_offset, TEAType, length);
    } else if (args[0]->IsObject()) {  // TypedArray / type[] constructor.
      v8::Local<v8::Object> obj = v8::Local<v8::Object>::Cast(args[0]);
      length = obj->Get(v8::String::New("length"))->Uint32Value();

      // TODO(deanm): Handle integer overflow.
      v8::Handle<v8::Value> argv[1] = {
          v8::Integer::NewFromUnsigned(length * TBytes)};
      buffer = ArrayBuffer::GetTemplate()->
                 GetFunction()->NewInstance(1, argv);

      void* buf = buffer->GetPointerFromInternalField(0);
      args.This()->SetIndexedPropertiesToExternalArrayData(
          buf, TEAType, length);
      // TODO(deanm): check for failure.
      for (uint32_t i = 0; i < length; ++i) {
        // Use the v8 setter to deal with typing.  Maybe slow?
        args.This()->Set(i, obj->Get(i));
      }
    } else {  // length constructor.
      // Try to match Chrome, Float32Array(""), Float32Array(true/false) is
      // okay, but Float32Array(null) throws a TypeError and
      // Float32Array(undefined) throw a RangeError.
      if (args.Length() > 0 && (args[0]->IsUndefined() || args[0]->IsNull()))
        return ThrowTypeError("Type error");

      if (args[0]->Int32Value() < 0) {
        return ThrowRangeError("ArrayBufferView size is not a small enough "
                               "positive integer.");
      }

      length = args[0]->Uint32Value();
      // TODO(deanm): Handle integer overflow.
      v8::Handle<v8::Value> argv[1] = {
          v8::Integer::NewFromUnsigned(length * TBytes)};

      buffer = ArrayBuffer::GetTemplate()->
                 GetFunction()->NewInstance(1, argv);
      void* buf = buffer->GetPointerFromInternalField(0);

      args.This()->SetIndexedPropertiesToExternalArrayData(
          buf, TEAType, length);
      // TODO(deanm): check for failure.
    }

    args.This()->Set(v8::String::New("buffer"),
                     buffer,
                     (v8::PropertyAttribute)(v8::ReadOnly|v8::DontDelete));
    args.This()->Set(v8::String::New("length"),
                     v8::Integer::NewFromUnsigned(length),
                     (v8::PropertyAttribute)(v8::ReadOnly|v8::DontDelete));
    args.This()->Set(v8::String::New("byteOffset"),
                     v8::Integer::NewFromUnsigned(byte_offset),
                     (v8::PropertyAttribute)(v8::ReadOnly|v8::DontDelete));
    args.This()->Set(v8::String::New("byteLength"),
                     v8::Integer::NewFromUnsigned(length * TBytes),
                     (v8::PropertyAttribute)(v8::ReadOnly|v8::DontDelete));

    return args.This();
  }

  static v8::Handle<v8::Value> set(const v8::Arguments& args) {
    if (args.Length() < 1)
      return ThrowError("Wrong number of arguments.");

    if (!args[0]->IsObject())
      return ThrowTypeError("Type error.");

    v8::Handle<v8::Object> obj = v8::Handle<v8::Object>::Cast(args[0]);

    if (TypedArray<TBytes, TEAType>::HasInstance(obj)) {  // ArrayBufferView.
      v8::Handle<v8::Object> src_buffer = v8::Handle<v8::Object>::Cast(
          obj->Get(v8::String::New("buffer")));
      v8::Handle<v8::Object> dst_buffer = v8::Handle<v8::Object>::Cast(
          args.This()->Get(v8::String::New("buffer")));

      if (args[1]->Int32Value() < 0)
        return ThrowRangeError("Offset may not be negative.");

      unsigned int offset = args[1]->Uint32Value();
      unsigned int src_length =
          obj->Get(v8::String::New("length"))->Uint32Value();
      unsigned int dst_length =
          args.This()->Get(v8::String::New("length"))->Uint32Value();
      if (offset > dst_length)
        return ThrowRangeError("Offset out of range.");

      if (src_length > dst_length - offset)
        return ThrowRangeError("Offset/length out of range.");

      // We don't want to get the buffer pointer, because that means we'll have
      // to just do the calculations for byteOffset / byteLength again.
      // Instead just use the pointer on the external array data.
      void* src_ptr = obj->GetIndexedPropertiesExternalArrayData();
      void* dst_ptr = args.This()->GetIndexedPropertiesExternalArrayData();

      // From the spec:
      // If the input array is a TypedArray, the two arrays may use the same
      // underlying ArrayBuffer. In this situation, setting the values takes
      // place as if all the data is first copied into a temporary buffer that
      // does not overlap either of the arrays, and then the data from the
      // temporary buffer is copied into the current array.
      memmove(reinterpret_cast<char*>(dst_ptr) + offset * TBytes,
              src_ptr, src_length * TBytes);
    } else {  // type[]
      if (args[1]->Int32Value() < 0)
        return ThrowRangeError("Offset may not be negative.");

      unsigned int src_length =
          obj->Get(v8::String::New("length"))->Uint32Value();
      unsigned int dst_length =
          args.This()->Get(v8::String::New("length"))->Uint32Value();
      unsigned int offset = args[1]->Uint32Value();

      if (offset > dst_length)
        return ThrowRangeError("Offset out of range.");

      if (src_length > dst_length - offset)
        return ThrowRangeError("Offset/length out of range.");

      for (uint32_t i = 0; i < src_length; ++i) {
        // Use the v8 setter to deal with typing.  Maybe slow?
        args.This()->Set(i + offset, obj->Get(i));
      }
    }

    return v8::Undefined();
  }

  static v8::Handle<v8::Value> subarray(const v8::Arguments& args) {
    // TODO(deanm): The unsigned / signed type mixing makes me super nervous.

    unsigned int length =
        args.This()->Get(v8::String::New("length"))->Uint32Value();
    int begin = args[0]->Int32Value();
    int end = length;
    if (args.Length() > 1)
      end = args[1]->Int32Value();

    if (begin < 0) begin = length + begin;
    if (begin < 0) begin = 0;
    if ((unsigned)begin > length) begin = length;

    if (end < 0) end = length + end;
    if (end < 0) end = 0;
    if ((unsigned)end > length) end = length;

    if (begin > end) begin = end;

    int byte_offset = begin * TBytes +
        args.This()->Get(v8::String::New("byteOffset"))->Uint32Value();

    // Call through to the ArrayBuffer, byteOffset, length constructor.
    v8::Handle<v8::Value> argv[] = {
        args.This()->Get(v8::String::New("buffer")),
        v8::Integer::New(byte_offset),
        v8::Integer::New(end - begin)};
    return TypedArray<TBytes, TEAType>::GetTemplate()->
        GetFunction()->NewInstance(3, argv);
  }

  static const char* TypeName() {
    switch (TEAType) {
      case v8::kExternalByteArray: return "Int8Array";
      case v8::kExternalUnsignedByteArray: return "Uint8Array";
      case v8::kExternalShortArray: return "Int16Array";
      case v8::kExternalUnsignedShortArray: return "Uint16Array";
      case v8::kExternalIntArray: return "Int32Array";
      case v8::kExternalUnsignedIntArray: return "Uint32Array";
      case v8::kExternalFloatArray: return "Float32Array";
      case v8::kExternalDoubleArray: return "Float64Array";
    }
    abort();
    // Please the compiler
    return "";
  }
};

class Int8Array : public TypedArray<1, v8::kExternalByteArray> { };
class Uint8Array : public TypedArray<1, v8::kExternalUnsignedByteArray> { };
class Int16Array : public TypedArray<2, v8::kExternalShortArray> { };
class Uint16Array : public TypedArray<2, v8::kExternalUnsignedShortArray> { };
class Int32Array : public TypedArray<4, v8::kExternalIntArray> { };
class Uint32Array : public TypedArray<4, v8::kExternalUnsignedIntArray> { };
class Float32Array : public TypedArray<4, v8::kExternalFloatArray> { };
class Float64Array : public TypedArray<8, v8::kExternalDoubleArray> { };

template <typename T>
v8::Handle<v8::Value> cTypeToValue(T) {
  return v8::Undefined();
}

template <>
v8::Handle<v8::Value> cTypeToValue(unsigned char val) {
  return v8::Integer::NewFromUnsigned(val);
}

template <>
v8::Handle<v8::Value> cTypeToValue(char val) {
  return v8::Integer::New(val);
}

template <>
v8::Handle<v8::Value> cTypeToValue(unsigned short val) {
  return v8::Integer::NewFromUnsigned(val);
}

template <>
v8::Handle<v8::Value> cTypeToValue(short val) {
  return v8::Integer::New(val);
}

template <>
v8::Handle<v8::Value> cTypeToValue(unsigned int val) {
  return v8::Integer::NewFromUnsigned(val);
}

template <>
v8::Handle<v8::Value> cTypeToValue(int val) {
  return v8::Integer::New(val);
}

template <>
v8::Handle<v8::Value> cTypeToValue(float val) {
  return v8::Number::New(val);
}

template <>
v8::Handle<v8::Value> cTypeToValue(double val) {
  return v8::Number::New(val);
}


template <typename T>
T valueToCType(v8::Handle<v8::Value> value) {
  return 0;
}

template <>
unsigned char valueToCType(v8::Handle<v8::Value> value) {
  return value->Uint32Value();
}

template <>
char valueToCType(v8::Handle<v8::Value> value) {
  return value->Int32Value();
}

template <>
unsigned short valueToCType(v8::Handle<v8::Value> value) {
  return value->Uint32Value();
}

template <>
short valueToCType(v8::Handle<v8::Value> value) {
  return value->Int32Value();
}

template <>
unsigned int valueToCType(v8::Handle<v8::Value> value) {
  return value->Uint32Value();
}

template <>
int valueToCType(v8::Handle<v8::Value> value) {
  return value->Int32Value();
}

template <>
float valueToCType(v8::Handle<v8::Value> value) {
  return static_cast<float>(value->NumberValue());
}

template <>
double valueToCType(v8::Handle<v8::Value> value) {
  return value->NumberValue();
}


class DataView {
 public:
  static v8::Persistent<v8::FunctionTemplate> GetTemplate() {
    static v8::Persistent<v8::FunctionTemplate> ft_cache;
    if (!ft_cache.IsEmpty())
      return ft_cache;

    v8::HandleScope scope;
    ft_cache = v8::Persistent<v8::FunctionTemplate>::New(
        v8::FunctionTemplate::New(&DataView::V8New));
    ft_cache->SetClassName(v8::String::New("DataView"));
    v8::Local<v8::ObjectTemplate> instance = ft_cache->InstanceTemplate();
    instance->SetInternalFieldCount(0);

    v8::Local<v8::Signature> default_signature = v8::Signature::New(ft_cache);

    static BatchedMethods methods[] = {
      { "getUint8", &DataView::getUint8 },
      { "getInt8", &DataView::getInt8 },
      { "getUint16", &DataView::getUint16 },
      { "getInt16", &DataView::getInt16 },
      { "getUint32", &DataView::getUint32 },
      { "getInt32", &DataView::getInt32 },
      { "getFloat32", &DataView::getFloat32 },
      { "getFloat64", &DataView::getFloat64 },
      { "setUint8", &DataView::setUint8 },
      { "setInt8", &DataView::setInt8 },
      { "setUint16", &DataView::setUint16 },
      { "setInt16", &DataView::setInt16 },
      { "setUint32", &DataView::setUint32 },
      { "setInt32", &DataView::setInt32 },
      { "setFloat32", &DataView::setFloat32 },
      { "setFloat64", &DataView::setFloat64 },
    };

    for (size_t i = 0; i < sizeof(methods) / sizeof(*methods); ++i) {
      instance->Set(v8::String::New(methods[i].name),
                    v8::FunctionTemplate::New(methods[i].func,
                                              v8::Handle<v8::Value>(),
                                              default_signature));
    }

    return ft_cache;
  }

  static bool HasInstance(v8::Handle<v8::Value> value) {
    return GetTemplate()->HasInstance(value);
  }

 private:
  static v8::Handle<v8::Value> V8New(const v8::Arguments& args) {
    if (!args.IsConstructCall())
      return ThrowTypeError("Constructor cannot be called as a function.");

    if (args.Length() < 1)
      return ThrowError("Wrong number of arguments.");

    if (!args[0]->IsObject())
      return ThrowError("Object must be an ArrayBuffer.");

    v8::Handle<v8::Object> buffer = v8::Handle<v8::Object>::Cast(args[0]);
    if (!buffer->HasIndexedPropertiesInExternalArrayData())
      return ThrowError("Object must be an ArrayBuffer.");

    unsigned int byte_length =
        buffer->GetIndexedPropertiesExternalArrayDataLength();
    unsigned int byte_offset = args[1]->Uint32Value();

    if (args[1]->Int32Value() < 0 || byte_offset >= byte_length)
      return ThrowRangeError("byteOffset out of range.");

    if (!args[2]->IsUndefined()) {
      if (args[2]->Int32Value() < 0)
        return ThrowRangeError("byteLength out of range.");
      unsigned int new_byte_length = args[2]->Uint32Value();
      if (new_byte_length > byte_length)
        return ThrowRangeError("byteLength out of range.");
      if (byte_offset + new_byte_length > byte_length)
        return ThrowRangeError("byteOffset/byteLength out of range.");
      byte_length = new_byte_length;
    } else {
      // Adjust the original byte_length from total length to length to end.
      byte_length -= byte_offset;
    }

    void* buf = buffer->GetIndexedPropertiesExternalArrayData();

    // Like ArrayBuffer, we violate the spec and add an operator[].
    args.This()->SetIndexedPropertiesToExternalArrayData(
        reinterpret_cast<char*>(buf) + byte_offset,
        v8::kExternalUnsignedByteArray, byte_length);

    args.This()->Set(v8::String::New("buffer"),
                     buffer,
                     (v8::PropertyAttribute)(v8::ReadOnly|v8::DontDelete));
    args.This()->Set(v8::String::New("byteOffset"),
                     v8::Integer::NewFromUnsigned(byte_offset),
                     (v8::PropertyAttribute)(v8::ReadOnly|v8::DontDelete));
    args.This()->Set(v8::String::New("byteLength"),
                     v8::Integer::NewFromUnsigned(byte_length),
                     (v8::PropertyAttribute)(v8::ReadOnly|v8::DontDelete));
    return args.This();
  }

  // TODO(deanm): This isn't beautiful or optimal.
  static void swizzle(char* buf, size_t len) {
    for (size_t i = 0; i < len / 2; ++i) {
      char t = buf[i];
      buf[i] = buf[len - i - 1];
      buf[len - i - 1] = t;
    }
  }

  template <typename T>
  static T getValue(void* ptr, unsigned int index, bool swiz) {
    char buf[sizeof(T)];
    memcpy(buf, reinterpret_cast<char*>(ptr) + index, sizeof(T));
    if (swiz)
      swizzle(buf, sizeof(T));
    T val;
    memcpy(&val, buf, sizeof(T));
    return val;
  }

  template <typename T>
  static void setValue(void* ptr, unsigned int index, T val, bool swiz) {
    char buf[sizeof(T)];
    memcpy(buf, &val, sizeof(T));
    if (swiz)
      swizzle(buf, sizeof(T));
    memcpy(reinterpret_cast<char*>(ptr) + index, buf, sizeof(T));
  }

  template <typename T>
  static v8::Handle<v8::Value> getGeneric(const v8::Arguments& args) {
    if (args.Length() < 1)
      return ThrowError("Wrong number of arguments.");

    unsigned int index = args[0]->Uint32Value();
    bool little_endian = args[1]->BooleanValue();
    // TODO(deanm): All of these things should be cacheable.
    int element_size = v8_typed_array::SizeOfArrayElementForType(
        args.This()->GetIndexedPropertiesExternalArrayDataType());
    int size = args.This()->GetIndexedPropertiesExternalArrayDataLength() *
               element_size;

    if (index + sizeof(T) > (unsigned)size)  // TODO(deanm): integer overflow.
      return ThrowError("Index out of range.");

    void* ptr = args.This()->GetIndexedPropertiesExternalArrayData();
    return cTypeToValue<T>(getValue<T>(ptr, index, !little_endian));
  }

  template <typename T>
  static v8::Handle<v8::Value> setGeneric(const v8::Arguments& args) {
    if (args.Length() < 2)
      return ThrowError("Wrong number of arguments.");

    unsigned int index = args[0]->Int32Value();
    bool little_endian = args[2]->BooleanValue();
    // TODO(deanm): All of these things should be cacheable.
    int element_size = v8_typed_array::SizeOfArrayElementForType(
        args.This()->GetIndexedPropertiesExternalArrayDataType());
    int size = args.This()->GetIndexedPropertiesExternalArrayDataLength() *
               element_size;

    if (index + sizeof(T) > (unsigned)size)  // TODO(deanm): integer overflow.
      return ThrowError("Index out of range.");

    void* ptr = args.This()->GetIndexedPropertiesExternalArrayData();
    setValue<T>(ptr, index, valueToCType<T>(args[1]), !little_endian);
    return v8::Undefined();
  }

  static v8::Handle<v8::Value> getUint8(const v8::Arguments& args) {
    return getGeneric<unsigned char>(args);
  }

  static v8::Handle<v8::Value> getInt8(const v8::Arguments& args) {
    return getGeneric<char>(args);
  }

  static v8::Handle<v8::Value> getUint16(const v8::Arguments& args) {
    return getGeneric<unsigned short>(args);
  }

  static v8::Handle<v8::Value> getInt16(const v8::Arguments& args) {
    return getGeneric<short>(args);
  }

  static v8::Handle<v8::Value> getUint32(const v8::Arguments& args) {
    return getGeneric<unsigned int>(args);
  }

  static v8::Handle<v8::Value> getInt32(const v8::Arguments& args) {
    return getGeneric<int>(args);
  }

  static v8::Handle<v8::Value> getFloat32(const v8::Arguments& args) {
    return getGeneric<float>(args);
  }

  static v8::Handle<v8::Value> getFloat64(const v8::Arguments& args) {
    return getGeneric<double>(args);
  }

  static v8::Handle<v8::Value> setUint8(const v8::Arguments& args) {
    return setGeneric<unsigned char>(args);
  }

  static v8::Handle<v8::Value> setInt8(const v8::Arguments& args) {
    return setGeneric<char>(args);
  }

  static v8::Handle<v8::Value> setUint16(const v8::Arguments& args) {
    return setGeneric<unsigned short>(args);
  }

  static v8::Handle<v8::Value> setInt16(const v8::Arguments& args) {
    return setGeneric<short>(args);
  }

  static v8::Handle<v8::Value> setUint32(const v8::Arguments& args) {
    return setGeneric<unsigned int>(args);
  }

  static v8::Handle<v8::Value> setInt32(const v8::Arguments& args) {
    return setGeneric<int>(args);
  }

  static v8::Handle<v8::Value> setFloat32(const v8::Arguments& args) {
    return setGeneric<float>(args);
  }

  static v8::Handle<v8::Value> setFloat64(const v8::Arguments& args) {
    return setGeneric<double>(args);
  }
};


}  // namespace

namespace v8_typed_array {

void AttachBindings(v8::Handle<v8::Object> obj) {
  obj->Set(v8::String::New("ArrayBuffer"),
           ArrayBuffer::GetTemplate()->GetFunction());
  obj->Set(v8::String::New("Int8Array"),
           Int8Array::GetTemplate()->GetFunction());
  obj->Set(v8::String::New("Uint8Array"),
           Uint8Array::GetTemplate()->GetFunction());
  obj->Set(v8::String::New("Int16Array"),
           Int16Array::GetTemplate()->GetFunction());
  obj->Set(v8::String::New("Uint16Array"),
           Uint16Array::GetTemplate()->GetFunction());
  obj->Set(v8::String::New("Int32Array"),
           Int32Array::GetTemplate()->GetFunction());
  obj->Set(v8::String::New("Uint32Array"),
           Uint32Array::GetTemplate()->GetFunction());
  obj->Set(v8::String::New("Float32Array"),
           Float32Array::GetTemplate()->GetFunction());
  obj->Set(v8::String::New("Float64Array"),
           Float64Array::GetTemplate()->GetFunction());
  obj->Set(v8::String::New("DataView"),
           DataView::GetTemplate()->GetFunction());
}

int SizeOfArrayElementForType(v8::ExternalArrayType type) {
  switch (type) {
    case v8::kExternalByteArray:
    case v8::kExternalUnsignedByteArray:
      return 1;
    case v8::kExternalShortArray:
    case v8::kExternalUnsignedShortArray:
      return 2;
    case v8::kExternalIntArray:
    case v8::kExternalUnsignedIntArray:
    case v8::kExternalFloatArray:
      return 4;
    case v8::kExternalDoubleArray:
      return 8;
    default:
      return 0;
  }
}

}  // namespace v8_typed_array
