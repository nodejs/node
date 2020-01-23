// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_BACKING_STORE_H_
#define V8_OBJECTS_BACKING_STORE_H_

#include <memory>

#include "include/v8-internal.h"
#include "include/v8.h"
#include "src/handles/handles.h"

namespace v8 {
namespace internal {

class Isolate;
class WasmMemoryObject;

// Whether the backing store is shared or not.
enum class SharedFlag : uint8_t { kNotShared, kShared };

// Whether the backing store memory is initialied to zero or not.
enum class InitializedFlag : uint8_t { kUninitialized, kZeroInitialized };

// Internal information for shared wasm memories. E.g. contains
// a list of all memory objects (across all isolates) that share this
// backing store.
struct SharedWasmMemoryData;

// The {BackingStore} data structure stores all the low-level details about the
// backing store of an array buffer or Wasm memory, including its base address
// and length, whether it is shared, provided by the embedder, has guard
// regions, etc. Instances of this classes *own* the underlying memory
// when they are created through one of the {Allocate()} methods below,
// and the destructor frees the memory (and page allocation if necessary).
// Backing stores can also *wrap* embedder-allocated memory. In this case,
// they do not own the memory, and upon destruction, they do not deallocate it.
class V8_EXPORT_PRIVATE BackingStore : public BackingStoreBase {
 public:
  ~BackingStore();

  // Allocate an array buffer backing store using the default method,
  // which currently is the embedder-provided array buffer allocator.
  static std::unique_ptr<BackingStore> Allocate(Isolate* isolate,
                                                size_t byte_length,
                                                SharedFlag shared,
                                                InitializedFlag initialized);

  // Allocate the backing store for a Wasm memory.
  static std::unique_ptr<BackingStore> AllocateWasmMemory(Isolate* isolate,
                                                          size_t initial_pages,
                                                          size_t maximum_pages,
                                                          SharedFlag shared);

  // Create a backing store that wraps existing allocated memory.
  // If {free_on_destruct} is {true}, the memory will be freed using the
  // ArrayBufferAllocator::Free() callback when this backing store is
  // destructed. Otherwise destructing the backing store will do nothing
  // to the allocated memory.
  static std::unique_ptr<BackingStore> WrapAllocation(Isolate* isolate,
                                                      void* allocation_base,
                                                      size_t allocation_length,
                                                      SharedFlag shared,
                                                      bool free_on_destruct);

  static std::unique_ptr<BackingStore> WrapAllocation(
      void* allocation_base, size_t allocation_length,
      v8::BackingStoreDeleterCallback deleter, void* deleter_data,
      SharedFlag shared);

  // Create an empty backing store.
  static std::unique_ptr<BackingStore> EmptyBackingStore(SharedFlag shared);

  // Accessors.
  void* buffer_start() const { return buffer_start_; }
  size_t byte_length() const {
    return byte_length_.load(std::memory_order_relaxed);
  }
  size_t byte_capacity() const { return byte_capacity_; }
  bool is_shared() const { return is_shared_; }
  bool is_wasm_memory() const { return is_wasm_memory_; }
  bool has_guard_regions() const { return has_guard_regions_; }
  bool free_on_destruct() const { return free_on_destruct_; }

  // Attempt to grow this backing store in place.
  bool GrowWasmMemoryInPlace(Isolate* isolate, size_t delta_pages,
                             size_t max_pages);

  // Allocate a new, larger, backing store for this Wasm memory and copy the
  // contents of this backing store into it.
  std::unique_ptr<BackingStore> CopyWasmMemory(Isolate* isolate,
                                               size_t new_pages);

  // Attach the given memory object to this backing store. The memory object
  // will be updated if this backing store is grown.
  void AttachSharedWasmMemoryObject(Isolate* isolate,
                                    Handle<WasmMemoryObject> memory_object);

  // Send asynchronous updates to attached memory objects in other isolates
  // after the backing store has been grown. Memory objects in this
  // isolate are updated synchronously.
  static void BroadcastSharedWasmMemoryGrow(Isolate* isolate,
                                            std::shared_ptr<BackingStore>,
                                            size_t new_pages);

  // TODO(wasm): address space limitations should be enforced in page alloc.
  // These methods enforce a limit on the total amount of address space,
  // which is used for both backing stores and wasm memory.
  static bool ReserveAddressSpace(uint64_t num_bytes);
  static void ReleaseReservation(uint64_t num_bytes);

  // Remove all memory objects in the given isolate that refer to this
  // backing store.
  static void RemoveSharedWasmMemoryObjects(Isolate* isolate);

