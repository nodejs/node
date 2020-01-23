// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_SERIALIZER_FOR_BACKGROUND_COMPILATION_H_
#define V8_COMPILER_SERIALIZER_FOR_BACKGROUND_COMPILATION_H_

#include "src/handles/handles.h"

namespace v8 {
namespace internal {

class BailoutId;
class Zone;

namespace compiler {

class CompilationDependencies;
class JSHeapBroker;
class ZoneStats;

enum class SerializerForBackgroundCompilationFlag : uint8_t {
  kBailoutOnUninitialized = 1 << 0,
  kCollectSourcePositions = 1 << 1,
  kAnalyzeEnvironmentLiveness = 1 << 2,
};
using SerializerForBackgroundCompilationFlags =
    base::Flags<SerializerForBackgroundCompilationFlag>;

void RunSerializerForBackgroundCompilation(
    ZoneStats* zone_stats, JSHeapBroker* broker,
    CompilationDependencies* dependencies, Handle<JSFunction> closure,
    SerializerForBackgroundCompilationFlags flags, BailoutId osr_offset);

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_SERIALIZER_FOR_BACKGROUND_COMPILATION_H_
