#ifndef SRC_UTIL_INL_H_
#define SRC_UTIL_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "util.h"

namespace node {

template <typename T>
ListNode<T>::ListNode() : prev_(this), next_(this) {}

template <typename T>
ListNode<T>::~ListNode() {
  Remove();
}

template <typename T>
void ListNode<T>::Remove() {
  prev_->next_ = next_;
  next_->prev_ = prev_;
  prev_ = this;
  next_ = this;
}

template <typename T>
bool ListNode<T>::IsEmpty() const {
  return prev_ == this;
}

template <typename T, ListNodeMember(T) M>
ListHead<T, M>::Iterator::Iterator(ListNode<T>* node) : node_(node) {}

template <typename T, ListNodeMember(T) M>
T* ListHead<T, M>::Iterator::operator*() const {
  return ContainerOf(M, node_);
}

template <typename T, ListNodeMember(T) M>
const typename ListHead<T, M>::Iterator&
ListHead<T, M>::Iterator::operator++() {
  node_ = node_->next_;
  return *this;
}

template <typename T, ListNodeMember(T) M>
bool ListHead<T, M>::Iterator::operator!=(const Iterator& that) const {
  return node_ != that.node_;
}

template <typename T, ListNodeMember(T) M>
ListHead<T, M>::~ListHead() {
  while (IsEmpty() == false)
    head_.next_->Remove();
}

template <typename T, ListNodeMember(T) M>
void ListHead<T, M>::MoveBack(ListHead* that) {
  if (IsEmpty())
    return;
  ListNode<T>* to = &that->head_;
  head_.next_->prev_ = to->prev_;
  to->prev_->next_ = head_.next_;
  head_.prev_->next_ = to;
  to->prev_ = head_.prev_;
  head_.prev_ = &head_;
  head_.next_ = &head_;
}

template <typename T, ListNodeMember(T) M>
void ListHead<T, M>::PushBack(T* element) {
  ListNode<T>* that = &(element->*M);
  head_.prev_->next_ = that;
  that->prev_ = head_.prev_;
  that->next_ = &head_;
  head_.prev_ = that;
}

template <typename T, ListNodeMember(T) M>
void ListHead<T, M>::PushFront(T* element) {
  ListNode<T>* that = &(element->*M);
  head_.next_->prev_ = that;
  that->prev_ = &head_;
  that->next_ = head_.next_;
  head_.next_ = that;
}

template <typename T, ListNodeMember(T) M>
bool ListHead<T, M>::IsEmpty() const {
  return head_.IsEmpty();
}

template <typename T, ListNodeMember(T) M>
T* ListHead<T, M>::PopFront() {
  if (IsEmpty())
    return nullptr;
  ListNode<T>* node = head_.next_;
  node->Remove();
  return ContainerOf(M, node);
}

template <typename T, ListNodeMember(T) M>
typename ListHead<T, M>::Iterator ListHead<T, M>::begin() const {
  return Iterator(head_.next_);
}

template <typename T, ListNodeMember(T) M>
typename ListHead<T, M>::Iterator ListHead<T, M>::end() const {
  return Iterator(const_cast<ListNode<T>*>(&head_));
}

template <typename Inner, typename Outer>
ContainerOfHelper<Inner, Outer>::ContainerOfHelper(Inner Outer::*field,
                                                   Inner* pointer)
    : pointer_(reinterpret_cast<Outer*>(
          reinterpret_cast<uintptr_t>(pointer) -
          reinterpret_cast<uintptr_t>(&(static_cast<Outer*>(0)->*field)))) {
}

template <typename Inner, typename Outer>
template <typename TypeName>
ContainerOfHelper<Inner, Outer>::operator TypeName*() const {
  return static_cast<TypeName*>(pointer_);
}

template <typename Inner, typename Outer>
inline ContainerOfHelper<Inner, Outer> ContainerOf(Inner Outer::*field,
                                                   Inner* pointer) {
  return ContainerOfHelper<Inner, Outer>(field, pointer);
}

template <class TypeName>
inline v8::Local<TypeName> PersistentToLocal(
    v8::Isolate* isolate,
    const v8::Persistent<TypeName>& persistent) {
  if (persistent.IsWeak()) {
    return WeakPersistentToLocal(isolate, persistent);
  } else {
    return StrongPersistentToLocal(persistent);
  }
}

template <class TypeName>
inline v8::Local<TypeName> StrongPersistentToLocal(
    const v8::Persistent<TypeName>& persistent) {
  return *reinterpret_cast<v8::Local<TypeName>*>(
      const_cast<v8::Persistent<TypeName>*>(&persistent));
}

template <class TypeName>
inline v8::Local<TypeName> WeakPersistentToLocal(
    v8::Isolate* isolate,
    const v8::Persistent<TypeName>& persistent) {
  return v8::Local<TypeName>::New(isolate, persistent);
}

inline v8::Local<v8::String> OneByteString(v8::Isolate* isolate,
                                           const char* data,
                                           int length) {
  return v8::String::NewFromOneByte(isolate,
                                    reinterpret_cast<const uint8_t*>(data),
                                    v8::NewStringType::kNormal,
                                    length).ToLocalChecked();
}

inline v8::Local<v8::String> OneByteString(v8::Isolate* isolate,
                                           const signed char* data,
                                           int length) {
  return v8::String::NewFromOneByte(isolate,
                                    reinterpret_cast<const uint8_t*>(data),
                                    v8::NewStringType::kNormal,
                                    length).ToLocalChecked();
}

inline v8::Local<v8::String> OneByteString(v8::Isolate* isolate,
                                           const unsigned char* data,
                                           int length) {
  return v8::String::NewFromOneByte(isolate,
                                    reinterpret_cast<const uint8_t*>(data),
                                    v8::NewStringType::kNormal,
                                    length).ToLocalChecked();
}

template <typename TypeName>
void Wrap(v8::Local<v8::Object> object, TypeName* pointer) {
  CHECK_EQ(false, object.IsEmpty());
  CHECK_GT(object->InternalFieldCount(), 0);
  object->SetAlignedPointerInInternalField(0, pointer);
}

void ClearWrap(v8::Local<v8::Object> object) {
  Wrap<void>(object, nullptr);
}

template <typename TypeName>
TypeName* Unwrap(v8::Local<v8::Object> object) {
  CHECK_EQ(false, object.IsEmpty());
  CHECK_GT(object->InternalFieldCount(), 0);
  void* pointer = object->GetAlignedPointerFromInternalField(0);
  return static_cast<TypeName*>(pointer);
}

void SwapBytes16(char* dst, const char* src, size_t size) {
  for (size_t i = 0; i < size; i += 2) {
    char a = src[i + 0];
    char b = src[i + 1];
    dst[i + 0] = b;
    dst[i + 1] = a;
  }
}

void SwapBytes32(char* dst, const char* src, size_t size) {
  for (size_t i = 0; i < size; i += 4) {
    char a = src[i + 0];
    char b = src[i + 1];
    char c = src[i + 2];
    char d = src[i + 3];
    dst[i + 0] = d;
    dst[i + 1] = c;
    dst[i + 2] = b;
    dst[i + 3] = a;
  }
}

void SwapBytes64(char* dst, const char* src, size_t size) {
  for (size_t i = 0; i < size; i += 8) {
    char a = src[i + 0];
    char b = src[i + 1];
    char c = src[i + 2];
    char d = src[i + 3];
    char e = src[i + 4];
    char f = src[i + 5];
    char g = src[i + 6];
    char h = src[i + 7];
    dst[i + 0] = h;
    dst[i + 1] = g;
    dst[i + 2] = f;
    dst[i + 3] = e;
    dst[i + 4] = d;
    dst[i + 5] = c;
    dst[i + 6] = b;
    dst[i + 7] = a;
  }
}

char ToLower(char c) {
  return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}

bool StringEqualNoCase(const char* a, const char* b) {
  do {
    if (*a == '\0')
      return *b == '\0';
    if (*b == '\0')
      return *a == '\0';
  } while (ToLower(*a++) == ToLower(*b++));
  return false;
}

bool StringEqualNoCaseN(const char* a, const char* b, size_t length) {
  for (size_t i = 0; i < length; i++) {
    if (ToLower(a[i]) != ToLower(b[i]))
      return false;
    if (a[i] == '\0')
      return true;
  }
  return true;
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_UTIL_INL_H_
