#ifndef SRC_ALIASED_STRUCT_H_
#define SRC_ALIASED_STRUCT_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <memory>
#include <vector>
#include "node_internals.h"
#include "v8.h"

namespace node {

// AliasedStruct is a utility that allows uses a V8 Backing Store
// to be exposed to the C++/C side as a struct and to the
// JavaScript side as an ArrayBuffer to efficiently share
// data without marshalling. It is similar in nature to
// AliasedBuffer.
//
//   struct Foo { int x; }
//
//   AliasedStruct<Foo> foo;
//   foo->x = 1;
//
//   Local<ArrayBuffer> ab = foo.GetArrayBuffer();
template <typename T>
class AliasedStruct final {
 public:
  template <typename... Args>
  explicit AliasedStruct(v8::Isolate* isolate, Args&&... args);

  inline AliasedStruct(const AliasedStruct& that);

  inline ~AliasedStruct();

  inline AliasedStruct& operator=(AliasedStruct&& that) noexcept;

  v8::Local<v8::ArrayBuffer> GetArrayBuffer() const {
    return buffer_.Get(isolate_);
  }

  const T* Data() const { return ptr_; }

  T* Data() { return ptr_; }

  const T& operator*() const { return *ptr_; }

  T& operator*()  { return *ptr_; }

  const T* operator->() const { return ptr_; }

  T* operator->() { return ptr_; }

 private:
  v8::Isolate* isolate_;
  std::shared_ptr<v8::BackingStore> store_;
  T* ptr_;
  v8::Global<v8::ArrayBuffer> buffer_;
};

// ---------------------------------------------------------------------------
// ArenaSlot — type-erased handle to a slot in an AliasedStructArena page.
// This can be stored in headers where T is incomplete. The typed accessors
// are provided via a thin typed wrapper (AliasedStructArena<T>::Slot).
struct ArenaSlotBase {
  // Opaque page pointer — only the arena knows the concrete type.
  void* page = nullptr;
  void* ptr = nullptr;
  uint32_t index = 0;
  size_t byte_offset = 0;

  explicit operator bool() const { return ptr != nullptr; }

  // Returns the page's ArrayBuffer. Implemented below after ArenaPageHeader.
  v8::Local<v8::ArrayBuffer> GetArrayBuffer(v8::Isolate* isolate) const;

  size_t GetByteOffset() const { return byte_offset; }

  // Returns the page's cached DataView over the full page.
  // Callers use byte_offset to index into the correct slot region.
  v8::Local<v8::DataView> GetPageDataView(v8::Isolate* isolate) const;

  // Returns the page's cached BigUint64Array over the full page.
  // Callers use byte_offset / sizeof(uint64_t) to index into the
  // correct slot region.
  v8::Local<v8::BigUint64Array> GetPageBigUint64Array(
      v8::Isolate* isolate) const;
};

// ---------------------------------------------------------------------------
// AliasedStructArena<T> — pool allocator for AliasedStruct-style shared
// memory.  Instead of creating a separate ArrayBuffer + BackingStore per
// instance, the arena pre-allocates pages of N slots backed by a single
// ArrayBuffer each.  Callers receive a Slot handle that provides the same
// T*/operator-> interface as AliasedStruct, plus the ability to create a
// JS typed-array view over just that slot's region of the page buffer.
//
// Pages target kPageBytes (default 16 KB) for L1 cache residency during
// sequential access patterns.  Slots are recycled via an intrusive
// freelist, and empty pages are dropped when their last slot is released.
//
// Usage:
//   AliasedStructArena<MyState> arena;
//   auto slot = arena.Allocate(isolate);
//   slot->some_field = 42;
//   auto view = slot.GetArrayBuffer(isolate);  // JS-visible view
//   ...
//   arena.Release(std::move(slot));             // return to freelist
//
template <typename T, size_t kPageBytes = 16384>
class AliasedStructArena final {
 public:
  static constexpr size_t kSlotsPerPage = kPageBytes / sizeof(T);
  static_assert(kSlotsPerPage >= 4, "Page too small for type T");
  static_assert(sizeof(T) >= sizeof(uint32_t),
                "T must be at least 4 bytes for freelist linkage");

  AliasedStructArena() = default;
  ~AliasedStructArena() = default;

  AliasedStructArena(const AliasedStructArena&) = delete;
  AliasedStructArena& operator=(const AliasedStructArena&) = delete;

  struct Page {
    std::shared_ptr<v8::BackingStore> store;
    v8::Global<v8::ArrayBuffer> buffer;
    // Lazily created full-page views shared by all slots in
    // this page. Typically only one is used per arena.
    v8::Global<v8::DataView> data_view;
    v8::Global<v8::BigUint64Array> big_uint64_array;
    size_t page_byte_length = 0;
    T* base = nullptr;
    uint32_t free_head = 0;
    uint32_t used_count = 0;
    static constexpr uint32_t kNoFreeSlot = UINT32_MAX;

