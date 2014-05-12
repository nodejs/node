// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SMART_POINTERS_H_
#define V8_SMART_POINTERS_H_

namespace v8 {
namespace internal {


template<typename Deallocator, typename T>
class SmartPointerBase {
 public:
  // Default constructor. Constructs an empty scoped pointer.
  SmartPointerBase() : p_(NULL) {}

  // Constructs a scoped pointer from a plain one.
  explicit SmartPointerBase(T* ptr) : p_(ptr) {}

  // Copy constructor removes the pointer from the original to avoid double
  // freeing.
  SmartPointerBase(const SmartPointerBase<Deallocator, T>& rhs)
      : p_(rhs.p_) {
    const_cast<SmartPointerBase<Deallocator, T>&>(rhs).p_ = NULL;
  }

  T* operator->() const { return p_; }

  T& operator*() const { return *p_; }

  T* get() const { return p_; }

  // You can use [n] to index as if it was a plain pointer.
  T& operator[](size_t i) {
    return p_[i];
  }

  // You can use [n] to index as if it was a plain pointer.
  const T& operator[](size_t i) const {
    return p_[i];
  }

  // We don't have implicit conversion to a T* since that hinders migration:
  // You would not be able to change a method from returning a T* to
  // returning an SmartArrayPointer<T> and then get errors wherever it is used.


  // If you want to take out the plain pointer and don't want it automatically
  // deleted then call Detach().  Afterwards, the smart pointer is empty
  // (NULL).
  T* Detach() {
    T* temp = p_;
    p_ = NULL;
    return temp;
  }

  void Reset(T* new_value) {
    ASSERT(p_ == NULL || p_ != new_value);
    if (p_) Deallocator::Delete(p_);
    p_ = new_value;
  }

  // Assignment requires an empty (NULL) SmartArrayPointer as the receiver. Like
  // the copy constructor it removes the pointer in the original to avoid
  // double freeing.
  SmartPointerBase<Deallocator, T>& operator=(
      const SmartPointerBase<Deallocator, T>& rhs) {
    ASSERT(is_empty());
    T* tmp = rhs.p_;  // swap to handle self-assignment
    const_cast<SmartPointerBase<Deallocator, T>&>(rhs).p_ = NULL;
    p_ = tmp;
    return *this;
  }

  bool is_empty() const { return p_ == NULL; }

 protected:
  // When the destructor of the scoped pointer is executed the plain pointer
  // is deleted using DeleteArray.  This implies that you must allocate with
  // NewArray.
  ~SmartPointerBase() { if (p_) Deallocator::Delete(p_); }

 private:
  T* p_;
};

// A 'scoped array pointer' that calls DeleteArray on its pointer when the
// destructor is called.

template<typename T>
struct ArrayDeallocator {
  static void Delete(T* array) {
    DeleteArray(array);
  }
};


template<typename T>
class SmartArrayPointer: public SmartPointerBase<ArrayDeallocator<T>, T> {
 public:
  SmartArrayPointer() { }
  explicit SmartArrayPointer(T* ptr)
      : SmartPointerBase<ArrayDeallocator<T>, T>(ptr) { }
  SmartArrayPointer(const SmartArrayPointer<T>& rhs)
      : SmartPointerBase<ArrayDeallocator<T>, T>(rhs) { }
};


template<typename T>
struct ObjectDeallocator {
  static void Delete(T* object) {
    delete object;
  }
};


template<typename T>
class SmartPointer: public SmartPointerBase<ObjectDeallocator<T>, T> {
 public:
  SmartPointer() { }
  explicit SmartPointer(T* ptr)
      : SmartPointerBase<ObjectDeallocator<T>, T>(ptr) { }
  SmartPointer(const SmartPointer<T>& rhs)
      : SmartPointerBase<ObjectDeallocator<T>, T>(rhs) { }
};

} }  // namespace v8::internal

#endif  // V8_SMART_POINTERS_H_
