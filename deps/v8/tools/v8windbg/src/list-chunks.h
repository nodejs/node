// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TOOLS_V8WINDBG_SRC_LIST_CHUNKS_H_
#define V8_TOOLS_V8WINDBG_SRC_LIST_CHUNKS_H_

#include <crtdbg.h>
#include <wrl/implements.h>

#include <optional>
#include <string>
#include <vector>

#include "src/base/optional.h"
#include "tools/v8windbg/base/utilities.h"
#include "tools/v8windbg/src/v8-debug-helper-interop.h"
#include "tools/v8windbg/src/v8windbg-extension.h"

class ListChunksAlias
    : public WRL::RuntimeClass<
          WRL::RuntimeClassFlags<WRL::RuntimeClassType::ClassicCom>,
          IModelMethod> {
 public:
  IFACEMETHOD(Call)
  (IModelObject* p_context_object, ULONG64 arg_count,
   _In_reads_(arg_count) IModelObject** pp_arguments, IModelObject** pp_result,
   IKeyStore** pp_metadata);
};

struct ChunkData {
  ChunkData();
  ~ChunkData();
  ChunkData(const ChunkData&);
  ChunkData(ChunkData&&);
  ChunkData& operator=(const ChunkData&);
  ChunkData& operator=(ChunkData&&);
  WRL::ComPtr<IModelObject> area_start;
  WRL::ComPtr<IModelObject> area_end;
  WRL::ComPtr<IModelObject> space;
};

class MemoryChunkIterator
    : public WRL::RuntimeClass<
          WRL::RuntimeClassFlags<WRL::RuntimeClassType::ClassicCom>,
          IModelIterator> {
 public:
  MemoryChunkIterator(WRL::ComPtr<IDebugHostContext>& host_context);
  ~MemoryChunkIterator() override;

  HRESULT PopulateChunkData();

  IFACEMETHOD(Reset)();

  IFACEMETHOD(GetNext)
  (IModelObject** object, ULONG64 dimensions, IModelObject** indexers,
   IKeyStore** metadata);

  const std::vector<ChunkData>& GetChunks() const { return chunks_; }

  HRESULT GetAt(uint64_t index, IModelObject** result) const;

 private:
  ULONG position_ = 0;
  std::vector<ChunkData> chunks_;
  WRL::ComPtr<IDebugHostContext> sp_ctx_;
};

class MemoryChunks
    : public WRL::RuntimeClass<
          WRL::RuntimeClassFlags<WRL::RuntimeClassType::ClassicCom>,
          IIndexableConcept, IIterableConcept> {
 public:
  MemoryChunks();
  ~MemoryChunks() override;

  // IIndexableConcept members
  IFACEMETHOD(GetDimensionality)
  (IModelObject* context_object, ULONG64* dimensionality);

  IFACEMETHOD(GetAt)
  (IModelObject* context_object, ULONG64 indexer_count, IModelObject** indexers,
   IModelObject** object, IKeyStore** metadata);

  IFACEMETHOD(SetAt)
  (IModelObject* context_object, ULONG64 indexer_count, IModelObject** indexers,
   IModelObject* value);

  // IIterableConcept
  IFACEMETHOD(GetDefaultIndexDimensionality)
  (IModelObject* context_object, ULONG64* dimensionality);

  IFACEMETHOD(GetIterator)
  (IModelObject* context_object, IModelIterator** iterator);

 private:
  WRL::ComPtr<MemoryChunkIterator> opt_chunks_;
};

#endif  // V8_TOOLS_V8WINDBG_SRC_LIST_CHUNKS_H_
