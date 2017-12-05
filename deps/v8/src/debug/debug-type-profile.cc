// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/debug-type-profile.h"

#include "src/feedback-vector.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

std::unique_ptr<TypeProfile> TypeProfile::Collect(Isolate* isolate) {
  std::unique_ptr<TypeProfile> result(new TypeProfile());

  // Collect existing feedback vectors.
  std::vector<Handle<FeedbackVector>> feedback_vectors;
  {
    HeapIterator heap_iterator(isolate->heap());
    while (HeapObject* current_obj = heap_iterator.next()) {
      if (current_obj->IsFeedbackVector()) {
        FeedbackVector* vector = FeedbackVector::cast(current_obj);
        SharedFunctionInfo* shared = vector->shared_function_info();
        if (!shared->IsSubjectToDebugging()) continue;
        feedback_vectors.emplace_back(vector, isolate);
      }
    }
  }

  Script::Iterator scripts(isolate);

  while (Script* script = scripts.Next()) {
    if (!script->IsUserJavaScript()) {
      continue;
    }

    Handle<Script> script_handle(script, isolate);

    TypeProfileScript type_profile_script(script_handle);
    std::vector<TypeProfileEntry>* entries = &type_profile_script.entries;

    for (const auto& vector : feedback_vectors) {
      SharedFunctionInfo* info = vector->shared_function_info();
      DCHECK(info->IsSubjectToDebugging());

      // Match vectors with script.
      if (script != info->script()) {
        continue;
      }
      if (info->feedback_metadata()->is_empty() ||
          !info->feedback_metadata()->HasTypeProfileSlot()) {
        continue;
      }
      FeedbackSlot slot = vector->GetTypeProfileSlot();
      CollectTypeProfileNexus nexus(vector, slot);
      Handle<String> name(info->DebugName(), isolate);
      std::vector<int> source_positions = nexus.GetSourcePositions();
      for (int position : source_positions) {
        DCHECK_GE(position, 0);
        entries->emplace_back(position, nexus.GetTypesForSourcePositions(
                                            static_cast<uint32_t>(position)));
      }

      // Releases type profile data collected so far.
      nexus.Clear();
    }
    if (!entries->empty()) {
      result->emplace_back(type_profile_script);
    }
  }
  return result;
}

void TypeProfile::SelectMode(Isolate* isolate, debug::TypeProfile::Mode mode) {
  isolate->set_type_profile_mode(mode);
  HandleScope handle_scope(isolate);

  if (mode == debug::TypeProfile::Mode::kNone) {
    // Release type profile data collected so far.
    {
      HeapIterator heap_iterator(isolate->heap());
      while (HeapObject* current_obj = heap_iterator.next()) {
        if (current_obj->IsFeedbackVector()) {
          FeedbackVector* vector = FeedbackVector::cast(current_obj);
          SharedFunctionInfo* info = vector->shared_function_info();
          if (!info->IsSubjectToDebugging() ||
              info->feedback_metadata()->is_empty() ||
              !info->feedback_metadata()->HasTypeProfileSlot())
            continue;
          FeedbackSlot slot = vector->GetTypeProfileSlot();
          CollectTypeProfileNexus nexus(vector, slot);
          nexus.Clear();
        }
      }
    }
  }
}

}  // namespace internal
}  // namespace v8