  // Update all shared memory objects in this isolate (after a grow operation).
  static void UpdateSharedWasmMemoryObjects(Isolate* isolate);

 private:
  friend class GlobalBackingStoreRegistry;

  BackingStore(void* buffer_start, size_t byte_length, size_t byte_capacity,
               SharedFlag shared, bool is_wasm_memory, bool free_on_destruct,
               bool has_guard_regions, bool custom_deleter)
      : buffer_start_(buffer_start),
        byte_length_(byte_length),
        byte_capacity_(byte_capacity),
        is_shared_(shared == SharedFlag::kShared),
        is_wasm_memory_(is_wasm_memory),
        holds_shared_ptr_to_allocator_(false),
        free_on_destruct_(free_on_destruct),
        has_guard_regions_(has_guard_regions),
        globally_registered_(false),
        custom_deleter_(custom_deleter) {}
  void SetAllocatorFromIsolate(Isolate* isolate);

  void* buffer_start_ = nullptr;
  std::atomic<size_t> byte_length_{0};
  size_t byte_capacity_ = 0;

  struct DeleterInfo {
    v8::BackingStoreDeleterCallback callback;
    void* data;
  };

  union TypeSpecificData {
    TypeSpecificData() : v8_api_array_buffer_allocator(nullptr) {}
    ~TypeSpecificData() {}

    // If this backing store was allocated through the ArrayBufferAllocator API,
    // this is a direct pointer to the API object for freeing the backing
    // store.
    v8::ArrayBuffer::Allocator* v8_api_array_buffer_allocator;

    // Holds a shared_ptr to the ArrayBuffer::Allocator instance, if requested
    // so by the embedder through setting
    // Isolate::CreateParams::array_buffer_allocator_shared.
    std::shared_ptr<v8::ArrayBuffer::Allocator>
        v8_api_array_buffer_allocator_shared;

    // For shared Wasm memories, this is a list of all the attached memory
    // objects, which is needed to grow shared backing stores.
    SharedWasmMemoryData* shared_wasm_memory_data;

    // Custom deleter for the backing stores that wrap memory blocks that are
    // allocated with a custom allocator.
    DeleterInfo deleter;
  } type_specific_data_;

  bool is_shared_ : 1;
  bool is_wasm_memory_ : 1;
  bool holds_shared_ptr_to_allocator_ : 1;
  bool free_on_destruct_ : 1;
  bool has_guard_regions_ : 1;
  bool globally_registered_ : 1;
  bool custom_deleter_ : 1;

  // Accessors for type-specific data.
  v8::ArrayBuffer::Allocator* get_v8_api_array_buffer_allocator();
  SharedWasmMemoryData* get_shared_wasm_memory_data();

  void Clear();  // Internally clears fields after deallocation.
  static std::unique_ptr<BackingStore> TryAllocateWasmMemory(
      Isolate* isolate, size_t initial_pages, size_t maximum_pages,
      SharedFlag shared);

  DISALLOW_COPY_AND_ASSIGN(BackingStore);
};

// A global, per-process mapping from buffer addresses to backing stores.
// This is generally only used for dealing with an embedder that has not
// migrated to the new API which should use proper pointers to manage
// backing stores.
class GlobalBackingStoreRegistry {
 public:
  // Register a backing store in the global registry. A mapping from the
  // {buffer_start} to the backing store object will be added. The backing
  // store will automatically unregister itself upon destruction.
  static void Register(std::shared_ptr<BackingStore> backing_store);

  // Look up a backing store based on the {buffer_start} pointer.
  static std::shared_ptr<BackingStore> Lookup(void* buffer_start,
                                              size_t length);

 private:
  friend class BackingStore;
  // Unregister a backing store in the global registry.
  static void Unregister(BackingStore* backing_store);

  // Adds the given memory object to the backing store's weak list
  // of memory objects (under the registry lock).
  static void AddSharedWasmMemoryObject(Isolate* isolate,
                                        BackingStore* backing_store,
                                        Handle<WasmMemoryObject> memory_object);

  // Purge any shared wasm memory lists that refer to this isolate.
  static void Purge(Isolate* isolate);

  // Broadcast updates to all attached memory objects.
  static void BroadcastSharedWasmMemoryGrow(
      Isolate* isolate, std::shared_ptr<BackingStore> backing_store,
      size_t new_pages);

  // Update all shared memory objects in the given isolate.
  static void UpdateSharedWasmMemoryObjects(Isolate* isolate);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_OBJECTS_BACKING_STORE_H_
