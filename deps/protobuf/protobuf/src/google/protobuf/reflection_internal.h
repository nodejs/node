// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef GOOGLE_PROTOBUF_REFLECTION_INTERNAL_H__
#define GOOGLE_PROTOBUF_REFLECTION_INTERNAL_H__

#include <google/protobuf/map_field.h>
#include <google/protobuf/reflection.h>
#include <google/protobuf/repeated_field.h>

namespace google {
namespace protobuf {
namespace internal {
// A base class for RepeatedFieldAccessor implementations that can support
// random-access efficiently. All iterator methods delegates the work to
// corresponding random-access methods.
class RandomAccessRepeatedFieldAccessor : public RepeatedFieldAccessor {
 public:
  Iterator* BeginIterator(const Field* data) const override {
    return PositionToIterator(0);
  }
  Iterator* EndIterator(const Field* data) const override {
    return PositionToIterator(this->Size(data));
  }
  Iterator* CopyIterator(const Field* data,
                         const Iterator* iterator) const override {
    return const_cast<Iterator*>(iterator);
  }
  Iterator* AdvanceIterator(const Field* data,
                            Iterator* iterator) const override {
    return PositionToIterator(IteratorToPosition(iterator) + 1);
  }
  bool EqualsIterator(const Field* data, const Iterator* a,
                      const Iterator* b) const override {
    return a == b;
  }
  void DeleteIterator(const Field* data, Iterator* iterator) const override {}
  const Value* GetIteratorValue(const Field* data, const Iterator* iterator,
                                Value* scratch_space) const override {
    return Get(data, static_cast<int>(IteratorToPosition(iterator)),
               scratch_space);
  }

 protected:
  ~RandomAccessRepeatedFieldAccessor() = default;

 private:
  static intptr_t IteratorToPosition(const Iterator* iterator) {
    return reinterpret_cast<intptr_t>(iterator);
  }
  static Iterator* PositionToIterator(intptr_t position) {
    return reinterpret_cast<Iterator*>(position);
  }
};

// Base class for RepeatedFieldAccessor implementations that manipulates
// RepeatedField<T>.
template <typename T>
class RepeatedFieldWrapper : public RandomAccessRepeatedFieldAccessor {
 public:
  RepeatedFieldWrapper() {}
  bool IsEmpty(const Field* data) const override {
    return GetRepeatedField(data)->empty();
  }
  int Size(const Field* data) const override {
    return GetRepeatedField(data)->size();
  }
  const Value* Get(const Field* data, int index,
                   Value* scratch_space) const override {
    return ConvertFromT(GetRepeatedField(data)->Get(index), scratch_space);
  }
  void Clear(Field* data) const override {
    MutableRepeatedField(data)->Clear();
  }
  void Set(Field* data, int index, const Value* value) const override {
    MutableRepeatedField(data)->Set(index, ConvertToT(value));
  }
  void Add(Field* data, const Value* value) const override {
    MutableRepeatedField(data)->Add(ConvertToT(value));
  }
  void RemoveLast(Field* data) const override {
    MutableRepeatedField(data)->RemoveLast();
  }
  void SwapElements(Field* data, int index1, int index2) const override {
    MutableRepeatedField(data)->SwapElements(index1, index2);
  }

 protected:
  ~RepeatedFieldWrapper() = default;
  typedef RepeatedField<T> RepeatedFieldType;
  static const RepeatedFieldType* GetRepeatedField(const Field* data) {
    return reinterpret_cast<const RepeatedFieldType*>(data);
  }
  static RepeatedFieldType* MutableRepeatedField(Field* data) {
    return reinterpret_cast<RepeatedFieldType*>(data);
  }

  // Convert an object recevied by this accessor to an object to be stored in
  // the underlying RepeatedField.
  virtual T ConvertToT(const Value* value) const = 0;

