// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/v8windbg/src/js-stack.h"

HRESULT GetJSStackFrames(WRL::ComPtr<IModelObject>& sp_result) {
  sp_result = nullptr;

  // Get the current context
  WRL::ComPtr<IDebugHostContext> sp_host_context;
  RETURN_IF_FAIL(sp_debug_host->GetCurrentContext(&sp_host_context));

  WRL::ComPtr<IModelObject> sp_curr_thread;
  RETURN_IF_FAIL(GetCurrentThread(sp_host_context, &sp_curr_thread));

  WRL::ComPtr<IModelObject> sp_stack;
  RETURN_IF_FAIL(sp_curr_thread->GetKeyValue(L"Stack", &sp_stack, nullptr));

  RETURN_IF_FAIL(sp_stack->GetKeyValue(L"Frames", &sp_result, nullptr));

  return S_OK;
}

// v8windbg!JSStackAlias::Call
IFACEMETHODIMP JSStackAlias::Call(IModelObject* p_context_object,
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

  auto sp_iterator{WRL::Make<StackFrames>()};

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

FrameData::FrameData() = default;
FrameData::~FrameData() = default;
FrameData::FrameData(const FrameData&) = default;
FrameData::FrameData(FrameData&&) = default;
FrameData& FrameData::operator=(const FrameData&) = default;
FrameData& FrameData::operator=(FrameData&&) = default;

StackFrameIterator::StackFrameIterator(
    WRL::ComPtr<IDebugHostContext>& host_context)
    : sp_ctx_(host_context) {}
StackFrameIterator::~StackFrameIterator() = default;

HRESULT StackFrameIterator::PopulateFrameData() {
  frames_.clear();
  WRL::ComPtr<IModelObject> sp_frames;

  RETURN_IF_FAIL(GetJSStackFrames(sp_frames));

  // Iterate over the array of frames.
  WRL::ComPtr<IIterableConcept> sp_iterable;
  RETURN_IF_FAIL(
      sp_frames->GetConcept(__uuidof(IIterableConcept), &sp_iterable, nullptr));

  WRL::ComPtr<IModelIterator> sp_frame_iterator;
  RETURN_IF_FAIL(sp_iterable->GetIterator(sp_frames.Get(), &sp_frame_iterator));

  // Loop through all the frames in the array.
  WRL::ComPtr<IModelObject> sp_frame;
  while (sp_frame_iterator->GetNext(&sp_frame, 0, nullptr, nullptr) !=
         E_BOUNDS) {
    // Skip non-JS frame (frame that doesn't have a function_name).
    WRL::ComPtr<IModelObject> sp_local_variables;
    HRESULT hr =
        sp_frame->GetKeyValue(L"LocalVariables", &sp_local_variables, nullptr);
    if (FAILED(hr)) continue;

    WRL::ComPtr<IModelObject> sp_currently_executing_jsfunction;
    hr = sp_local_variables->GetKeyValue(L"currently_executing_jsfunction",
                                         &sp_currently_executing_jsfunction,
                                         nullptr);
    if (FAILED(hr)) continue;

    // At this point, it is safe to add frame entry even though some fields
    // might not be available.
    WRL::ComPtr<IModelObject> sp_function_name, sp_script_name,
        sp_script_source, sp_function_character_offset;
    FrameData frame_entry;
    hr = sp_local_variables->GetKeyValue(L"script_name", &sp_script_name,
                                         nullptr);
    if (SUCCEEDED(hr)) {
      frame_entry.script_name = sp_script_name;
    }
    hr = sp_local_variables->GetKeyValue(L"script_source", &sp_script_source,
                                         nullptr);
    if (SUCCEEDED(hr)) {
      frame_entry.script_source = sp_script_source;
    }
    hr = sp_local_variables->GetKeyValue(L"function_name", &sp_function_name,
                                         nullptr);
    if (SUCCEEDED(hr)) {
      frame_entry.function_name = sp_function_name;
    }
    hr = sp_local_variables->GetKeyValue(
        L"function_character_offset", &sp_function_character_offset, nullptr);
    if (SUCCEEDED(hr)) {
      frame_entry.function_character_offset = sp_function_character_offset;
    }

    frames_.push_back(frame_entry);
  }

  return S_OK;
}

IFACEMETHODIMP StackFrameIterator::Reset() noexcept {
  position_ = 0;
  return S_OK;
}

IFACEMETHODIMP StackFrameIterator::GetNext(IModelObject** object,
                                           ULONG64 dimensions,
                                           IModelObject** indexers,
                                           IKeyStore** metadata) noexcept {
  if (dimensions > 1) return E_INVALIDARG;

  if (position_ == 0) {
    RETURN_IF_FAIL(PopulateFrameData());
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

HRESULT StackFrameIterator::GetAt(uint64_t index, IModelObject** result) const {
  if (index >= frames_.size()) return E_BOUNDS;

  // Create the synthetic object representing the frame here.
  const FrameData& curr_frame = frames_.at(index);
  WRL::ComPtr<IModelObject> sp_value;
  RETURN_IF_FAIL(
      sp_data_model_manager->CreateSyntheticObject(sp_ctx_.Get(), &sp_value));
  RETURN_IF_FAIL(
      sp_value->SetKey(L"script_name", curr_frame.script_name.Get(),
      nullptr));
  RETURN_IF_FAIL(sp_value->SetKey(L"script_source",
                                  curr_frame.script_source.Get(), nullptr));
  RETURN_IF_FAIL(sp_value->SetKey(L"function_name",
                                  curr_frame.function_name.Get(), nullptr));
  RETURN_IF_FAIL(sp_value->SetKey(L"function_character_offset",
                                  curr_frame.function_character_offset.Get(),
                                  nullptr));

  *result = sp_value.Detach();
  return S_OK;
}

StackFrames::StackFrames() = default;
StackFrames::~StackFrames() = default;

IFACEMETHODIMP StackFrames::GetDimensionality(
    IModelObject* context_object, ULONG64* dimensionality) noexcept {
  *dimensionality = 1;
  return S_OK;
}

IFACEMETHODIMP StackFrames::GetAt(IModelObject* context_object,
                                  ULONG64 indexer_count,
                                  IModelObject** indexers,
                                  IModelObject** object,
                                  IKeyStore** metadata) noexcept {
  if (indexer_count != 1) return E_INVALIDARG;
  if (metadata != nullptr) *metadata = nullptr;
  WRL::ComPtr<IDebugHostContext> sp_ctx;
  RETURN_IF_FAIL(context_object->GetContext(&sp_ctx));

  // This should be instantiated once for each synthetic object returned,
  // so should be able to cache/reuse an iterator.
  if (opt_frames_ == nullptr) {
    opt_frames_ = WRL::Make<StackFrameIterator>(sp_ctx);
    _ASSERT(opt_frames_ != nullptr);
    RETURN_IF_FAIL(opt_frames_->PopulateFrameData());
  }

  uint64_t index;
  RETURN_IF_FAIL(UnboxULong64(indexers[0], &index, true /*convert*/));

  return opt_frames_->GetAt(index, object);
}

IFACEMETHODIMP StackFrames::SetAt(IModelObject* context_object,
                                  ULONG64 indexer_count,
                                  IModelObject** indexers,
                                  IModelObject* value) noexcept {
  return E_NOTIMPL;
}

IFACEMETHODIMP StackFrames::GetDefaultIndexDimensionality(
    IModelObject* context_object, ULONG64* dimensionality) noexcept {
  *dimensionality = 1;
  return S_OK;
}

IFACEMETHODIMP StackFrames::GetIterator(IModelObject* context_object,
                                        IModelIterator** iterator) noexcept {
  WRL::ComPtr<IDebugHostContext> sp_ctx;
  RETURN_IF_FAIL(context_object->GetContext(&sp_ctx));
  auto sp_memory_iterator{WRL::Make<StackFrameIterator>(sp_ctx)};
  *iterator = sp_memory_iterator.Detach();
  return S_OK;
}
