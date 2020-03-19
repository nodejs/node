// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/v8windbg/src/list-chunks.h"

#include "tools/v8windbg/src/cur-isolate.h"

// v8windbg!ListChunksAlias::Call
IFACEMETHODIMP ListChunksAlias::Call(IModelObject* p_context_object,
                                     ULONG64 arg_count,
                                     _In_reads_(arg_count)
                                         IModelObject** pp_arguments,
                                     IModelObject** pp_result,
                                     IKeyStore** pp_metadata) noexcept {
  WRL::ComPtr<IDebugHostContext> sp_ctx;
  RETURN_IF_FAIL(sp_debug_host->GetCurrentContext(&sp_ctx));

  WRL::ComPtr<IModelObject> result;
  RETURN_IF_FAIL(
      sp_data_model_manager->CreateSyntheticObject(sp_ctx.Get(), &result));

  auto sp_iterator{WRL::Make<MemoryChunks>()};

  RETURN_IF_FAIL(result->SetConcept(
      __uuidof(IIndexableConcept),
      static_cast<IIndexableConcept*>(sp_iterator.Get()), nullptr));
  RETURN_IF_FAIL(result->SetConcept(
      __uuidof(IIterableConcept),
      static_cast<IIterableConcept*>(sp_iterator.Get()), nullptr));

  *pp_result = result.Detach();
  if (pp_metadata) {
    *pp_metadata = nullptr;
  }
  return S_OK;
}

ChunkData::ChunkData() = default;
ChunkData::~ChunkData() = default;
ChunkData::ChunkData(const ChunkData&) = default;
ChunkData::ChunkData(ChunkData&&) = default;
ChunkData& ChunkData::operator=(const ChunkData&) = default;
ChunkData& ChunkData::operator=(ChunkData&&) = default;

MemoryChunkIterator::MemoryChunkIterator(
    WRL::ComPtr<IDebugHostContext>& host_context)
    : sp_ctx_(host_context) {}
MemoryChunkIterator::~MemoryChunkIterator() = default;

HRESULT MemoryChunkIterator::PopulateChunkData() {
  WRL::ComPtr<IModelObject> sp_isolate, sp_heap, sp_space;
  chunks_.clear();

  RETURN_IF_FAIL(GetCurrentIsolate(sp_isolate));

  RETURN_IF_FAIL(
      sp_isolate->GetRawValue(SymbolField, L"heap_", RawSearchNone, &sp_heap));
  RETURN_IF_FAIL(
      sp_heap->GetRawValue(SymbolField, L"space_", RawSearchNone, &sp_space));

  WRL::ComPtr<IDebugHostType> sp_space_type;
  RETURN_IF_FAIL(sp_space->GetTypeInfo(&sp_space_type));

  // Iterate over the array of Space pointers
  WRL::ComPtr<IIterableConcept> sp_iterable;
  RETURN_IF_FAIL(
      sp_space->GetConcept(__uuidof(IIterableConcept), &sp_iterable, nullptr));

  WRL::ComPtr<IModelIterator> sp_space_iterator;
  RETURN_IF_FAIL(sp_iterable->GetIterator(sp_space.Get(), &sp_space_iterator));

  // Loop through all the spaces in the array
  WRL::ComPtr<IModelObject> sp_space_ptr;
  while (sp_space_iterator->GetNext(&sp_space_ptr, 0, nullptr, nullptr) !=
         E_BOUNDS) {
    // Should have gotten a "v8::internal::Space *". Dereference, then get field
    // "memory_chunk_list_" [Type: v8::base::List<v8::internal::MemoryChunk>]
    WRL::ComPtr<IModelObject> sp_space, sp_chunk_list, sp_mem_chunk_ptr,
        sp_mem_chunk;
    RETURN_IF_FAIL(sp_space_ptr->Dereference(&sp_space));
    RETURN_IF_FAIL(sp_space->GetRawValue(SymbolField, L"memory_chunk_list_",
                                         RawSearchNone, &sp_chunk_list));

    // Then get field "front_" [Type: v8::internal::MemoryChunk *]
    RETURN_IF_FAIL(sp_chunk_list->GetRawValue(
        SymbolField, L"front_", RawSearchNone, &sp_mem_chunk_ptr));

    // Loop here on the list of MemoryChunks for the space
    while (true) {
      // See if it is a nullptr (i.e. no chunks in this space)
      uint64_t front_val;
      RETURN_IF_FAIL(
          UnboxULong64(sp_mem_chunk_ptr.Get(), &front_val, true /*convert*/));
      if (front_val == 0) {
        break;
      }

      // Dereference and get fields "area_start_" and "area_end_" (both uint64)
      RETURN_IF_FAIL(sp_mem_chunk_ptr->Dereference(&sp_mem_chunk));

      WRL::ComPtr<IModelObject> sp_start, sp_end;
      RETURN_IF_FAIL(sp_mem_chunk->GetRawValue(SymbolField, L"area_start_",
                                               RawSearchNone, &sp_start));
      RETURN_IF_FAIL(sp_mem_chunk->GetRawValue(SymbolField, L"area_end_",
                                               RawSearchNone, &sp_end));

      ChunkData chunk_entry;
      chunk_entry.area_start = sp_start;
      chunk_entry.area_end = sp_end;
      chunk_entry.space = sp_space;
      chunks_.push_back(chunk_entry);

      // Follow the list_node_.next_ to the next memory chunk
      WRL::ComPtr<IModelObject> sp_list_node;
      RETURN_IF_FAIL(sp_mem_chunk->GetRawValue(SymbolField, L"list_node_",
                                               RawSearchNone, &sp_list_node));

      sp_mem_chunk_ptr = nullptr;
      sp_mem_chunk = nullptr;
      RETURN_IF_FAIL(sp_list_node->GetRawValue(
          SymbolField, L"next_", RawSearchNone, &sp_mem_chunk_ptr));
      // Top of the loop will check if this is a nullptr and exit if so
    }
    sp_space_ptr = nullptr;
  }

  return S_OK;
}