  // Convert an object stored in RepeatedPtrField to an object that will be
  // returned by this accessor. If the two objects have the same type (true for
  // string fields with ctype=STRING), a pointer to the source object can be
  // returned directly. Otherwise, data should be copied from value to
  // scratch_space and scratch_space should be returned.
  virtual const Value* ConvertFromT(const T& value,
                                    Value* scratch_space) const = 0;
};

// Base class for RepeatedFieldAccessor implementations that manipulates
// RepeatedPtrField<T>.
template <typename T>
class RepeatedPtrFieldWrapper : public RandomAccessRepeatedFieldAccessor {
 public:
  bool IsEmpty(const Field* data) const override {
    return GetRepeatedField(data)->empty();
  }
  int Size(const Field* data) const override {
    return GetRepeatedField(data)->size();
  }
  const Value* Get(const Field* data, int index,
                   Value* scratch_space) const override {
    return ConvertFromT(GetRepeatedField(data)->Get(index), scratch_space);
  }
  void Clear(Field* data) const override {
    MutableRepeatedField(data)->Clear();
  }
  void Set(Field* data, int index, const Value* value) const override {
    ConvertToT(value, MutableRepeatedField(data)->Mutable(index));
  }
  void Add(Field* data, const Value* value) const override {
    T* allocated = New(value);
    ConvertToT(value, allocated);
    MutableRepeatedField(data)->AddAllocated(allocated);
  }
  void RemoveLast(Field* data) const override {
    MutableRepeatedField(data)->RemoveLast();
  }
  void SwapElements(Field* data, int index1, int index2) const override {
    MutableRepeatedField(data)->SwapElements(index1, index2);
  }

 protected:
  ~RepeatedPtrFieldWrapper() = default;
  typedef RepeatedPtrField<T> RepeatedFieldType;
  static const RepeatedFieldType* GetRepeatedField(const Field* data) {
    return reinterpret_cast<const RepeatedFieldType*>(data);
  }
  static RepeatedFieldType* MutableRepeatedField(Field* data) {
    return reinterpret_cast<RepeatedFieldType*>(data);
  }

  // Create a new T instance. For repeated message fields, T can be specified
  // as google::protobuf::Message so we can't use "new T()" directly. In that case, value
  // should be a message of the same type (it's ensured by the caller) and a
  // new message object will be created using it.
  virtual T* New(const Value* value) const = 0;

  // Convert an object received by this accessor to an object that will be
  // stored in the underlying RepeatedPtrField.
  virtual void ConvertToT(const Value* value, T* result) const = 0;

  // Convert an object stored in RepeatedPtrField to an object that will be
  // returned by this accessor. If the two objects have the same type (true for
  // string fields with ctype=STRING), a pointer to the source object can be
  // returned directly. Otherwise, data should be copied from value to
  // scratch_space and scratch_space should be returned.
  virtual const Value* ConvertFromT(const T& value,
                                    Value* scratch_space) const = 0;
};

// An implementation of RandomAccessRepeatedFieldAccessor that manipulates
// MapFieldBase.
class MapFieldAccessor final : public RandomAccessRepeatedFieldAccessor {
 public:
  MapFieldAccessor() {}
  virtual ~MapFieldAccessor() {}
  bool IsEmpty(const Field* data) const override {
    return GetRepeatedField(data)->empty();
  }
  int Size(const Field* data) const override {
    return GetRepeatedField(data)->size();
  }
  const Value* Get(const Field* data, int index,
                   Value* scratch_space) const override {
    return ConvertFromEntry(GetRepeatedField(data)->Get(index), scratch_space);
  }
  void Clear(Field* data) const override {
    MutableRepeatedField(data)->Clear();
  }
  void Set(Field* data, int index, const Value* value) const override {
    ConvertToEntry(value, MutableRepeatedField(data)->Mutable(index));
  }
  void Add(Field* data, const Value* value) const override {
    Message* allocated = New(value);
    ConvertToEntry(value, allocated);
    MutableRepeatedField(data)->AddAllocated(allocated);
  }
  void RemoveLast(Field* data) const override {
    MutableRepeatedField(data)->RemoveLast();
  }
  void SwapElements(Field* data, int index1, int index2) const override {
    MutableRepeatedField(data)->SwapElements(index1, index2);
  }
  void Swap(Field* data, const internal::RepeatedFieldAccessor* other_mutator,
            Field* other_data) const override {
    GOOGLE_CHECK(this == other_mutator);
    MutableRepeatedField(data)->Swap(MutableRepeatedField(other_data));
  }

