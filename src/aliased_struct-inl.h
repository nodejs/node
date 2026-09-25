#ifndef SRC_ALIASED_STRUCT_INL_H_
#define SRC_ALIASED_STRUCT_INL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "aliased_struct.h"
#include "v8.h"
#include <memory>

namespace node {

template <typename T>
template <typename... Args>
AliasedStruct<T>::AliasedStruct(v8::Isolate* isolate, Args&&... args)
    : isolate_(isolate) {
  const v8::HandleScope handle_scope(isolate);

  store_ = v8::ArrayBuffer::NewBackingStore(isolate, sizeof(T));
  ptr_ = new (store_->Data()) T(std::forward<Args>(args)...);
  DCHECK_NOT_NULL(ptr_);

  v8::Local<v8::ArrayBuffer> buffer = v8::ArrayBuffer::New(isolate, store_);
  buffer_ = v8::Global<v8::ArrayBuffer>(isolate, buffer);
}

template <typename T>
AliasedStruct<T>::AliasedStruct(const AliasedStruct& that)
    : AliasedStruct(that.isolate_, *that) {}

template <typename T>
AliasedStruct<T>& AliasedStruct<T>::operator=(
    AliasedStruct<T>&& that) noexcept {
  this->~AliasedStruct();
  isolate_ = that.isolate_;
  store_ = that.store_;
  ptr_ = that.ptr_;

  buffer_ = std::move(that.buffer_);

  that.ptr_ = nullptr;
  that.store_.reset();
  return *this;
}

template <typename T>
AliasedStruct<T>::~AliasedStruct() {
  if (ptr_ != nullptr) ptr_->~T();
}

// ---------------------------------------------------------------------------
// AliasedStructArena implementation
// ---------------------------------------------------------------------------

template <typename T, size_t kPageBytes>
typename AliasedStructArena<T, kPageBytes>::Page*
AliasedStructArena<T, kPageBytes>::FindOrCreatePage(v8::Isolate* isolate) {
  for (auto& p : pages_) {
    if (p->HasFreeSlots()) return p.get();
  }
  auto p = std::make_unique<Page>();
  p->Init(isolate);
  Page* raw = p.get();
  pages_.push_back(std::move(p));
  return raw;
}

template <typename T, size_t kPageBytes>
template <typename... Args>
typename AliasedStructArena<T, kPageBytes>::Slot
AliasedStructArena<T, kPageBytes>::Allocate(v8::Isolate* isolate,
                                            Args&&... args) {
  Page* page = FindOrCreatePage(isolate);
  DCHECK(page->HasFreeSlots());

  uint32_t idx = page->free_head;
  T* raw = &page->base[idx];

  // Advance freelist before placement new overwrites the linkage.
  page->free_head = *reinterpret_cast<uint32_t*>(raw);
  page->used_count++;

  // Placement-construct T in the slot.
  T* ptr = new (raw) T(std::forward<Args>(args)...);

  Slot slot;
  slot.page = static_cast<void*>(page);
  slot.ptr = static_cast<void*>(ptr);
  slot.index = idx;
  slot.byte_offset = reinterpret_cast<const uint8_t*>(ptr) -
                     static_cast<const uint8_t*>(page->store->Data());
  return slot;
}

template <typename T, size_t kPageBytes>
void AliasedStructArena<T, kPageBytes>::Release(
    typename AliasedStructArena<T, kPageBytes>::Slot&& slot) {
  if (!slot) return;
  auto* page = static_cast<Page*>(slot.page);
  auto* ptr = static_cast<T*>(slot.ptr);
  uint32_t idx = slot.index;

  // Destruct and zero so JS views see clean data.
  ptr->~T();
  memset(ptr, 0, sizeof(T));

  // Push onto page freelist.
  *reinterpret_cast<uint32_t*>(ptr) = page->free_head;
  page->free_head = idx;
  page->used_count--;

  slot.page = nullptr;
  slot.ptr = nullptr;

  // Drop empty pages. The shared_ptr<BackingStore> ensures the
  // underlying memory stays alive until V8 GCs any remaining JS
  // references to the page's ArrayBuffer/views.
  if (page->used_count == 0) {
    for (auto it = pages_.begin(); it != pages_.end(); ++it) {
      if (it->get() == page) {
        pages_.erase(it);
        break;
      }
    }
  }
}

template <typename T, size_t kPageBytes>
void AliasedStructArena<T, kPageBytes>::ReleaseSlot(ArenaSlotBase& base) {
  Slot slot;
  slot.page = base.page;
  slot.ptr = base.ptr;
  slot.index = base.index;
  slot.byte_offset = base.byte_offset;
  Release(std::move(slot));
  base.page = nullptr;
  base.ptr = nullptr;
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_ALIASED_STRUCT_INL_H_
