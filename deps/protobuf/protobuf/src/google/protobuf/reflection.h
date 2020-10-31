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

// This header defines the RepeatedFieldRef class template used to access
// repeated fields with protobuf reflection API.
#ifndef GOOGLE_PROTOBUF_REFLECTION_H__
#define GOOGLE_PROTOBUF_REFLECTION_H__

#include <memory>

#include <google/protobuf/message.h>
#include <google/protobuf/generated_enum_util.h>

#ifdef SWIG
#error "You cannot SWIG proto headers"
#endif

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {
namespace internal {
template <typename T, typename Enable = void>
struct RefTypeTraits;
}  // namespace internal

template <typename T>
RepeatedFieldRef<T> Reflection::GetRepeatedFieldRef(
    const Message& message, const FieldDescriptor* field) const {
  return RepeatedFieldRef<T>(message, field);
}

template <typename T>
MutableRepeatedFieldRef<T> Reflection::GetMutableRepeatedFieldRef(
    Message* message, const FieldDescriptor* field) const {
  return MutableRepeatedFieldRef<T>(message, field);
}

// RepeatedFieldRef definition for non-message types.
template <typename T>
class RepeatedFieldRef<
    T, typename std::enable_if<!std::is_base_of<Message, T>::value>::type> {
  typedef typename internal::RefTypeTraits<T>::iterator IteratorType;
  typedef typename internal::RefTypeTraits<T>::AccessorType AccessorType;

 public:
  bool empty() const { return accessor_->IsEmpty(data_); }
  int size() const { return accessor_->Size(data_); }
  T Get(int index) const { return accessor_->template Get<T>(data_, index); }

  typedef IteratorType iterator;
  typedef IteratorType const_iterator;
  typedef T value_type;
  typedef T& reference;
  typedef const T& const_reference;
  typedef int size_type;
  typedef ptrdiff_t difference_type;

  iterator begin() const { return iterator(data_, accessor_, true); }
  iterator end() const { return iterator(data_, accessor_, false); }

 private:
  friend class Reflection;
  RepeatedFieldRef(const Message& message, const FieldDescriptor* field) {
    const Reflection* reflection = message.GetReflection();
    data_ = reflection->RepeatedFieldData(const_cast<Message*>(&message), field,
                                          internal::RefTypeTraits<T>::cpp_type,
                                          NULL);
    accessor_ = reflection->RepeatedFieldAccessor(field);
  }

  const void* data_;
  const AccessorType* accessor_;
};

// MutableRepeatedFieldRef definition for non-message types.
template <typename T>
class MutableRepeatedFieldRef<
    T, typename std::enable_if<!std::is_base_of<Message, T>::value>::type> {
  typedef typename internal::RefTypeTraits<T>::AccessorType AccessorType;

 public:
  bool empty() const { return accessor_->IsEmpty(data_); }
  int size() const { return accessor_->Size(data_); }
  T Get(int index) const { return accessor_->template Get<T>(data_, index); }

  void Set(int index, const T& value) const {
    accessor_->template Set<T>(data_, index, value);
  }
  void Add(const T& value) const { accessor_->template Add<T>(data_, value); }
  void RemoveLast() const { accessor_->RemoveLast(data_); }
  void SwapElements(int index1, int index2) const {
    accessor_->SwapElements(data_, index1, index2);
  }
  void Clear() const { accessor_->Clear(data_); }

  void Swap(const MutableRepeatedFieldRef& other) const {
    accessor_->Swap(data_, other.accessor_, other.data_);
  }

  template <typename Container>
  void MergeFrom(const Container& container) const {
    typedef typename Container::const_iterator Iterator;
    for (Iterator it = container.begin(); it != container.end(); ++it) {
      Add(*it);
    }
  }
  template <typename Container>
  void CopyFrom(const Container& container) const {
    Clear();
    MergeFrom(container);
  }

 private:
  friend class Reflection;
  MutableRepeatedFieldRef(Message* message, const FieldDescriptor* field) {
    const Reflection* reflection = message->GetReflection();
    data_ = reflection->RepeatedFieldData(
        message, field, internal::RefTypeTraits<T>::cpp_type, NULL);
    accessor_ = reflection->RepeatedFieldAccessor(field);
  }

  void* data_;
  const AccessorType* accessor_;
};

// RepeatedFieldRef definition for message types.
template <typename T>
class RepeatedFieldRef<
    T, typename std::enable_if<std::is_base_of<Message, T>::value>::type> {
  typedef typename internal::RefTypeTraits<T>::iterator IteratorType;
  typedef typename internal::RefTypeTraits<T>::AccessorType AccessorType;

 public:
  bool empty() const { return accessor_->IsEmpty(data_); }
  int size() const { return accessor_->Size(data_); }
  // This method returns a reference to the underlying message object if it
  // exists. If a message object doesn't exist (e.g., data stored in serialized
  // form), scratch_space will be filled with the data and a reference to it
  // will be returned.
  //
  // Example:
  //   RepeatedFieldRef<Message> h = ...
  //   unique_ptr<Message> scratch_space(h.NewMessage());
  //   const Message& item = h.Get(index, scratch_space.get());
  const T& Get(int index, T* scratch_space) const {
    return *static_cast<const T*>(accessor_->Get(data_, index, scratch_space));
  }
  // Create a new message of the same type as the messages stored in this
  // repeated field. Caller takes ownership of the returned object.
  T* NewMessage() const { return static_cast<T*>(default_instance_->New()); }

  typedef IteratorType iterator;
  typedef IteratorType const_iterator;
  typedef T value_type;
  typedef T& reference;
  typedef const T& const_reference;
  typedef int size_type;
  typedef ptrdiff_t difference_type;

  iterator begin() const {
    return iterator(data_, accessor_, true, NewMessage());
  }
  iterator end() const {
    // The end iterator must not be dereferenced, no need for scratch space.
    return iterator(data_, accessor_, false, nullptr);
  }

 private:
  friend class Reflection;
  RepeatedFieldRef(const Message& message, const FieldDescriptor* field) {
    const Reflection* reflection = message.GetReflection();
    data_ = reflection->RepeatedFieldData(
        const_cast<Message*>(&message), field,
        internal::RefTypeTraits<T>::cpp_type,
        internal::RefTypeTraits<T>::GetMessageFieldDescriptor());
    accessor_ = reflection->RepeatedFieldAccessor(field);
    default_instance_ =
        reflection->GetMessageFactory()->GetPrototype(field->message_type());
  }

  const void* data_;
  const AccessorType* accessor_;
  const Message* default_instance_;
};

// MutableRepeatedFieldRef definition for message types.
template <typename T>
class MutableRepeatedFieldRef<
    T, typename std::enable_if<std::is_base_of<Message, T>::value>::type> {
  typedef typename internal::RefTypeTraits<T>::AccessorType AccessorType;

 public:
  bool empty() const { return accessor_->IsEmpty(data_); }
  int size() const { return accessor_->Size(data_); }
  // See comments for RepeatedFieldRef<Message>::Get()
  const T& Get(int index, T* scratch_space) const {
    return *static_cast<const T*>(accessor_->Get(data_, index, scratch_space));
  }
  // Create a new message of the same type as the messages stored in this
  // repeated field. Caller takes ownership of the returned object.
  T* NewMessage() const { return static_cast<T*>(default_instance_->New()); }

  void Set(int index, const T& value) const {
    accessor_->Set(data_, index, &value);
  }
  void Add(const T& value) const { accessor_->Add(data_, &value); }
  void RemoveLast() const { accessor_->RemoveLast(data_); }
  void SwapElements(int index1, int index2) const {
    accessor_->SwapElements(data_, index1, index2);
  }
  void Clear() const { accessor_->Clear(data_); }

  void Swap(const MutableRepeatedFieldRef& other) const {
    accessor_->Swap(data_, other.accessor_, other.data_);
  }

  template <typename Container>
  void MergeFrom(const Container& container) const {
    typedef typename Container::const_iterator Iterator;
    for (Iterator it = container.begin(); it != container.end(); ++it) {
      Add(*it);
    }
  }
  template <typename Container>
  void CopyFrom(const Container& container) const {
    Clear();
    MergeFrom(container);
  }

 private:
  friend class Reflection;
  MutableRepeatedFieldRef(Message* message, const FieldDescriptor* field) {
    const Reflection* reflection = message->GetReflection();
    data_ = reflection->RepeatedFieldData(
        message, field, internal::RefTypeTraits<T>::cpp_type,
        internal::RefTypeTraits<T>::GetMessageFieldDescriptor());
    accessor_ = reflection->RepeatedFieldAccessor(field);
    default_instance_ =
        reflection->GetMessageFactory()->GetPrototype(field->message_type());
  }

  void* data_;
  const AccessorType* accessor_;
  const Message* default_instance_;
};

namespace internal {
// Interfaces used to implement reflection RepeatedFieldRef API.
// Reflection::GetRepeatedAccessor() should return a pointer to an singleton
// object that implements the below interface.
//
// This interface passes/returns values using void pointers. The actual type
// of the value depends on the field's cpp_type. Following is a mapping from
// cpp_type to the type that should be used in this interface:
//
//   field->cpp_type()      T                Actual type of void*
//   CPPTYPE_INT32        int32                   int32
//   CPPTYPE_UINT32       uint32                  uint32
//   CPPTYPE_INT64        int64                   int64
//   CPPTYPE_UINT64       uint64                  uint64
//   CPPTYPE_DOUBLE       double                  double
//   CPPTYPE_FLOAT        float                   float
//   CPPTYPE_BOOL         bool                    bool
//   CPPTYPE_ENUM         generated enum type     int32
//   CPPTYPE_STRING       string                  std::string
//   CPPTYPE_MESSAGE      generated message type  google::protobuf::Message
//                        or google::protobuf::Message
//
// Note that for enums we use int32 in the interface.
//
// You can map from T to the actual type using RefTypeTraits:
//   typedef RefTypeTraits<T>::AccessorValueType ActualType;
class PROTOBUF_EXPORT RepeatedFieldAccessor {
 public:
  // Typedefs for clarity.
  typedef void Field;
  typedef void Value;
  typedef void Iterator;

  virtual bool IsEmpty(const Field* data) const = 0;
  virtual int Size(const Field* data) const = 0;
  // Depends on the underlying representation of the repeated field, this
  // method can return a pointer to the underlying object if such an object
  // exists, or fill the data into scratch_space and return scratch_space.
  // Callers of this method must ensure scratch_space is a valid pointer
  // to a mutable object of the correct type.
  virtual const Value* Get(const Field* data, int index,
                           Value* scratch_space) const = 0;

  virtual void Clear(Field* data) const = 0;
  virtual void Set(Field* data, int index, const Value* value) const = 0;
  virtual void Add(Field* data, const Value* value) const = 0;
  virtual void RemoveLast(Field* data) const = 0;
  virtual void SwapElements(Field* data, int index1, int index2) const = 0;
  virtual void Swap(Field* data, const RepeatedFieldAccessor* other_mutator,
                    Field* other_data) const = 0;

  // Create an iterator that points at the beginning of the repeated field.
  virtual Iterator* BeginIterator(const Field* data) const = 0;
  // Create an iterator that points at the end of the repeated field.
  virtual Iterator* EndIterator(const Field* data) const = 0;
  // Make a copy of an iterator and return the new copy.
  virtual Iterator* CopyIterator(const Field* data,
                                 const Iterator* iterator) const = 0;
  // Move an iterator to point to the next element.
  virtual Iterator* AdvanceIterator(const Field* data,
                                    Iterator* iterator) const = 0;
  // Compare whether two iterators point to the same element.
  virtual bool EqualsIterator(const Field* data, const Iterator* a,
                              const Iterator* b) const = 0;
  // Delete an iterator created by BeginIterator(), EndIterator() and
  // CopyIterator().
  virtual void DeleteIterator(const Field* data, Iterator* iterator) const = 0;
  // Like Get() but for iterators.
  virtual const Value* GetIteratorValue(const Field* data,
                                        const Iterator* iterator,
                                        Value* scratch_space) const = 0;

  // Templated methods that make using this interface easier for non-message
  // types.
  template <typename T>
  T Get(const Field* data, int index) const {
    typedef typename RefTypeTraits<T>::AccessorValueType ActualType;
    ActualType scratch_space;
    return static_cast<T>(*reinterpret_cast<const ActualType*>(
        Get(data, index, static_cast<Value*>(&scratch_space))));
  }

  template <typename T, typename ValueType>
  void Set(Field* data, int index, const ValueType& value) const {
    typedef typename RefTypeTraits<T>::AccessorValueType ActualType;
    // In this RepeatedFieldAccessor interface we pass/return data using
    // raw pointers. Type of the data these raw pointers point to should
    // be ActualType. Here we have a ValueType object and want a ActualType
    // pointer. We can't cast a ValueType pointer to an ActualType pointer
    // directly because their type might be different (for enums ValueType
    // may be a generated enum type while ActualType is int32). To be safe
    // we make a copy to get a temporary ActualType object and use it.
    ActualType tmp = static_cast<ActualType>(value);
    Set(data, index, static_cast<const Value*>(&tmp));
  }

  template <typename T, typename ValueType>
  void Add(Field* data, const ValueType& value) const {
    typedef typename RefTypeTraits<T>::AccessorValueType ActualType;
    // In this RepeatedFieldAccessor interface we pass/return data using
    // raw pointers. Type of the data these raw pointers point to should
    // be ActualType. Here we have a ValueType object and want a ActualType
    // pointer. We can't cast a ValueType pointer to an ActualType pointer
    // directly because their type might be different (for enums ValueType
    // may be a generated enum type while ActualType is int32). To be safe
    // we make a copy to get a temporary ActualType object and use it.
    ActualType tmp = static_cast<ActualType>(value);
    Add(data, static_cast<const Value*>(&tmp));
  }

 protected:
  // We want the destructor to be completely trivial as to allow it to be
  // a function local static. Hence we make it non-virtual and protected,
  // this class only live as part of a global singleton and should not be
  // deleted.
  ~RepeatedFieldAccessor() = default;
};

// Implement (Mutable)RepeatedFieldRef::iterator
template <typename T>
class RepeatedFieldRefIterator
    : public std::iterator<std::forward_iterator_tag, T> {
  typedef typename RefTypeTraits<T>::AccessorValueType AccessorValueType;
  typedef typename RefTypeTraits<T>::IteratorValueType IteratorValueType;
  typedef typename RefTypeTraits<T>::IteratorPointerType IteratorPointerType;

 public:
  // Constructor for non-message fields.
  RepeatedFieldRefIterator(const void* data,
                           const RepeatedFieldAccessor* accessor, bool begin)
      : data_(data),
        accessor_(accessor),
        iterator_(begin ? accessor->BeginIterator(data)
                        : accessor->EndIterator(data)),
        // The end iterator must not be dereferenced, no need for scratch space.
        scratch_space_(begin ? new AccessorValueType : nullptr) {}
  // Constructor for message fields.
  RepeatedFieldRefIterator(const void* data,
                           const RepeatedFieldAccessor* accessor, bool begin,
                           AccessorValueType* scratch_space)
      : data_(data),
        accessor_(accessor),
        iterator_(begin ? accessor->BeginIterator(data)
                        : accessor->EndIterator(data)),
        scratch_space_(scratch_space) {}
  ~RepeatedFieldRefIterator() { accessor_->DeleteIterator(data_, iterator_); }
  RepeatedFieldRefIterator operator++(int) {
    RepeatedFieldRefIterator tmp(*this);
    iterator_ = accessor_->AdvanceIterator(data_, iterator_);
    return tmp;
  }
  RepeatedFieldRefIterator& operator++() {
    iterator_ = accessor_->AdvanceIterator(data_, iterator_);
    return *this;
  }
  IteratorValueType operator*() const {
    return static_cast<IteratorValueType>(
        *static_cast<const AccessorValueType*>(accessor_->GetIteratorValue(
            data_, iterator_, scratch_space_.get())));
  }
  IteratorPointerType operator->() const {
    return static_cast<IteratorPointerType>(
        accessor_->GetIteratorValue(data_, iterator_, scratch_space_.get()));
  }
  bool operator!=(const RepeatedFieldRefIterator& other) const {
    assert(data_ == other.data_);
    assert(accessor_ == other.accessor_);
    return !accessor_->EqualsIterator(data_, iterator_, other.iterator_);
  }
  bool operator==(const RepeatedFieldRefIterator& other) const {
    return !this->operator!=(other);
  }

  RepeatedFieldRefIterator(const RepeatedFieldRefIterator& other)
      : data_(other.data_),
        accessor_(other.accessor_),
        iterator_(accessor_->CopyIterator(data_, other.iterator_)) {}
  RepeatedFieldRefIterator& operator=(const RepeatedFieldRefIterator& other) {
    if (this != &other) {
      accessor_->DeleteIterator(data_, iterator_);
      data_ = other.data_;
      accessor_ = other.accessor_;
      iterator_ = accessor_->CopyIterator(data_, other.iterator_);
    }
    return *this;
  }

 protected:
  const void* data_;
  const RepeatedFieldAccessor* accessor_;
  void* iterator_;
  std::unique_ptr<AccessorValueType> scratch_space_;
};

// TypeTraits that maps the type parameter T of RepeatedFieldRef or
// MutableRepeatedFieldRef to corresponding iterator type,
// RepeatedFieldAccessor type, etc.
template <typename T>
struct PrimitiveTraits {
  static const bool is_primitive = false;
};
#define DEFINE_PRIMITIVE(TYPE, type)                 \
  template <>                                        \
  struct PrimitiveTraits<type> {                     \
    static const bool is_primitive = true;           \
    static const FieldDescriptor::CppType cpp_type = \
        FieldDescriptor::CPPTYPE_##TYPE;             \
  };
DEFINE_PRIMITIVE(INT32, int32)
DEFINE_PRIMITIVE(UINT32, uint32)
DEFINE_PRIMITIVE(INT64, int64)
DEFINE_PRIMITIVE(UINT64, uint64)
DEFINE_PRIMITIVE(FLOAT, float)
DEFINE_PRIMITIVE(DOUBLE, double)
DEFINE_PRIMITIVE(BOOL, bool)
#undef DEFINE_PRIMITIVE

template <typename T>
struct RefTypeTraits<
    T, typename std::enable_if<PrimitiveTraits<T>::is_primitive>::type> {
  typedef RepeatedFieldRefIterator<T> iterator;
  typedef RepeatedFieldAccessor AccessorType;
  typedef T AccessorValueType;
  typedef T IteratorValueType;
  typedef T* IteratorPointerType;
  static const FieldDescriptor::CppType cpp_type = PrimitiveTraits<T>::cpp_type;
  static const Descriptor* GetMessageFieldDescriptor() { return NULL; }
};

template <typename T>
struct RefTypeTraits<
    T, typename std::enable_if<is_proto_enum<T>::value>::type> {
  typedef RepeatedFieldRefIterator<T> iterator;
  typedef RepeatedFieldAccessor AccessorType;
  // We use int32 for repeated enums in RepeatedFieldAccessor.
  typedef int32 AccessorValueType;
  typedef T IteratorValueType;
  typedef int32* IteratorPointerType;
  static const FieldDescriptor::CppType cpp_type =
      FieldDescriptor::CPPTYPE_ENUM;
  static const Descriptor* GetMessageFieldDescriptor() { return NULL; }
};

template <typename T>
struct RefTypeTraits<
    T, typename std::enable_if<std::is_same<std::string, T>::value>::type> {
  typedef RepeatedFieldRefIterator<T> iterator;
  typedef RepeatedFieldAccessor AccessorType;
  typedef std::string AccessorValueType;
  typedef const std::string IteratorValueType;
  typedef const std::string* IteratorPointerType;
  static const FieldDescriptor::CppType cpp_type =
      FieldDescriptor::CPPTYPE_STRING;
  static const Descriptor* GetMessageFieldDescriptor() { return NULL; }
};

template <typename T>
struct MessageDescriptorGetter {
  static const Descriptor* get() {
    return T::default_instance().GetDescriptor();
  }
};
template <>
struct MessageDescriptorGetter<Message> {
  static const Descriptor* get() { return NULL; }
};

template <typename T>
struct RefTypeTraits<
    T, typename std::enable_if<std::is_base_of<Message, T>::value>::type> {
  typedef RepeatedFieldRefIterator<T> iterator;
  typedef RepeatedFieldAccessor AccessorType;
  typedef Message AccessorValueType;
  typedef const T& IteratorValueType;
  typedef const T* IteratorPointerType;
  static const FieldDescriptor::CppType cpp_type =
      FieldDescriptor::CPPTYPE_MESSAGE;
  static const Descriptor* GetMessageFieldDescriptor() {
    return MessageDescriptorGetter<T>::get();
  }
};
}  // namespace internal
}  // namespace protobuf
}  // namespace google

#include <google/protobuf/port_undef.inc>

#endif  // GOOGLE_PROTOBUF_REFLECTION_H__
