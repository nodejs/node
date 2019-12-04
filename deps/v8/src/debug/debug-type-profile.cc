// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/debug-type-profile.h"

#include "src/execution/isolate.h"
#include "src/objects/feedback-vector.h"
#include "src/objects/objects-inl.h"
#include "src/objects/objects.h"

namespace v8 {
namespace internal {

std::unique_ptr<TypeProfile> TypeProfile::Collect(Isolate* isolate) {
  std::unique_ptr<TypeProfile> result(new TypeProfile());

  // Feedback vectors are already listed to prevent losing them to GC.
  DCHECK(isolate->factory()
             ->feedback_vectors_for_profiling_tools()
             ->IsArrayList());
  Handle<ArrayList> list = Handle<ArrayList>::cast(
      isolate->factory()->feedback_vectors_for_profiling_tools());

  Script::Iterator scripts(isolate);

  for (Script script = scripts.Next(); !script.is_null();
       script = scripts.Next()) {
    if (!script.IsUserJavaScript()) {
      continue;
    }

    Handle<Script> script_handle(script, isolate);

    TypeProfileScript type_profile_script(script_handle);
    std::vector<TypeProfileEntry>* entries = &type_profile_script.entries;

    // TODO(franzih): Sort the vectors by script first instead of iterating
    // the list multiple times.
    for (int i = 0; i < list->Length(); i++) {
      FeedbackVector vector = FeedbackVector::cast(list->Get(i));
      SharedFunctionInfo info = vector.shared_function_info();
      DCHECK(info.IsSubjectToDebugging());

      // Match vectors with script.
      if (script != info.script()) {
        continue;
      }
      if (!info.HasFeedbackMetadata() || info.feedback_metadata().is_empty() ||
          !info.feedback_metadata().HasTypeProfileSlot()) {
        continue;
      }
      FeedbackSlot slot = vector.GetTypeProfileSlot();
      FeedbackNexus nexus(vector, slot);
      Handle<String> name(info.DebugName(), isolate);
      std::vector<int> source_positions = nexus.GetSourcePositions();
      for (int position : source_positions) {
        DCHECK_GE(position, 0);
        entries->emplace_back(position, nexus.GetTypesForSourcePositions(
                                            static_cast<uint32_t>(position)));
      }

      // Releases type profile data collected so far.
      nexus.ResetTypeProfile();
    }
    if (!entries->empty()) {
      result->emplace_back(type_profile_script);
    }
  }
  return result;
}

void TypeProfile::SelectMode(Isolate* isolate, debug::TypeProfileMode mode) {
  if (mode != isolate->type_profile_mode()) {
    // Changing the type profile mode can change the bytecode that would be
    // generated for a function, which can interfere with lazy source positions,
    // so just force source position collection whenever there's such a change.
    isolate->CollectSourcePositionsForAllBytecodeArrays();
  }

  HandleScope handle_scope(isolate);

  if (mode == debug::TypeProfileMode::kNone) {
    if (!isolate->factory()
             ->feedback_vectors_for_profiling_tools()
             ->IsUndefined(isolate)) {
      // Release type profile data collected so far.

      // Feedback vectors are already listed to prevent losing them to GC.
      DCHECK(isolate->factory()
                 ->feedback_vectors_for_profiling_tools()
                 ->IsArrayList());
      Handle<ArrayList> list = Handle<ArrayList>::cast(
          isolate->factory()->feedback_vectors_for_profiling_tools());

      for (int i = 0; i < list->Length(); i++) {
        FeedbackVector vector = FeedbackVector::cast(list->Get(i));
        SharedFunctionInfo info = vector.shared_function_info();
        DCHECK(info.IsSubjectToDebugging());
        if (info.feedback_metadata().HasTypeProfileSlot()) {
          FeedbackSlot slot = vector.GetTypeProfileSlot();
          FeedbackNexus nexus(vector, slot);
          nexus.ResetTypeProfile();
        }
      }

      // Delete the feedback vectors from the list if they're not used by code
      // coverage.
      if (isolate->is_best_effort_code_coverage()) {
        isolate->SetFeedbackVectorsForProfilingTools(
            ReadOnlyRoots(isolate).undefined_value());
      }
    }
  } else {
    DCHECK_EQ(debug::TypeProfileMode::kCollect, mode);
    isolate->MaybeInitializeVectorListFromHeap();
  }
  isolate->set_type_profile_mode(mode);
}

}  // namespace internal
}  // namespace v8
