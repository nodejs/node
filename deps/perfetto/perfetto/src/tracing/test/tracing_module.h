/*
 * Copyright (C) 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACING_TEST_TRACING_MODULE_H_
#define SRC_TRACING_TEST_TRACING_MODULE_H_

// Note: No non-client API header includes are allowed here.

namespace perfetto {
namespace internal {
struct TrackEventIncrementalState;
}  // namespace internal
}  // namespace perfetto

namespace tracing_module {

void InitializeCategories();
void EmitTrackEvents();
void EmitTrackEvents2();
perfetto::internal::TrackEventIncrementalState* GetIncrementalState();

// These functions are used to check the instruction size overhead track events.
void FunctionWithOneTrackEvent();
void FunctionWithOneTrackEventWithTypedArgument();
void FunctionWithOneScopedTrackEvent();
void FunctionWithOneTrackEventWithDebugAnnotations();
void FunctionWithOneTrackEventWithCustomTrack();

// Legacy events.
void FunctionWithOneLegacyEvent();
void FunctionWithOneScopedLegacyEvent();

}  // namespace tracing_module

#endif  // SRC_TRACING_TEST_TRACING_MODULE_H_
