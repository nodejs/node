#ifndef SRC_FREELIST_H_
#define SRC_FREELIST_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "util.h"

namespace node {

struct DefaultFreelistTraits;

template <typename T,
          size_t kMaximumLength,
          typename FreelistTraits = DefaultFreelistTraits>
class Freelist {
 public:
  typedef struct list_item {
    T* item = nullptr;
    list_item* next = nullptr;
  } list_item;

  Freelist() {}
  ~Freelist() {
    while (head_ != nullptr) {
      list_item* item = head_;
      head_ = item->next;
      FreelistTraits::Free(item->item);
      free(item);
    }
  }

  void push(T* item) {
    if (size_ > kMaximumLength) {
      FreelistTraits::Free(item);
    } else {
      size_++;
      FreelistTraits::Reset(item);
      list_item* li = Calloc<list_item>(1);
      li->item = item;
      if (head_ == nullptr) {
        head_ = li;
        tail_ = li;
      } else {
        tail_->next = li;
        tail_ = li;
      }
    }
  }

  T* pop() {
    if (head_ != nullptr) {
      size_--;
      list_item* cur = head_;
      T* item = cur->item;
      head_ = cur->next;
      free(cur);
      return item;
    } else {
      return FreelistTraits::template Alloc<T>();
    }
  }

 private:
  size_t size_ = 0;
  list_item* head_ = nullptr;
  list_item* tail_ = nullptr;
};

struct DefaultFreelistTraits {
  template <typename T>
  static T* Alloc() {
    return ::new (Malloc<T>(1)) T();
  }

  template <typename T>
  static void Free(T* item) {
    item->~T();
    free(item);
  }

  template <typename T>
  static void Reset(T* item) {
    item->~T();
    ::new (item) T();
  }
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_FREELIST_H_