IFACEMETHODIMP MemoryChunkIterator::Reset() noexcept {
  position_ = 0;
  return S_OK;
}

IFACEMETHODIMP MemoryChunkIterator::GetNext(IModelObject** object,
                                            ULONG64 dimensions,
                                            IModelObject** indexers,
                                            IKeyStore** metadata) noexcept {
  if (dimensions > 1) return E_INVALIDARG;

  if (position_ == 0) {
    RETURN_IF_FAIL(PopulateChunkData());
  }

  if (metadata != nullptr) *metadata = nullptr;

  WRL::ComPtr<IModelObject> sp_index, sp_value;

  if (dimensions == 1) {
    RETURN_IF_FAIL(CreateULong64(position_, &sp_index));
  }

  RETURN_IF_FAIL(GetAt(position_, &sp_value));

  // Now update counter and transfer ownership of results, because nothing can
  // fail from this point onward.
  ++position_;
  if (dimensions == 1) {
    *indexers = sp_index.Detach();
  }
  *object = sp_value.Detach();
  return S_OK;
}

HRESULT MemoryChunkIterator::GetAt(uint64_t index,
                                   IModelObject** result) const {
  if (index >= chunks_.size()) return E_BOUNDS;

  // Create the synthetic object representing the chunk here
  const ChunkData& curr_chunk = chunks_.at(index);
  WRL::ComPtr<IModelObject> sp_value;
  RETURN_IF_FAIL(
      sp_data_model_manager->CreateSyntheticObject(sp_ctx_.Get(), &sp_value));
  RETURN_IF_FAIL(
      sp_value->SetKey(L"area_start", curr_chunk.area_start.Get(), nullptr));
  RETURN_IF_FAIL(
      sp_value->SetKey(L"area_end", curr_chunk.area_end.Get(), nullptr));
  RETURN_IF_FAIL(sp_value->SetKey(L"space", curr_chunk.space.Get(), nullptr));

  *result = sp_value.Detach();
  return S_OK;
}

MemoryChunks::MemoryChunks() = default;
MemoryChunks::~MemoryChunks() = default;

IFACEMETHODIMP MemoryChunks::GetDimensionality(
    IModelObject* context_object, ULONG64* dimensionality) noexcept {
  *dimensionality = 1;
  return S_OK;
}

IFACEMETHODIMP MemoryChunks::GetAt(IModelObject* context_object,
                                   ULONG64 indexer_count,
                                   IModelObject** indexers,
                                   IModelObject** object,
                                   IKeyStore** metadata) noexcept {
  if (indexer_count != 1) return E_INVALIDARG;
  if (metadata != nullptr) *metadata = nullptr;
  WRL::ComPtr<IDebugHostContext> sp_ctx;
  RETURN_IF_FAIL(context_object->GetContext(&sp_ctx));

  // This should be instantiated once for each synthetic object returned,
  // so should be able to cache/reuse an iterator
  if (opt_chunks_ == nullptr) {
    opt_chunks_ = WRL::Make<MemoryChunkIterator>(sp_ctx);
    _ASSERT(opt_chunks_ != nullptr);
    RETURN_IF_FAIL(opt_chunks_->PopulateChunkData());
  }

  uint64_t index;
  RETURN_IF_FAIL(UnboxULong64(indexers[0], &index, true /*convert*/));

  return opt_chunks_->GetAt(index, object);
}

IFACEMETHODIMP MemoryChunks::SetAt(IModelObject* context_object,
                                   ULONG64 indexer_count,
                                   IModelObject** indexers,
                                   IModelObject* value) noexcept {
  return E_NOTIMPL;
}

IFACEMETHODIMP MemoryChunks::GetDefaultIndexDimensionality(
    IModelObject* context_object, ULONG64* dimensionality) noexcept {
  *dimensionality = 1;
  return S_OK;
}

IFACEMETHODIMP MemoryChunks::GetIterator(IModelObject* context_object,
                                         IModelIterator** iterator) noexcept {
  WRL::ComPtr<IDebugHostContext> sp_ctx;
  RETURN_IF_FAIL(context_object->GetContext(&sp_ctx));
  auto sp_memory_iterator{WRL::Make<MemoryChunkIterator>(sp_ctx)};
  *iterator = sp_memory_iterator.Detach();
  return S_OK;
}