    void Init(v8::Isolate* isolate) {
      const v8::HandleScope handle_scope(isolate);
      const size_t total_bytes = kSlotsPerPage * sizeof(T);
      store = v8::ArrayBuffer::NewBackingStore(isolate, total_bytes);
      memset(store->Data(), 0, total_bytes);
      base = static_cast<T*>(store->Data());
      page_byte_length = total_bytes;
      v8::Local<v8::ArrayBuffer> ab = v8::ArrayBuffer::New(isolate, store);
      buffer = v8::Global<v8::ArrayBuffer>(isolate, ab);

      // Build freelist: each slot points to the next.
      for (uint32_t i = 0; i < kSlotsPerPage - 1; i++) {
        *reinterpret_cast<uint32_t*>(&base[i]) = i + 1;
      }
      *reinterpret_cast<uint32_t*>(&base[kSlotsPerPage - 1]) = kNoFreeSlot;
      free_head = 0;
      used_count = 0;
    }

    bool HasFreeSlots() const { return free_head != kNoFreeSlot; }
  };

  // Typed slot handle — wraps ArenaSlotBase with T* accessors.
  class Slot : public ArenaSlotBase {
   public:
    Slot() = default;

    const T& operator*() const { return *static_cast<T*>(ptr); }
    T& operator*() { return *static_cast<T*>(ptr); }
    const T* operator->() const { return static_cast<T*>(ptr); }
    T* operator->() { return static_cast<T*>(ptr); }
    T* Data() { return static_cast<T*>(ptr); }
    const T* Data() const { return static_cast<T*>(ptr); }
  };

  // Allocate a slot, placement-constructing T with the given args.
  // Creates a new page if all existing pages are full.
  template <typename... Args>
  Slot Allocate(v8::Isolate* isolate, Args&&... args);

  // Release a slot back to the arena freelist.  Calls ~T() and zeros
  // the memory so that any JS views see clean data.
  void Release(Slot&& slot);

  // Release a slot given a type-erased ArenaSlotBase reference.
  // Convenience for callers that store ArenaSlotBase in headers where
  // T is incomplete.
  void ReleaseSlot(ArenaSlotBase& base);

 private:
  Page* FindOrCreatePage(v8::Isolate* isolate);

  std::vector<std::unique_ptr<Page>> pages_;
};

// ArenaSlotBase accessors need to reach the v8::Globals inside a Page.
// All AliasedStructArena<T>::Page types share the same leading layout.
// The page_byte_length field allows lazy view creation without knowing T.
namespace detail {
struct ArenaPageHeader {
  std::shared_ptr<v8::BackingStore> store;
  v8::Global<v8::ArrayBuffer> buffer;
  v8::Global<v8::DataView> data_view;
  v8::Global<v8::BigUint64Array> big_uint64_array;
  size_t page_byte_length = 0;

  v8::Local<v8::DataView> GetDataView(v8::Isolate* isolate) {
    if (data_view.IsEmpty()) {
      const v8::HandleScope handle_scope(isolate);
      auto dv = v8::DataView::New(buffer.Get(isolate), 0, page_byte_length);
      data_view = v8::Global<v8::DataView>(isolate, dv);
    }
    return data_view.Get(isolate);
  }

  v8::Local<v8::BigUint64Array> GetBigUint64Array(v8::Isolate* isolate) {
    if (big_uint64_array.IsEmpty()) {
      const v8::HandleScope handle_scope(isolate);
      auto bu = v8::BigUint64Array::New(
          buffer.Get(isolate), 0, page_byte_length / sizeof(uint64_t));
      big_uint64_array = v8::Global<v8::BigUint64Array>(isolate, bu);
    }
    return big_uint64_array.Get(isolate);
  }
};
}  // namespace detail

inline v8::Local<v8::ArrayBuffer> ArenaSlotBase::GetArrayBuffer(
    v8::Isolate* isolate) const {
  DCHECK_NOT_NULL(page);
  auto* header = static_cast<detail::ArenaPageHeader*>(page);
  return header->buffer.Get(isolate);
}

inline v8::Local<v8::DataView> ArenaSlotBase::GetPageDataView(
    v8::Isolate* isolate) const {
  DCHECK_NOT_NULL(page);
  auto* header = static_cast<detail::ArenaPageHeader*>(page);
  return header->GetDataView(isolate);
}

inline v8::Local<v8::BigUint64Array> ArenaSlotBase::GetPageBigUint64Array(
    v8::Isolate* isolate) const {
  DCHECK_NOT_NULL(page);
  auto* header = static_cast<detail::ArenaPageHeader*>(page);
  return header->GetBigUint64Array(isolate);
}

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_ALIASED_STRUCT_H_
