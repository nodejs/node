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

#ifndef SRC_TRACE_PROCESSOR_TABLES_PROFILER_TABLES_H_
#define SRC_TRACE_PROCESSOR_TABLES_PROFILER_TABLES_H_

#include "src/trace_processor/tables/macros.h"

namespace perfetto {
namespace trace_processor {
namespace tables {

#define PERFETTO_TP_PROFILER_SMAPS_DEF(NAME, PARENT, C) \
  NAME(ProfilerSmapsTable, "profiler_smaps")            \
  PERFETTO_TP_ROOT_TABLE(PARENT, C)                     \
  C(uint32_t, upid)                                     \
  C(int64_t, ts)                                        \
  C(StringPool::Id, path)                               \
  C(int64_t, size_kb)                                   \
  C(int64_t, private_dirty_kb)                          \
  C(int64_t, swap_kb)

PERFETTO_TP_TABLE(PERFETTO_TP_PROFILER_SMAPS_DEF);

#define PERFETTO_TP_PACKAGES_LIST_DEF(NAME, PARENT, C) \
  NAME(PackageListTable, "package_list")               \
  PERFETTO_TP_ROOT_TABLE(PARENT, C)                    \
  C(StringPool::Id, package_name)                      \
  C(int64_t, uid)                                      \
  C(int32_t, debuggable)                               \
  C(int32_t, profileable_from_shell)                   \
  C(int64_t, version_code)

PERFETTO_TP_TABLE(PERFETTO_TP_PACKAGES_LIST_DEF);

#define PERFETTO_TP_STACK_PROFILE_MAPPING_DEF(NAME, PARENT, C) \
  NAME(StackProfileMappingTable, "stack_profile_mapping")      \
  PERFETTO_TP_ROOT_TABLE(PARENT, C)                            \
  C(StringPool::Id, build_id)                                  \
  C(int64_t, exact_offset)                                     \
  C(int64_t, start_offset)                                     \
  C(int64_t, start)                                            \
  C(int64_t, end)                                              \
  C(int64_t, load_bias)                                        \
  C(StringPool::Id, name)

PERFETTO_TP_TABLE(PERFETTO_TP_STACK_PROFILE_MAPPING_DEF);

#define PERFETTO_TP_STACK_PROFILE_FRAME_DEF(NAME, PARENT, C) \
  NAME(StackProfileFrameTable, "stack_profile_frame")        \
  PERFETTO_TP_ROOT_TABLE(PARENT, C)                          \
  C(StringPool::Id, name)                                    \
  C(StackProfileMappingTable::Id, mapping)                   \
  C(int64_t, rel_pc)                                         \
  C(base::Optional<uint32_t>, symbol_set_id)

PERFETTO_TP_TABLE(PERFETTO_TP_STACK_PROFILE_FRAME_DEF);

#define PERFETTO_TP_STACK_PROFILE_CALLSITE_DEF(NAME, PARENT, C) \
  NAME(StackProfileCallsiteTable, "stack_profile_callsite")     \
  PERFETTO_TP_ROOT_TABLE(PARENT, C)                             \
  C(uint32_t, depth)                                            \
  C(base::Optional<StackProfileCallsiteTable::Id>, parent_id)   \
  C(StackProfileFrameTable::Id, frame_id)

PERFETTO_TP_TABLE(PERFETTO_TP_STACK_PROFILE_CALLSITE_DEF);

#define PERFETTO_TP_CPU_PROFILE_STACK_SAMPLE_DEF(NAME, PARENT, C) \
  NAME(CpuProfileStackSampleTable, "cpu_profile_stack_sample")    \
  PERFETTO_TP_ROOT_TABLE(PARENT, C)                               \
  C(int64_t, ts, Column::Flag::kSorted)                           \
  C(StackProfileCallsiteTable::Id, callsite_id)                   \
  C(uint32_t, utid)                                               \
  C(int32_t, process_priority)

PERFETTO_TP_TABLE(PERFETTO_TP_CPU_PROFILE_STACK_SAMPLE_DEF);

#define PERFETTO_TP_SYMBOL_DEF(NAME, PARENT, C) \
  NAME(SymbolTable, "stack_profile_symbol")     \
  PERFETTO_TP_ROOT_TABLE(PARENT, C)             \
  C(uint32_t, symbol_set_id)                    \
  C(StringPool::Id, name)                       \
  C(StringPool::Id, source_file)                \
  C(uint32_t, line_number)

PERFETTO_TP_TABLE(PERFETTO_TP_SYMBOL_DEF);

#define PERFETTO_TP_HEAP_PROFILE_ALLOCATION_DEF(NAME, PARENT, C) \
  NAME(HeapProfileAllocationTable, "heap_profile_allocation")    \
  PERFETTO_TP_ROOT_TABLE(PARENT, C)                              \
  C(int64_t, ts, Column::Flag::kSorted)                          \
  C(uint32_t, upid)                                              \
  C(StackProfileCallsiteTable::Id, callsite_id)                  \
  C(int64_t, count)                                              \
  C(int64_t, size)

PERFETTO_TP_TABLE(PERFETTO_TP_HEAP_PROFILE_ALLOCATION_DEF);

#define PERFETTO_TP_EXPERIMENTAL_FLAMEGRAPH_NODES(NAME, PARENT, C)        \
  NAME(ExperimentalFlamegraphNodesTable, "experimental_flamegraph_nodes") \
  PERFETTO_TP_ROOT_TABLE(PARENT, C)                                       \
  C(int64_t, ts, Column::Flag::kSorted | Column::Flag::kHidden)           \
  C(uint32_t, upid, Column::Flag::kHidden)                                \
  C(StringPool::Id, profile_type, Column::Flag::kHidden)                  \
  C(StringPool::Id, focus_str, Column::Flag::kHidden)                     \
  C(uint32_t, depth)                                                      \
  C(StringPool::Id, name)                                                 \
  C(StringPool::Id, map_name)                                             \
  C(int64_t, count)                                                       \
  C(int64_t, cumulative_count)                                            \
  C(int64_t, size)                                                        \
  C(int64_t, cumulative_size)                                             \
  C(int64_t, alloc_count)                                                 \
  C(int64_t, cumulative_alloc_count)                                      \
  C(int64_t, alloc_size)                                                  \
  C(int64_t, cumulative_alloc_size)                                       \
  C(base::Optional<ExperimentalFlamegraphNodesTable::Id>, parent_id)

PERFETTO_TP_TABLE(PERFETTO_TP_EXPERIMENTAL_FLAMEGRAPH_NODES);

#define PERFETTO_TP_HEAP_GRAPH_CLASS_DEF(NAME, PARENT, C) \
  NAME(HeapGraphClassTable, "heap_graph_class")           \
  PERFETTO_TP_ROOT_TABLE(PARENT, C)                       \
  C(StringPool::Id, name)                                 \
  C(base::Optional<StringPool::Id>, deobfuscated_name)    \
  C(base::Optional<StringPool::Id>, location)

PERFETTO_TP_TABLE(PERFETTO_TP_HEAP_GRAPH_CLASS_DEF);

#define PERFETTO_TP_HEAP_GRAPH_OBJECT_DEF(NAME, PARENT, C) \
  NAME(HeapGraphObjectTable, "heap_graph_object")          \
  PERFETTO_TP_ROOT_TABLE(PARENT, C)                        \
  C(uint32_t, upid)                                        \
  C(int64_t, graph_sample_ts)                              \
  C(int64_t, object_id)                                    \
  C(int64_t, self_size)                                    \
  C(int64_t, retained_size)                                \
  C(int64_t, unique_retained_size)                         \
  C(base::Optional<uint32_t>, reference_set_id)            \
  C(int32_t, reachable)                                    \
  C(HeapGraphClassTable::Id, type_id)                      \
  C(base::Optional<StringPool::Id>, root_type)

PERFETTO_TP_TABLE(PERFETTO_TP_HEAP_GRAPH_OBJECT_DEF);

#define PERFETTO_TP_HEAP_GRAPH_REFERENCE_DEF(NAME, PARENT, C) \
  NAME(HeapGraphReferenceTable, "heap_graph_reference")       \
  PERFETTO_TP_ROOT_TABLE(PARENT, C)                           \
  C(uint32_t, reference_set_id, Column::Flag::kSorted)        \
  C(int64_t, owner_id)                                        \
  C(int64_t, owned_id)                                        \
  C(StringPool::Id, field_name)                               \
  C(StringPool::Id, field_type_name)                          \
  C(base::Optional<StringPool::Id>, deobfuscated_field_name)

PERFETTO_TP_TABLE(PERFETTO_TP_HEAP_GRAPH_REFERENCE_DEF);

#define PERFETTO_TP_VULKAN_MEMORY_ALLOCATIONS_DEF(NAME, PARENT, C) \
  NAME(VulkanMemoryAllocationsTable, "vulkan_memory_allocations")  \
  PERFETTO_TP_ROOT_TABLE(PARENT, C)                                \
  C(StringPool::Id, source)                                        \
  C(StringPool::Id, operation)                                     \
  C(int64_t, timestamp)                                            \
  C(base::Optional<uint32_t>, upid)                                \
  C(base::Optional<int64_t>, device)                               \
  C(base::Optional<int64_t>, device_memory)                        \
  C(base::Optional<uint32_t>, memory_type)                         \
  C(base::Optional<uint32_t>, heap)                                \
  C(base::Optional<StringPool::Id>, function_name)                 \
  C(base::Optional<int64_t>, object_handle)                        \
  C(base::Optional<int64_t>, memory_address)                       \
  C(base::Optional<int64_t>, memory_size)                          \
  C(StringPool::Id, scope)                                         \
  C(base::Optional<uint32_t>, arg_set_id)

PERFETTO_TP_TABLE(PERFETTO_TP_VULKAN_MEMORY_ALLOCATIONS_DEF);

}  // namespace tables
}  // namespace trace_processor
}  // namespace perfetto

#endif  // SRC_TRACE_PROCESSOR_TABLES_PROFILER_TABLES_H_