 protected:
  typedef RepeatedPtrField<Message> RepeatedFieldType;
  static const RepeatedFieldType* GetRepeatedField(const Field* data) {
    return reinterpret_cast<const RepeatedFieldType*>(
        (&reinterpret_cast<const MapFieldBase*>(data)->GetRepeatedField()));
  }
  static RepeatedFieldType* MutableRepeatedField(Field* data) {
    return reinterpret_cast<RepeatedFieldType*>(
        reinterpret_cast<MapFieldBase*>(data)->MutableRepeatedField());
  }
  virtual Message* New(const Value* value) const {
    return static_cast<const Message*>(value)->New();
  }
  // Convert an object received by this accessor to an MapEntry message to be
  // stored in the underlying MapFieldBase.
  virtual void ConvertToEntry(const Value* value, Message* result) const {
    result->CopyFrom(*static_cast<const Message*>(value));
  }
  // Convert a MapEntry message stored in the underlying MapFieldBase to an
  // object that will be returned by this accessor.
  virtual const Value* ConvertFromEntry(const Message& value,
                                        Value* scratch_space) const {
    return static_cast<const Value*>(&value);
  }
};

// Default implementations of RepeatedFieldAccessor for primitive types.
template <typename T>
class RepeatedFieldPrimitiveAccessor final : public RepeatedFieldWrapper<T> {
  typedef void Field;
  typedef void Value;
  using RepeatedFieldWrapper<T>::MutableRepeatedField;

 public:
  RepeatedFieldPrimitiveAccessor() {}
  void Swap(Field* data, const internal::RepeatedFieldAccessor* other_mutator,
            Field* other_data) const override {
    // Currently RepeatedFieldPrimitiveAccessor is the only implementation of
    // RepeatedFieldAccessor for primitive types. As we are using singletons
    // for these accessors, here "other_mutator" must be "this".
    GOOGLE_CHECK(this == other_mutator);
    MutableRepeatedField(data)->Swap(MutableRepeatedField(other_data));
  }

 protected:
  T ConvertToT(const Value* value) const override {
    return *static_cast<const T*>(value);
  }
  const Value* ConvertFromT(const T& value,
                            Value* scratch_space) const override {
    return static_cast<const Value*>(&value);
  }
};

// Default implementation of RepeatedFieldAccessor for string fields with
// ctype=STRING.
class RepeatedPtrFieldStringAccessor final
    : public RepeatedPtrFieldWrapper<std::string> {
  typedef void Field;
  typedef void Value;
  using RepeatedFieldAccessor::Add;

 public:
  RepeatedPtrFieldStringAccessor() {}
  void Swap(Field* data, const internal::RepeatedFieldAccessor* other_mutator,
            Field* other_data) const override {
    if (this == other_mutator) {
      MutableRepeatedField(data)->Swap(MutableRepeatedField(other_data));
    } else {
      RepeatedPtrField<std::string> tmp;
      tmp.Swap(MutableRepeatedField(data));
      int other_size = other_mutator->Size(other_data);
      for (int i = 0; i < other_size; ++i) {
        Add<std::string>(data, other_mutator->Get<std::string>(other_data, i));
      }
      int size = Size(data);
      other_mutator->Clear(other_data);
      for (int i = 0; i < size; ++i) {
        other_mutator->Add<std::string>(other_data, tmp.Get(i));
      }
    }
  }

 protected:
  std::string* New(const Value*) const override { return new std::string(); }
  void ConvertToT(const Value* value, std::string* result) const override {
    *result = *static_cast<const std::string*>(value);
  }
  const Value* ConvertFromT(const std::string& value,
                            Value* scratch_space) const override {
    return static_cast<const Value*>(&value);
  }
};


class RepeatedPtrFieldMessageAccessor final
    : public RepeatedPtrFieldWrapper<Message> {
  typedef void Field;
  typedef void Value;

 public:
  RepeatedPtrFieldMessageAccessor() {}
  void Swap(Field* data, const internal::RepeatedFieldAccessor* other_mutator,
            Field* other_data) const override {
    GOOGLE_CHECK(this == other_mutator);
    MutableRepeatedField(data)->Swap(MutableRepeatedField(other_data));
  }

 protected:
  Message* New(const Value* value) const override {
    return static_cast<const Message*>(value)->New();
  }
  void ConvertToT(const Value* value, Message* result) const override {
    result->CopyFrom(*static_cast<const Message*>(value));
  }
  const Value* ConvertFromT(const Message& value,
                            Value* scratch_space) const override {
    return static_cast<const Value*>(&value);
  }
};
}  // namespace internal
}  // namespace protobuf
}  // namespace google

#endif  // GOOGLE_PROTOBUF_REFLECTION_INTERNAL_H__
