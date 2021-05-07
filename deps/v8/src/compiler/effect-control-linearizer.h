// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_EFFECT_CONTROL_LINEARIZER_H_
#define V8_COMPILER_EFFECT_CONTROL_LINEARIZER_H_

#include <vector>

#include "src/handles/handles.h"

namespace v8 {
namespace internal {

class Map;
class Zone;

namespace compiler {

class JSGraph;
class NodeOriginTable;
class Schedule;
class SourcePositionTable;
class JSHeapBroker;

enum class MaskArrayIndexEnable { kDoNotMaskArrayIndex, kMaskArrayIndex };

enum class MaintainSchedule { kMaintain, kDiscard };

V8_EXPORT_PRIVATE void LinearizeEffectControl(
    JSGraph* graph, Schedule* schedule, Zone* temp_zone,
    SourcePositionTable* source_positions, NodeOriginTable* node_origins,
    MaskArrayIndexEnable mask_array_index, MaintainSchedule maintain_schedule,
    JSHeapBroker* broker);

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_EFFECT_CONTROL_LINEARIZER_H_
