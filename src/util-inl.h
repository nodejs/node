#ifndef SRC_UTIL_INL_H_
#define SRC_UTIL_INL_H_

#include "util.h"
#include "uv.h"

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
void ListHead<T, M>::Remove(T* element) {
  CHECK_NE(element, nullptr);
  CHECK(!IsEmpty());
  ListNode<T>* node = &(element->*M);
  CHECK(!node->IsEmpty());
  node->Remove();
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
                                    v8::String::kNormalString,
                                    length);
}

inline v8::Local<v8::String> OneByteString(v8::Isolate* isolate,
                                           const signed char* data,
                                           int length) {
  return v8::String::NewFromOneByte(isolate,
                                    reinterpret_cast<const uint8_t*>(data),
                                    v8::String::kNormalString,
                                    length);
}

inline v8::Local<v8::String> OneByteString(v8::Isolate* isolate,
                                           const unsigned char* data,
                                           int length) {
  return v8::String::NewFromOneByte(isolate,
                                    reinterpret_cast<const uint8_t*>(data),
                                    v8::String::kNormalString,
                                    length);
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

void BarrierInitAndWait(uv_barrier_t* barrier, int count) {
  CHECK_EQ(uv_barrier_init(barrier, count), 0);
  if (uv_barrier_wait(barrier) != 0)
    uv_barrier_destroy(barrier);
}

void BarrierWait(uv_barrier_t* barrier) {
  if (uv_barrier_wait(barrier) != 0)
    uv_barrier_destroy(barrier);
}

class ScopedLock {
  public:
    class Mutex {
      public:
        explicit Mutex(uv_mutex_t* lock) : lock_(lock) {
          if (lock_ != nullptr)
            uv_mutex_lock(lock_);
        }

        void unlock() {
          if (!unlocked_ && lock_ != nullptr) {
            uv_mutex_unlock(lock_);
            unlocked_ = true;
          }
        }

        ~Mutex() {
          unlock();
        }

      private:
        uv_mutex_t* lock_;
        bool unlocked_ = false;
        DISALLOW_COPY_AND_ASSIGN(Mutex);
    };

    class Read {
      public:
        explicit Read(uv_rwlock_t* lock) : lock_(lock) {
          if (lock_ != nullptr)
            uv_rwlock_rdlock(lock_);
        }

        void unlock() {
          if (!unlocked_ && lock_ != nullptr) {
            uv_rwlock_rdunlock(lock_);
            unlocked_ = true;
          }
        }

        ~Read() {
          unlock();
        }
      private:
        uv_rwlock_t* lock_;
        bool unlocked_ = false;
        DISALLOW_COPY_AND_ASSIGN(Read);
    };

    class Write {
      public:
        explicit Write(uv_rwlock_t* lock) : lock_(lock) {
          if (lock_ != nullptr)
            uv_rwlock_wrlock(lock_);
        }

        void unlock() {
          if (!unlocked_ && lock_ != nullptr) {
            uv_rwlock_wrunlock(lock_);
            unlocked_ = true;
          }
        }

        ~Write() {
          unlock();
        }
      private:
        uv_rwlock_t* lock_;
        bool unlocked_ = false;
        DISALLOW_COPY_AND_ASSIGN(Write);
    };
};

}  // namespace node

#endif  // SRC_UTIL_INL_H_
