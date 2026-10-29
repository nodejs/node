// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_INSTANCE_TYPES_GEN_H_
#define V8_OBJECTS_INSTANCE_TYPES_GEN_H_

// Selects which instance-type generator's output is compiled into V8:
// metagen (tools/metagen/) or Torque's emission. Controlled by the build arg
// v8_use_metagen_instance_types (GN and Bazel): when off, the build sets
// V8_USE_METAGEN_INSTANCE_TYPES=0 (the "features" config in BUILD.gn,
// ":define_flags" in BUILD.bazel); when on it is left undefined and the
// fallback below selects metagen, so every translation unit agrees regardless
// of which configs it picks up. Torque runs either way; the metagen harvest is
// only in the build graph when the arg is on.
#if !defined(V8_USE_METAGEN_INSTANCE_TYPES)
#define V8_USE_METAGEN_INSTANCE_TYPES 1
#endif

// The harvest pass is checked before the switch, not inside it: the
// harvest action depends on neither generator, so on either setting its
// parse would reach for a header that does not exist yet.
#ifdef V8_METAGEN_GENERATION_PASS
// This is metagen's own libclang harvest, parsing the headers that
// produce metagen/instance-types.h -- so that file does not exist yet,
// and Torque has not necessarily run either. Define the macros both
// paths supply as no-ops: the harvest reads class declarations, not
// instance-type values, and instance-type.h skips its file-scope uses
// of the generated symbols under the same guard.
#define TORQUE_ASSIGNED_INSTANCE_TYPES(V)
#define TORQUE_ASSIGNED_INSTANCE_TYPE_LIST(V)
#define INSTANCE_TYPE_LIST_SINGLE(V)
#define INSTANCE_TYPE_LIST_MULTIPLE(V)
#define INSTANCE_TYPE_LIST_RANGE(V)
#elif V8_USE_METAGEN_INSTANCE_TYPES
#include "metagen/instance-types.h"
#else
#include "torque-generated/instance-types.h"
// metagen emits the INSTANCE_TYPE_LIST_{SINGLE,MULTIPLE,RANGE} buckets
// inside metagen/instance-types.h; on the Torque path they come from this
// separate generated header. Remove with the
// V8_USE_METAGEN_INSTANCE_TYPES switch.
#include "torque-generated/instance-type-checker-lists.h"
// metagen derives HEAP_OBJECT_DIAGNOSTIC_DISPATCH_LIST from Name##Print /
// Name##Verify declarations, which Torque knows nothing about. The debug-
// reader lists (classes with a Torque body) are the exact set that carried
// the Print/Verify dispatch before the metagen cutover, so alias them here.
// Remove with the V8_USE_METAGEN_INSTANCE_TYPES switch.
#include "torque-generated/debug-reader-classes-list.h"
#define HEAP_OBJECT_DIAGNOSTIC_DISPATCH_LIST(V) \
  TORQUE_DEBUG_READER_CLASSES_SINGLE(V)         \
  TORQUE_DEBUG_READER_CLASSES_MULTIPLE(V)
#endif

#endif  // V8_OBJECTS_INSTANCE_TYPES_GEN_H_
