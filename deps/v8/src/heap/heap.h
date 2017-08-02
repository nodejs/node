// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_HEAP_H_
#define V8_HEAP_HEAP_H_

#include <cmath>
#include <vector>

// Clients of this interface shouldn't depend on lots of heap internals.
// Do not include anything from src/heap here!
#include "include/v8.h"
#include "src/allocation.h"
#include "src/assert-scope.h"
#include "src/base/atomic-utils.h"
#include "src/debug/debug-interface.h"
#include "src/globals.h"
#include "src/heap-symbols.h"
#include "src/list.h"
#include "src/objects.h"
#include "src/objects/hash-table.h"
#include "src/objects/string-table.h"
#include "src/visitors.h"

namespace v8 {
namespace internal {

using v8::MemoryPressureLevel;

// Defines all the roots in Heap.
#define STRONG_ROOT_LIST(V)                                                    \
  /* Cluster the most popular ones in a few cache lines here at the top.    */ \
  /* The first 32 entries are most often used in the startup snapshot and   */ \
  /* can use a shorter representation in the serialization format.          */ \
  V(Map, free_space_map, FreeSpaceMap)                                         \
  V(Map, one_pointer_filler_map, OnePointerFillerMap)                          \
  V(Map, two_pointer_filler_map, TwoPointerFillerMap)                          \
  V(Oddball, uninitialized_value, UninitializedValue)                          \
  V(Oddball, undefined_value, UndefinedValue)                                  \
  V(Oddball, the_hole_value, TheHoleValue)                                     \
  V(Oddball, null_value, NullValue)                                            \
  V(Oddball, true_value, TrueValue)                                            \
  V(Oddball, false_value, FalseValue)                                          \
  V(String, empty_string, empty_string)                                        \
  V(Map, meta_map, MetaMap)                                                    \
  V(Map, byte_array_map, ByteArrayMap)                                         \
  V(Map, fixed_array_map, FixedArrayMap)                                       \
  V(Map, fixed_cow_array_map, FixedCOWArrayMap)                                \
  V(Map, hash_table_map, HashTableMap)                                         \
  V(Map, symbol_map, SymbolMap)                                                \
  V(Map, one_byte_string_map, OneByteStringMap)                                \
  V(Map, one_byte_internalized_string_map, OneByteInternalizedStringMap)       \
  V(Map, scope_info_map, ScopeInfoMap)                                         \
  V(Map, shared_function_info_map, SharedFunctionInfoMap)                      \
  V(Map, code_map, CodeMap)                                                    \
  V(Map, function_context_map, FunctionContextMap)                             \
  V(Map, cell_map, CellMap)                                                    \
  V(Map, weak_cell_map, WeakCellMap)                                           \
  V(Map, global_property_cell_map, GlobalPropertyCellMap)                      \
  V(Map, foreign_map, ForeignMap)                                              \
  V(Map, heap_number_map, HeapNumberMap)                                       \
  V(Map, transition_array_map, TransitionArrayMap)                             \
  V(Map, feedback_vector_map, FeedbackVectorMap)                               \
  V(ScopeInfo, empty_scope_info, EmptyScopeInfo)                               \
  V(FixedArray, empty_fixed_array, EmptyFixedArray)                            \
  V(DescriptorArray, empty_descriptor_array, EmptyDescriptorArray)             \
  /* Entries beyond the first 32                                            */ \
  /* The roots above this line should be boring from a GC point of view.    */ \
  /* This means they are never in new space and never on a page that is     */ \
  /* being compacted.                                                       */ \
  /* Oddballs */                                                               \
  V(Oddball, arguments_marker, ArgumentsMarker)                                \
  V(Oddball, exception, Exception)                                             \
  V(Oddball, termination_exception, TerminationException)                      \
  V(Oddball, optimized_out, OptimizedOut)                                      \
  V(Oddball, stale_register, StaleRegister)                                    \
  /* Context maps */                                                           \
  V(Map, native_context_map, NativeContextMap)                                 \
  V(Map, module_context_map, ModuleContextMap)                                 \
  V(Map, eval_context_map, EvalContextMap)                                     \
  V(Map, script_context_map, ScriptContextMap)                                 \
  V(Map, block_context_map, BlockContextMap)                                   \
  V(Map, catch_context_map, CatchContextMap)                                   \
  V(Map, with_context_map, WithContextMap)                                     \
  V(Map, debug_evaluate_context_map, DebugEvaluateContextMap)                  \
  V(Map, script_context_table_map, ScriptContextTableMap)                      \
  /* Maps */                                                                   \
  V(Map, fixed_double_array_map, FixedDoubleArrayMap)                          \
  V(Map, mutable_heap_number_map, MutableHeapNumberMap)                        \
  V(Map, ordered_hash_table_map, OrderedHashTableMap)                          \
  V(Map, unseeded_number_dictionary_map, UnseededNumberDictionaryMap)          \
  V(Map, sloppy_arguments_elements_map, SloppyArgumentsElementsMap)            \
  V(Map, message_object_map, JSMessageObjectMap)                               \
  V(Map, external_map, ExternalMap)                                            \
  V(Map, bytecode_array_map, BytecodeArrayMap)                                 \
  V(Map, module_info_map, ModuleInfoMap)                                       \
  V(Map, no_closures_cell_map, NoClosuresCellMap)                              \
  V(Map, one_closure_cell_map, OneClosureCellMap)                              \
  V(Map, many_closures_cell_map, ManyClosuresCellMap)                          \
  /* String maps */                                                            \
  V(Map, native_source_string_map, NativeSourceStringMap)                      \
  V(Map, string_map, StringMap)                                                \
  V(Map, cons_one_byte_string_map, ConsOneByteStringMap)                       \
  V(Map, cons_string_map, ConsStringMap)                                       \
  V(Map, thin_one_byte_string_map, ThinOneByteStringMap)                       \
  V(Map, thin_string_map, ThinStringMap)                                       \
  V(Map, sliced_string_map, SlicedStringMap)                                   \
  V(Map, sliced_one_byte_string_map, SlicedOneByteStringMap)                   \
  V(Map, external_string_map, ExternalStringMap)                               \
  V(Map, external_string_with_one_byte_data_map,                               \
    ExternalStringWithOneByteDataMap)                                          \
  V(Map, external_one_byte_string_map, ExternalOneByteStringMap)               \
  V(Map, short_external_string_map, ShortExternalStringMap)                    \
  V(Map, short_external_string_with_one_byte_data_map,                         \
    ShortExternalStringWithOneByteDataMap)                                     \
  V(Map, internalized_string_map, InternalizedStringMap)                       \
  V(Map, external_internalized_string_map, ExternalInternalizedStringMap)      \
  V(Map, external_internalized_string_with_one_byte_data_map,                  \
    ExternalInternalizedStringWithOneByteDataMap)                              \
  V(Map, external_one_byte_internalized_string_map,                            \
    ExternalOneByteInternalizedStringMap)                                      \
  V(Map, short_external_internalized_string_map,                               \
    ShortExternalInternalizedStringMap)                                        \
  V(Map, short_external_internalized_string_with_one_byte_data_map,            \
    ShortExternalInternalizedStringWithOneByteDataMap)                         \
  V(Map, short_external_one_byte_internalized_string_map,                      \
    ShortExternalOneByteInternalizedStringMap)                                 \
  V(Map, short_external_one_byte_string_map, ShortExternalOneByteStringMap)    \
  /* Array element maps */                                                     \
  V(Map, fixed_uint8_array_map, FixedUint8ArrayMap)                            \
  V(Map, fixed_int8_array_map, FixedInt8ArrayMap)                              \
  V(Map, fixed_uint16_array_map, FixedUint16ArrayMap)                          \
  V(Map, fixed_int16_array_map, FixedInt16ArrayMap)                            \
  V(Map, fixed_uint32_array_map, FixedUint32ArrayMap)                          \
  V(Map, fixed_int32_array_map, FixedInt32ArrayMap)                            \
  V(Map, fixed_float32_array_map, FixedFloat32ArrayMap)                        \
  V(Map, fixed_float64_array_map, FixedFloat64ArrayMap)                        \
  V(Map, fixed_uint8_clamped_array_map, FixedUint8ClampedArrayMap)             \
  /* Oddball maps */                                                           \
  V(Map, undefined_map, UndefinedMap)                                          \
  V(Map, the_hole_map, TheHoleMap)                                             \
  V(Map, null_map, NullMap)                                                    \
  V(Map, boolean_map, BooleanMap)                                              \
  V(Map, uninitialized_map, UninitializedMap)                                  \
  V(Map, arguments_marker_map, ArgumentsMarkerMap)                             \
  V(Map, exception_map, ExceptionMap)                                          \
  V(Map, termination_exception_map, TerminationExceptionMap)                   \
  V(Map, optimized_out_map, OptimizedOutMap)                                   \
  V(Map, stale_register_map, StaleRegisterMap)                                 \
  /* Canonical empty values */                                                 \
  V(ByteArray, empty_byte_array, EmptyByteArray)                               \
  V(FixedTypedArrayBase, empty_fixed_uint8_array, EmptyFixedUint8Array)        \
  V(FixedTypedArrayBase, empty_fixed_int8_array, EmptyFixedInt8Array)          \
  V(FixedTypedArrayBase, empty_fixed_uint16_array, EmptyFixedUint16Array)      \
  V(FixedTypedArrayBase, empty_fixed_int16_array, EmptyFixedInt16Array)        \
  V(FixedTypedArrayBase, empty_fixed_uint32_array, EmptyFixedUint32Array)      \
  V(FixedTypedArrayBase, empty_fixed_int32_array, EmptyFixedInt32Array)        \
  V(FixedTypedArrayBase, empty_fixed_float32_array, EmptyFixedFloat32Array)    \
  V(FixedTypedArrayBase, empty_fixed_float64_array, EmptyFixedFloat64Array)    \
  V(FixedTypedArrayBase, empty_fixed_uint8_clamped_array,                      \
    EmptyFixedUint8ClampedArray)                                               \
  V(Script, empty_script, EmptyScript)                                         \
  V(Cell, undefined_cell, UndefinedCell)                                       \
  V(FixedArray, empty_sloppy_arguments_elements, EmptySloppyArgumentsElements) \
  V(SeededNumberDictionary, empty_slow_element_dictionary,                     \
    EmptySlowElementDictionary)                                                \
  V(PropertyCell, empty_property_cell, EmptyPropertyCell)                      \
  V(WeakCell, empty_weak_cell, EmptyWeakCell)                                  \
  V(InterceptorInfo, noop_interceptor_info, NoOpInterceptorInfo)               \
  /* Protectors */                                                             \
  V(PropertyCell, array_protector, ArrayProtector)                             \
  V(Cell, is_concat_spreadable_protector, IsConcatSpreadableProtector)         \
  V(Cell, species_protector, SpeciesProtector)                                 \
  V(PropertyCell, string_length_protector, StringLengthProtector)              \
  V(Cell, fast_array_iteration_protector, FastArrayIterationProtector)         \
  V(PropertyCell, array_iterator_protector, ArrayIteratorProtector)            \
  V(PropertyCell, array_buffer_neutering_protector,                            \
    ArrayBufferNeuteringProtector)                                             \
  /* Special numbers */                                                        \
  V(HeapNumber, nan_value, NanValue)                                           \
  V(HeapNumber, hole_nan_value, HoleNanValue)                                  \
  V(HeapNumber, infinity_value, InfinityValue)                                 \
  V(HeapNumber, minus_zero_value, MinusZeroValue)                              \
  V(HeapNumber, minus_infinity_value, MinusInfinityValue)                      \
  /* Caches */                                                                 \
  V(FixedArray, number_string_cache, NumberStringCache)                        \
  V(FixedArray, single_character_string_cache, SingleCharacterStringCache)     \
  V(FixedArray, string_split_cache, StringSplitCache)                          \
  V(FixedArray, regexp_multiple_cache, RegExpMultipleCache)                    \
  V(Object, instanceof_cache_function, InstanceofCacheFunction)                \
  V(Object, instanceof_cache_map, InstanceofCacheMap)                          \
  V(Object, instanceof_cache_answer, InstanceofCacheAnswer)                    \
  /* Lists and dictionaries */                                                 \
  V(NameDictionary, empty_properties_dictionary, EmptyPropertiesDictionary)    \
  V(NameDictionary, public_symbol_table, PublicSymbolTable)                    \
  V(NameDictionary, api_symbol_table, ApiSymbolTable)                          \
  V(NameDictionary, api_private_symbol_table, ApiPrivateSymbolTable)           \
  V(Object, script_list, ScriptList)                                           \
  V(UnseededNumberDictionary, code_stubs, CodeStubs)                           \
  V(FixedArray, materialized_objects, MaterializedObjects)                     \
  V(FixedArray, microtask_queue, MicrotaskQueue)                               \
  V(FixedArray, detached_contexts, DetachedContexts)                           \
  V(ArrayList, retained_maps, RetainedMaps)                                    \
  V(WeakHashTable, weak_object_to_code_table, WeakObjectToCodeTable)           \
  /* weak_new_space_object_to_code_list is an array of weak cells, where */    \
  /* slots with even indices refer to the weak object, and the subsequent */   \
  /* slots refer to the code with the reference to the weak object. */         \
  V(ArrayList, weak_new_space_object_to_code_list,                             \
    WeakNewSpaceObjectToCodeList)                                              \
  /* List to hold onto feedback vectors that we need for code coverage */      \
  V(Object, code_coverage_list, CodeCoverageList)                              \
  V(Object, weak_stack_trace_list, WeakStackTraceList)                         \
  V(Object, noscript_shared_function_infos, NoScriptSharedFunctionInfos)       \
  V(FixedArray, serialized_templates, SerializedTemplates)                     \
  V(FixedArray, serialized_global_proxy_sizes, SerializedGlobalProxySizes)     \
  V(TemplateList, message_listeners, MessageListeners)                         \
  /* per-Isolate map for JSPromiseCapability. */                               \
  /* TODO(caitp): Make this a Struct */                                        \
  V(Map, js_promise_capability_map, JSPromiseCapabilityMap)                    \
  /* JS Entries */                                                             \
  V(Code, js_entry_code, JsEntryCode)                                          \
  V(Code, js_construct_entry_code, JsConstructEntryCode)

// Entries in this list are limited to Smis and are not visited during GC.
#define SMI_ROOT_LIST(V)                                                       \
  V(Smi, stack_limit, StackLimit)                                              \
  V(Smi, real_stack_limit, RealStackLimit)                                     \
  V(Smi, last_script_id, LastScriptId)                                         \
  V(Smi, hash_seed, HashSeed)                                                  \
  /* To distinguish the function templates, so that we can find them in the */ \
  /* function cache of the native context. */                                  \
  V(Smi, next_template_serial_number, NextTemplateSerialNumber)                \
  V(Smi, arguments_adaptor_deopt_pc_offset, ArgumentsAdaptorDeoptPCOffset)     \
  V(Smi, construct_stub_create_deopt_pc_offset,                                \
    ConstructStubCreateDeoptPCOffset)                                          \
  V(Smi, construct_stub_invoke_deopt_pc_offset,                                \
    ConstructStubInvokeDeoptPCOffset)                                          \
  V(Smi, getter_stub_deopt_pc_offset, GetterStubDeoptPCOffset)                 \
  V(Smi, setter_stub_deopt_pc_offset, SetterStubDeoptPCOffset)                 \
  V(Smi, interpreter_entry_return_pc_offset, InterpreterEntryReturnPCOffset)

#define ROOT_LIST(V)  \
  STRONG_ROOT_LIST(V) \
  SMI_ROOT_LIST(V)    \
  V(StringTable, string_table, StringTable)


// Heap roots that are known to be immortal immovable, for which we can safely
// skip write barriers. This list is not complete and has omissions.
#define IMMORTAL_IMMOVABLE_ROOT_LIST(V) \
  V(ArgumentsMarker)                    \
  V(ArgumentsMarkerMap)                 \
  V(ArrayBufferNeuteringProtector)      \
  V(ArrayIteratorProtector)             \
  V(ArrayProtector)                     \
  V(BlockContextMap)                    \
  V(BooleanMap)                         \
  V(ByteArrayMap)                       \
  V(BytecodeArrayMap)                   \
  V(CatchContextMap)                    \
  V(CellMap)                            \
  V(CodeMap)                            \
  V(EmptyByteArray)                     \
  V(EmptyDescriptorArray)               \
  V(EmptyFixedArray)                    \
  V(EmptyFixedFloat32Array)             \
  V(EmptyFixedFloat64Array)             \
  V(EmptyFixedInt16Array)               \
  V(EmptyFixedInt32Array)               \
  V(EmptyFixedInt8Array)                \
  V(EmptyFixedUint16Array)              \
  V(EmptyFixedUint32Array)              \
  V(EmptyFixedUint8Array)               \
  V(EmptyFixedUint8ClampedArray)        \
  V(EmptyPropertyCell)                  \
  V(EmptyScopeInfo)                     \
  V(EmptyScript)                        \
  V(EmptySloppyArgumentsElements)       \
  V(EmptySlowElementDictionary)         \
  V(empty_string)                       \
  V(EmptyWeakCell)                      \
  V(EvalContextMap)                     \
  V(Exception)                          \
  V(FalseValue)                         \
  V(FastArrayIterationProtector)        \
  V(FixedArrayMap)                      \
  V(FixedCOWArrayMap)                   \
  V(FixedDoubleArrayMap)                \
  V(ForeignMap)                         \
  V(FreeSpaceMap)                       \
  V(FunctionContextMap)                 \
  V(GlobalPropertyCellMap)              \
  V(HashTableMap)                       \
  V(HeapNumberMap)                      \
  V(HoleNanValue)                       \
  V(InfinityValue)                      \
  V(IsConcatSpreadableProtector)        \
  V(JsConstructEntryCode)               \
  V(JsEntryCode)                        \
  V(JSMessageObjectMap)                 \
  V(ManyClosuresCellMap)                \
  V(MetaMap)                            \
  V(MinusInfinityValue)                 \
  V(MinusZeroValue)                     \
  V(ModuleContextMap)                   \
  V(ModuleInfoMap)                      \
  V(MutableHeapNumberMap)               \
  V(NanValue)                           \
  V(NativeContextMap)                   \
  V(NoClosuresCellMap)                  \
  V(NullMap)                            \
  V(NullValue)                          \
  V(OneClosureCellMap)                  \
  V(OnePointerFillerMap)                \
  V(OptimizedOut)                       \
  V(OrderedHashTableMap)                \
  V(ScopeInfoMap)                       \
  V(ScriptContextMap)                   \
  V(SharedFunctionInfoMap)              \
  V(SloppyArgumentsElementsMap)         \
  V(SpeciesProtector)                   \
  V(StaleRegister)                      \
  V(StringLengthProtector)              \
  V(SymbolMap)                          \
  V(TerminationException)               \
  V(TheHoleMap)                         \
  V(TheHoleValue)                       \
  V(TransitionArrayMap)                 \
  V(TrueValue)                          \
  V(TwoPointerFillerMap)                \
  V(UndefinedCell)                      \
  V(UndefinedMap)                       \
  V(UndefinedValue)                     \
  V(UninitializedMap)                   \
  V(UninitializedValue)                 \
  V(WeakCellMap)                        \
  V(WithContextMap)                     \
  PRIVATE_SYMBOL_LIST(V)

// Forward declarations.
class AllocationObserver;
class ArrayBufferTracker;
class ConcurrentMarking;
class GCIdleTimeAction;
class GCIdleTimeHandler;
class GCIdleTimeHeapState;
class GCTracer;
class HeapObjectsFilter;
class HeapStats;
class HistogramTimer;
class Isolate;
class LocalEmbedderHeapTracer;
class MemoryAllocator;
class MemoryReducer;
class MinorMarkCompactCollector;
class ObjectIterator;
class ObjectStats;
class Page;
class PagedSpace;
class RootVisitor;
class Scavenger;
class ScavengeJob;
class Space;
class StoreBuffer;
class TracePossibleWrapperReporter;
class WeakObjectRetainer;

typedef void (*ObjectSlotCallback)(HeapObject** from, HeapObject* to);

enum ArrayStorageAllocationMode {
  DONT_INITIALIZE_ARRAY_ELEMENTS,
  INITIALIZE_ARRAY_ELEMENTS_WITH_HOLE
};

enum class ClearRecordedSlots { kYes, kNo };

enum class GarbageCollectionReason {
  kUnknown = 0,
  kAllocationFailure = 1,
  kAllocationLimit = 2,
  kContextDisposal = 3,
  kCountersExtension = 4,
  kDebugger = 5,
  kDeserializer = 6,
  kExternalMemoryPressure = 7,
  kFinalizeMarkingViaStackGuard = 8,
  kFinalizeMarkingViaTask = 9,
  kFullHashtable = 10,
  kHeapProfiler = 11,
  kIdleTask = 12,
  kLastResort = 13,
  kLowMemoryNotification = 14,
  kMakeHeapIterable = 15,
  kMemoryPressure = 16,
  kMemoryReducer = 17,
  kRuntime = 18,
  kSamplingProfiler = 19,
  kSnapshotCreator = 20,
  kTesting = 21
  // If you add new items here, then update the incremental_marking_reason,
  // mark_compact_reason, and scavenge_reason counters in counters.h.
  // Also update src/tools/metrics/histograms/histograms.xml in chromium.
};

enum class YoungGenerationHandling {
  kRegularScavenge = 0,
  kFastPromotionDuringScavenge = 1,
  // Histogram::InspectConstructionArguments in chromium requires us to have at
  // least three buckets.
  kUnusedBucket = 2,
  // If you add new items here, then update the young_generation_handling in
  // counters.h.
  // Also update src/tools/metrics/histograms/histograms.xml in chromium.
};

// A queue of objects promoted during scavenge. Each object is accompanied by
// its size to avoid dereferencing a map pointer for scanning. The last page in
// to-space is used for the promotion queue. On conflict during scavenge, the
// promotion queue is allocated externally and all entries are copied to the
// external queue.
class PromotionQueue {
 public:
  explicit PromotionQueue(Heap* heap)
      : front_(nullptr),
        rear_(nullptr),
        limit_(nullptr),
        emergency_stack_(nullptr),
        heap_(heap) {}

  void Initialize();
  void Destroy();

  inline void SetNewLimit(Address limit);
  inline bool IsBelowPromotionQueue(Address to_space_top);

  inline void insert(HeapObject* target, int32_t size);
  inline void remove(HeapObject** target, int32_t* size);

  bool is_empty() {
    return (front_ == rear_) &&
           (emergency_stack_ == nullptr || emergency_stack_->length() == 0);
  }

 private:
  struct Entry {
    Entry(HeapObject* obj, int32_t size) : obj_(obj), size_(size) {}

    HeapObject* obj_;
    int32_t size_;
  };

  inline Page* GetHeadPage();

  void RelocateQueueHead();

  // The front of the queue is higher in the memory page chain than the rear.
  struct Entry* front_;
  struct Entry* rear_;
  struct Entry* limit_;

  List<Entry>* emergency_stack_;
  Heap* heap_;

  DISALLOW_COPY_AND_ASSIGN(PromotionQueue);
};

class AllocationResult {
 public:
  static inline AllocationResult Retry(AllocationSpace space = NEW_SPACE) {
    return AllocationResult(space);
  }

  // Implicit constructor from Object*.
  AllocationResult(Object* object)  // NOLINT
      : object_(object) {
    // AllocationResults can't return Smis, which are used to represent
    // failure and the space to retry in.
    CHECK(!object->IsSmi());
  }

  AllocationResult() : object_(Smi::FromInt(NEW_SPACE)) {}

  inline bool IsRetry() { return object_->IsSmi(); }
  inline HeapObject* ToObjectChecked();
  inline AllocationSpace RetrySpace();

  template <typename T>
  bool To(T** obj) {
    if (IsRetry()) return false;
    *obj = T::cast(object_);
    return true;
  }

 private:
  explicit AllocationResult(AllocationSpace space)
      : object_(Smi::FromInt(static_cast<int>(space))) {}

  Object* object_;
};

STATIC_ASSERT(sizeof(AllocationResult) == kPointerSize);

#ifdef DEBUG
struct CommentStatistic {
  const char* comment;
  int size;
  int count;
  void Clear() {
    comment = NULL;
    size = 0;
    count = 0;
  }
  // Must be small, since an iteration is used for lookup.
  static const int kMaxComments = 64;
};
#endif

class NumberAndSizeInfo BASE_EMBEDDED {
 public:
  NumberAndSizeInfo() : number_(0), bytes_(0) {}

  int number() const { return number_; }
  void increment_number(int num) { number_ += num; }

  int bytes() const { return bytes_; }
  void increment_bytes(int size) { bytes_ += size; }

  void clear() {
    number_ = 0;
    bytes_ = 0;
  }

 private:
  int number_;
  int bytes_;
};

// HistogramInfo class for recording a single "bar" of a histogram.  This
// class is used for collecting statistics to print to the log file.
class HistogramInfo : public NumberAndSizeInfo {
 public:
  HistogramInfo() : NumberAndSizeInfo(), name_(nullptr) {}

  const char* name() { return name_; }
  void set_name(const char* name) { name_ = name; }

 private:
  const char* name_;
};

class Heap {
 public:
  // Declare all the root indices.  This defines the root list order.
  enum RootListIndex {
#define ROOT_INDEX_DECLARATION(type, name, camel_name) k##camel_name##RootIndex,
    STRONG_ROOT_LIST(ROOT_INDEX_DECLARATION)
#undef ROOT_INDEX_DECLARATION

#define STRING_INDEX_DECLARATION(name, str) k##name##RootIndex,
        INTERNALIZED_STRING_LIST(STRING_INDEX_DECLARATION)
#undef STRING_DECLARATION

#define SYMBOL_INDEX_DECLARATION(name) k##name##RootIndex,
            PRIVATE_SYMBOL_LIST(SYMBOL_INDEX_DECLARATION)
#undef SYMBOL_INDEX_DECLARATION

#define SYMBOL_INDEX_DECLARATION(name, description) k##name##RootIndex,
                PUBLIC_SYMBOL_LIST(SYMBOL_INDEX_DECLARATION)
                    WELL_KNOWN_SYMBOL_LIST(SYMBOL_INDEX_DECLARATION)
#undef SYMBOL_INDEX_DECLARATION

// Utility type maps
#define DECLARE_STRUCT_MAP(NAME, Name, name) k##Name##MapRootIndex,
                        STRUCT_LIST(DECLARE_STRUCT_MAP)
#undef DECLARE_STRUCT_MAP
                            kStringTableRootIndex,

#define ROOT_INDEX_DECLARATION(type, name, camel_name) k##camel_name##RootIndex,
    SMI_ROOT_LIST(ROOT_INDEX_DECLARATION)
#undef ROOT_INDEX_DECLARATION
        kRootListLength,
    kStrongRootListLength = kStringTableRootIndex,
    kSmiRootsStart = kStringTableRootIndex + 1
  };

  enum FindMementoMode { kForRuntime, kForGC };

  enum HeapState { NOT_IN_GC, SCAVENGE, MARK_COMPACT, MINOR_MARK_COMPACT };

  enum UpdateAllocationSiteMode { kGlobal, kCached };

  // Taking this mutex prevents the GC from entering a phase that relocates
  // object references.
  base::Mutex* relocation_mutex() { return &relocation_mutex_; }

  // Support for partial snapshots.  After calling this we have a linear
  // space to write objects in each space.
  struct Chunk {
    uint32_t size;
    Address start;
    Address end;
  };
  typedef std::vector<Chunk> Reservation;

  static const int kInitalOldGenerationLimitFactor = 2;

#if V8_OS_ANDROID
  // Don't apply pointer multiplier on Android since it has no swap space and
  // should instead adapt it's heap size based on available physical memory.
  static const int kPointerMultiplier = 1;
#else
  static const int kPointerMultiplier = i::kPointerSize / 4;
#endif

  // The new space size has to be a power of 2. Sizes are in MB.
  static const int kMaxSemiSpaceSizeLowMemoryDevice = 1 * kPointerMultiplier;
  static const int kMaxSemiSpaceSizeMediumMemoryDevice = 4 * kPointerMultiplier;
  static const int kMaxSemiSpaceSizeHighMemoryDevice = 8 * kPointerMultiplier;
  static const int kMaxSemiSpaceSizeHugeMemoryDevice = 8 * kPointerMultiplier;

  // The old space size has to be a multiple of Page::kPageSize.
  // Sizes are in MB.
  static const int kMaxOldSpaceSizeLowMemoryDevice = 128 * kPointerMultiplier;
  static const int kMaxOldSpaceSizeMediumMemoryDevice =
      256 * kPointerMultiplier;
  static const int kMaxOldSpaceSizeHighMemoryDevice = 512 * kPointerMultiplier;
  static const int kMaxOldSpaceSizeHugeMemoryDevice = 1024 * kPointerMultiplier;

  static const int kTraceRingBufferSize = 512;
  static const int kStacktraceBufferSize = 512;

  V8_EXPORT_PRIVATE static const double kMinHeapGrowingFactor;
  V8_EXPORT_PRIVATE static const double kMaxHeapGrowingFactor;
  static const double kMaxHeapGrowingFactorMemoryConstrained;
  static const double kMaxHeapGrowingFactorIdle;
  static const double kConservativeHeapGrowingFactor;
  static const double kTargetMutatorUtilization;

  static const int kNoGCFlags = 0;
  static const int kReduceMemoryFootprintMask = 1;
  static const int kAbortIncrementalMarkingMask = 2;
  static const int kFinalizeIncrementalMarkingMask = 4;

  // Making the heap iterable requires us to abort incremental marking.
  static const int kMakeHeapIterableMask = kAbortIncrementalMarkingMask;

  // The roots that have an index less than this are always in old space.
  static const int kOldSpaceRoots = 0x20;

  // The minimum size of a HeapObject on the heap.
  static const int kMinObjectSizeInWords = 2;

  static const int kMinPromotedPercentForFastPromotionMode = 90;

  STATIC_ASSERT(kUndefinedValueRootIndex ==
                Internals::kUndefinedValueRootIndex);
  STATIC_ASSERT(kTheHoleValueRootIndex == Internals::kTheHoleValueRootIndex);
  STATIC_ASSERT(kNullValueRootIndex == Internals::kNullValueRootIndex);
  STATIC_ASSERT(kTrueValueRootIndex == Internals::kTrueValueRootIndex);
  STATIC_ASSERT(kFalseValueRootIndex == Internals::kFalseValueRootIndex);
  STATIC_ASSERT(kempty_stringRootIndex == Internals::kEmptyStringRootIndex);

  // Calculates the maximum amount of filler that could be required by the
  // given alignment.
  static int GetMaximumFillToAlign(AllocationAlignment alignment);
  // Calculates the actual amount of filler required for a given address at the
  // given alignment.
  static int GetFillToAlign(Address address, AllocationAlignment alignment);

  template <typename T>
  static inline bool IsOneByte(T t, int chars);

  static void FatalProcessOutOfMemory(const char* location,
                                      bool is_heap_oom = false);

  V8_EXPORT_PRIVATE static bool RootIsImmortalImmovable(int root_index);

  // Checks whether the space is valid.
  static bool IsValidAllocationSpace(AllocationSpace space);

  // Generated code can embed direct references to non-writable roots if
  // they are in new space.
  static bool RootCanBeWrittenAfterInitialization(RootListIndex root_index);

  static bool IsUnmodifiedHeapObject(Object** p);

  // Zapping is needed for verify heap, and always done in debug builds.
  static inline bool ShouldZapGarbage() {
#ifdef DEBUG
    return true;
#else
#ifdef VERIFY_HEAP
    return FLAG_verify_heap;
#else
    return false;
#endif
#endif
  }

  static inline bool IsYoungGenerationCollector(GarbageCollector collector) {
    return collector == SCAVENGER || collector == MINOR_MARK_COMPACTOR;
  }

  static inline GarbageCollector YoungGenerationCollector() {
    return (FLAG_minor_mc) ? MINOR_MARK_COMPACTOR : SCAVENGER;
  }

  static inline const char* CollectorName(GarbageCollector collector) {
    switch (collector) {
      case SCAVENGER:
        return "Scavenger";
      case MARK_COMPACTOR:
        return "Mark-Compact";
      case MINOR_MARK_COMPACTOR:
        return "Minor Mark-Compact";
    }
    return "Unknown collector";
  }

  V8_EXPORT_PRIVATE static double HeapGrowingFactor(double gc_speed,
                                                    double mutator_speed);

  // Copy block of memory from src to dst. Size of block should be aligned
  // by pointer size.
  static inline void CopyBlock(Address dst, Address src, int byte_size);

  // Determines a static visitor id based on the given {map} that can then be
  // stored on the map to facilitate fast dispatch for {StaticVisitorBase}.
  static int GetStaticVisitorIdForMap(Map* map);

  // Notifies the heap that is ok to start marking or other activities that
  // should not happen during deserialization.
  void NotifyDeserializationComplete();

  inline Address* NewSpaceAllocationTopAddress();
  inline Address* NewSpaceAllocationLimitAddress();
  inline Address* OldSpaceAllocationTopAddress();
  inline Address* OldSpaceAllocationLimitAddress();

  // Clear the Instanceof cache (used when a prototype changes).
  inline void ClearInstanceofCache();

  // FreeSpace objects have a null map after deserialization. Update the map.
  void RepairFreeListsAfterDeserialization();

  // Move len elements within a given array from src_index index to dst_index
  // index.
  void MoveElements(FixedArray* array, int dst_index, int src_index, int len);

  // Initialize a filler object to keep the ability to iterate over the heap
  // when introducing gaps within pages. If slots could have been recorded in
  // the freed area, then pass ClearRecordedSlots::kYes as the mode. Otherwise,
  // pass ClearRecordedSlots::kNo.
  V8_EXPORT_PRIVATE HeapObject* CreateFillerObjectAt(Address addr, int size,
                                                     ClearRecordedSlots mode);

  bool CanMoveObjectStart(HeapObject* object);

  static bool IsImmovable(HeapObject* object);

  // Maintain consistency of live bytes during incremental marking.
  void AdjustLiveBytes(HeapObject* object, int by);

  // Trim the given array from the left. Note that this relocates the object
  // start and hence is only valid if there is only a single reference to it.
  FixedArrayBase* LeftTrimFixedArray(FixedArrayBase* obj, int elements_to_trim);

  // Trim the given array from the right.
  void RightTrimFixedArray(FixedArrayBase* obj, int elements_to_trim);

  // Converts the given boolean condition to JavaScript boolean value.
  inline Oddball* ToBoolean(bool condition);

  // Notify the heap that a context has been disposed.
  int NotifyContextDisposed(bool dependant_context);

  void set_native_contexts_list(Object* object) {
    native_contexts_list_ = object;
  }
  Object* native_contexts_list() const { return native_contexts_list_; }

  void set_allocation_sites_list(Object* object) {
    allocation_sites_list_ = object;
  }
  Object* allocation_sites_list() { return allocation_sites_list_; }

  // Used in CreateAllocationSiteStub and the (de)serializer.
  Object** allocation_sites_list_address() { return &allocation_sites_list_; }

  void set_encountered_weak_collections(Object* weak_collection) {
    encountered_weak_collections_ = weak_collection;
  }
  Object* encountered_weak_collections() const {
    return encountered_weak_collections_;
  }
  void IterateEncounteredWeakCollections(RootVisitor* visitor);

  void set_encountered_weak_cells(Object* weak_cell) {
    encountered_weak_cells_ = weak_cell;
  }
  Object* encountered_weak_cells() const { return encountered_weak_cells_; }

  void set_encountered_transition_arrays(Object* transition_array) {
    encountered_transition_arrays_ = transition_array;
  }
  Object* encountered_transition_arrays() const {
    return encountered_transition_arrays_;
  }

  // Number of mark-sweeps.
  int ms_count() const { return ms_count_; }

  // Checks whether the given object is allowed to be migrated from it's
  // current space into the given destination space. Used for debugging.
  inline bool AllowedToBeMigrated(HeapObject* object, AllocationSpace dest);

  void CheckHandleCount();

  // Number of "runtime allocations" done so far.
  uint32_t allocations_count() { return allocations_count_; }

  // Print short heap statistics.
  void PrintShortHeapStatistics();

  inline HeapState gc_state() { return gc_state_; }
  void SetGCState(HeapState state);

  inline bool IsInGCPostProcessing() { return gc_post_processing_depth_ > 0; }

  // If an object has an AllocationMemento trailing it, return it, otherwise
  // return NULL;
  template <FindMementoMode mode>
  inline AllocationMemento* FindAllocationMemento(HeapObject* object);

  // Returns false if not able to reserve.
  bool ReserveSpace(Reservation* reservations, List<Address>* maps);

  //
  // Support for the API.
  //

  bool CreateApiObjects();

  // Implements the corresponding V8 API function.
  bool IdleNotification(double deadline_in_seconds);
  bool IdleNotification(int idle_time_in_ms);

  void MemoryPressureNotification(MemoryPressureLevel level,
                                  bool is_isolate_locked);
  void CheckMemoryPressure();

  void SetOutOfMemoryCallback(v8::debug::OutOfMemoryCallback callback,
                              void* data);

  double MonotonicallyIncreasingTimeInMs();

  void RecordStats(HeapStats* stats, bool take_snapshot = false);

  // Check new space expansion criteria and expand semispaces if it was hit.
  void CheckNewSpaceExpansionCriteria();

  void VisitExternalResources(v8::ExternalResourceVisitor* visitor);

  // An object should be promoted if the object has survived a
  // scavenge operation.
  inline bool ShouldBePromoted(Address old_address, int object_size);

  void ClearNormalizedMapCaches();

  void IncrementDeferredCount(v8::Isolate::UseCounterFeature feature);

  // Completely clear the Instanceof cache (to stop it keeping objects alive
  // around a GC).
  inline void CompletelyClearInstanceofCache();

  inline uint32_t HashSeed();

  inline int NextScriptId();

  inline void SetArgumentsAdaptorDeoptPCOffset(int pc_offset);
  inline void SetConstructStubCreateDeoptPCOffset(int pc_offset);
  inline void SetConstructStubInvokeDeoptPCOffset(int pc_offset);
  inline void SetGetterStubDeoptPCOffset(int pc_offset);
  inline void SetSetterStubDeoptPCOffset(int pc_offset);
  inline void SetInterpreterEntryReturnPCOffset(int pc_offset);
  inline int GetNextTemplateSerialNumber();

  inline void SetSerializedTemplates(FixedArray* templates);
  inline void SetSerializedGlobalProxySizes(FixedArray* sizes);

  // For post mortem debugging.
  void RememberUnmappedPage(Address page, bool compacted);

  // Global inline caching age: it is incremented on some GCs after context
  // disposal. We use it to flush inline caches.
  int global_ic_age() { return global_ic_age_; }

  void AgeInlineCaches() {
    global_ic_age_ = (global_ic_age_ + 1) & SharedFunctionInfo::ICAgeBits::kMax;
  }

  int64_t external_memory_hard_limit() { return MaxOldGenerationSize() / 2; }

  int64_t external_memory() { return external_memory_; }
  void update_external_memory(int64_t delta) { external_memory_ += delta; }

  void update_external_memory_concurrently_freed(intptr_t freed) {
    external_memory_concurrently_freed_.Increment(freed);
  }

  void account_external_memory_concurrently_freed() {
    external_memory_ -= external_memory_concurrently_freed_.Value();
    external_memory_concurrently_freed_.SetValue(0);
  }

  void DeoptMarkedAllocationSites();

  inline bool DeoptMaybeTenuredAllocationSites();

  void AddWeakNewSpaceObjectToCodeDependency(Handle<HeapObject> obj,
                                             Handle<WeakCell> code);

  void AddWeakObjectToCodeDependency(Handle<HeapObject> obj,
                                     Handle<DependentCode> dep);

  DependentCode* LookupWeakObjectToCodeDependency(Handle<HeapObject> obj);

  void CompactWeakFixedArrays();

  void AddRetainedMap(Handle<Map> map);

  // This event is triggered after successful allocation of a new object made
  // by runtime. Allocations of target space for object evacuation do not
  // trigger the event. In order to track ALL allocations one must turn off
  // FLAG_inline_new and FLAG_use_allocation_folding.
  inline void OnAllocationEvent(HeapObject* object, int size_in_bytes);

  // This event is triggered after object is moved to a new place.
  inline void OnMoveEvent(HeapObject* target, HeapObject* source,
                          int size_in_bytes);

  bool deserialization_complete() const { return deserialization_complete_; }

  bool HasLowAllocationRate();
  bool HasHighFragmentation();
  bool HasHighFragmentation(size_t used, size_t committed);

  void ActivateMemoryReducerIfNeeded();

  bool ShouldOptimizeForMemoryUsage();

  bool IsLowMemoryDevice() {
    return max_old_generation_size_ <= kMaxOldSpaceSizeLowMemoryDevice;
  }

  bool IsMemoryConstrainedDevice() {
    return max_old_generation_size_ <= kMaxOldSpaceSizeMediumMemoryDevice;
  }

  bool HighMemoryPressure() {
    return memory_pressure_level_.Value() != MemoryPressureLevel::kNone;
  }

  size_t HeapLimitForDebugging() {
    const size_t kDebugHeapSizeFactor = 4;
    size_t max_limit = std::numeric_limits<size_t>::max() / 4;
    return Min(max_limit,
               initial_max_old_generation_size_ * kDebugHeapSizeFactor);
  }

  void IncreaseHeapLimitForDebugging() {
    max_old_generation_size_ =
        Max(max_old_generation_size_, HeapLimitForDebugging());
  }

  void RestoreOriginalHeapLimit() {
    // Do not set the limit lower than the live size + some slack.
    size_t min_limit = SizeOfObjects() + SizeOfObjects() / 4;
    max_old_generation_size_ =
        Min(max_old_generation_size_,
            Max(initial_max_old_generation_size_, min_limit));
  }

  bool IsHeapLimitIncreasedForDebugging() {
    return max_old_generation_size_ == HeapLimitForDebugging();
  }

  // ===========================================================================
  // Initialization. ===========================================================
  // ===========================================================================

  // Configure heap size in MB before setup. Return false if the heap has been
  // set up already.
  bool ConfigureHeap(size_t max_semi_space_size, size_t max_old_space_size,
                     size_t code_range_size);
  bool ConfigureHeapDefault();

  // Prepares the heap, setting up memory areas that are needed in the isolate
  // without actually creating any objects.
  bool SetUp();

  // (Re-)Initialize hash seed from flag or RNG.
  void InitializeHashSeed();

  // Bootstraps the object heap with the core set of objects required to run.
  // Returns whether it succeeded.
  bool CreateHeapObjects();

  // Create ObjectStats if live_object_stats_ or dead_object_stats_ are nullptr.
  V8_INLINE void CreateObjectStats();

  // Destroys all memory allocated by the heap.
  void TearDown();

  // Returns whether SetUp has been called.
  bool HasBeenSetUp();

  // ===========================================================================
  // Getters for spaces. =======================================================
  // ===========================================================================

  inline Address NewSpaceTop();

  NewSpace* new_space() { return new_space_; }
  OldSpace* old_space() { return old_space_; }
  OldSpace* code_space() { return code_space_; }
  MapSpace* map_space() { return map_space_; }
  LargeObjectSpace* lo_space() { return lo_space_; }

  inline PagedSpace* paged_space(int idx);
  inline Space* space(int idx);

  // Returns name of the space.
  const char* GetSpaceName(int idx);

  // ===========================================================================
  // Getters to other components. ==============================================
  // ===========================================================================

  GCTracer* tracer() { return tracer_; }

  MemoryAllocator* memory_allocator() { return memory_allocator_; }

  PromotionQueue* promotion_queue() { return &promotion_queue_; }

  inline Isolate* isolate();

  MarkCompactCollector* mark_compact_collector() {
    return mark_compact_collector_;
  }

  MinorMarkCompactCollector* minor_mark_compact_collector() {
    return minor_mark_compact_collector_;
  }

  // ===========================================================================
  // Root set access. ==========================================================
  // ===========================================================================

  // Heap root getters.
#define ROOT_ACCESSOR(type, name, camel_name) inline type* name();
  ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

  // Utility type maps.
#define STRUCT_MAP_ACCESSOR(NAME, Name, name) inline Map* name##_map();
  STRUCT_LIST(STRUCT_MAP_ACCESSOR)
#undef STRUCT_MAP_ACCESSOR

#define STRING_ACCESSOR(name, str) inline String* name();
  INTERNALIZED_STRING_LIST(STRING_ACCESSOR)
#undef STRING_ACCESSOR

#define SYMBOL_ACCESSOR(name) inline Symbol* name();
  PRIVATE_SYMBOL_LIST(SYMBOL_ACCESSOR)
#undef SYMBOL_ACCESSOR

#define SYMBOL_ACCESSOR(name, description) inline Symbol* name();
  PUBLIC_SYMBOL_LIST(SYMBOL_ACCESSOR)
  WELL_KNOWN_SYMBOL_LIST(SYMBOL_ACCESSOR)
#undef SYMBOL_ACCESSOR

  Object* root(RootListIndex index) { return roots_[index]; }
  Handle<Object> root_handle(RootListIndex index) {
    return Handle<Object>(&roots_[index]);
  }
  template <typename T>
  bool IsRootHandle(Handle<T> handle, RootListIndex* index) const {
    Object** const handle_location = bit_cast<Object**>(handle.address());
    if (handle_location >= &roots_[kRootListLength]) return false;
    if (handle_location < &roots_[0]) return false;
    *index = static_cast<RootListIndex>(handle_location - &roots_[0]);
    return true;
  }

  // Generated code can embed this address to get access to the roots.
  Object** roots_array_start() { return roots_; }

  // Sets the stub_cache_ (only used when expanding the dictionary).
  void SetRootCodeStubs(UnseededNumberDictionary* value);

  void SetRootMaterializedObjects(FixedArray* objects) {
    roots_[kMaterializedObjectsRootIndex] = objects;
  }

  void SetRootScriptList(Object* value) {
    roots_[kScriptListRootIndex] = value;
  }

  void SetRootStringTable(StringTable* value) {
    roots_[kStringTableRootIndex] = value;
  }

  void SetRootNoScriptSharedFunctionInfos(Object* value) {
    roots_[kNoScriptSharedFunctionInfosRootIndex] = value;
  }

  void SetMessageListeners(TemplateList* value) {
    roots_[kMessageListenersRootIndex] = value;
  }

  // Set the stack limit in the roots_ array.  Some architectures generate
  // code that looks here, because it is faster than loading from the static
  // jslimit_/real_jslimit_ variable in the StackGuard.
  void SetStackLimits();

  // The stack limit is thread-dependent. To be able to reproduce the same
  // snapshot blob, we need to reset it before serializing.
  void ClearStackLimits();

  // Generated code can treat direct references to this root as constant.
  bool RootCanBeTreatedAsConstant(RootListIndex root_index);

  Map* MapForFixedTypedArray(ExternalArrayType array_type);
  RootListIndex RootIndexForFixedTypedArray(ExternalArrayType array_type);

  RootListIndex RootIndexForEmptyFixedTypedArray(ElementsKind kind);
  FixedTypedArrayBase* EmptyFixedTypedArrayForMap(Map* map);

  void RegisterStrongRoots(Object** start, Object** end);
  void UnregisterStrongRoots(Object** start);

  // ===========================================================================
  // Inline allocation. ========================================================
  // ===========================================================================

  // Indicates whether inline bump-pointer allocation has been disabled.
  bool inline_allocation_disabled() { return inline_allocation_disabled_; }

  // Switch whether inline bump-pointer allocation should be used.
  void EnableInlineAllocation();
  void DisableInlineAllocation();

  // ===========================================================================
  // Methods triggering GCs. ===================================================
  // ===========================================================================

  // Performs garbage collection operation.
  // Returns whether there is a chance that another major GC could
  // collect more garbage.
  inline bool CollectGarbage(
      AllocationSpace space, GarbageCollectionReason gc_reason,
      const GCCallbackFlags gc_callback_flags = kNoGCCallbackFlags);

  // Performs a full garbage collection.  If (flags & kMakeHeapIterableMask) is
  // non-zero, then the slower precise sweeper is used, which leaves the heap
  // in a state where we can iterate over the heap visiting all objects.
  void CollectAllGarbage(
      int flags, GarbageCollectionReason gc_reason,
      const GCCallbackFlags gc_callback_flags = kNoGCCallbackFlags);

  // Last hope GC, should try to squeeze as much as possible.
  void CollectAllAvailableGarbage(GarbageCollectionReason gc_reason);

  // Reports and external memory pressure event, either performs a major GC or
  // completes incremental marking in order to free external resources.
  void ReportExternalMemoryPressure();

  // Invoked when GC was requested via the stack guard.
  void HandleGCRequest();

  // ===========================================================================
  // Iterators. ================================================================
  // ===========================================================================

  // Iterates over all roots in the heap.
  void IterateRoots(RootVisitor* v, VisitMode mode);
  // Iterates over all strong roots in the heap.
  void IterateStrongRoots(RootVisitor* v, VisitMode mode);
  // Iterates over entries in the smi roots list.  Only interesting to the
  // serializer/deserializer, since GC does not care about smis.
  void IterateSmiRoots(RootVisitor* v);
  // Iterates over all the other roots in the heap.
  void IterateWeakRoots(RootVisitor* v, VisitMode mode);

  // Iterate pointers of promoted objects.
  void IterateAndScavengePromotedObject(HeapObject* target, int size);

  // ===========================================================================
  // Store buffer API. =========================================================
  // ===========================================================================

  // Write barrier support for object[offset] = o;
  inline void RecordWrite(Object* object, int offset, Object* o);
  inline void RecordWriteIntoCode(Code* host, RelocInfo* rinfo, Object* target);
  void RecordWriteIntoCodeSlow(Code* host, RelocInfo* rinfo, Object* target);
  void RecordWritesIntoCode(Code* code);
  inline void RecordFixedArrayElements(FixedArray* array, int offset,
                                       int length);

  inline Address* store_buffer_top_address();

  void ClearRecordedSlot(HeapObject* object, Object** slot);
  void ClearRecordedSlotRange(Address start, Address end);

  bool HasRecordedSlot(HeapObject* object, Object** slot);

  // ===========================================================================
  // Incremental marking API. ==================================================
  // ===========================================================================

  // Start incremental marking and ensure that idle time handler can perform
  // incremental steps.
  void StartIdleIncrementalMarking(
      GarbageCollectionReason gc_reason,
      GCCallbackFlags gc_callback_flags = GCCallbackFlags::kNoGCCallbackFlags);

  // Starts incremental marking assuming incremental marking is currently
  // stopped.
  void StartIncrementalMarking(
      int gc_flags, GarbageCollectionReason gc_reason,
      GCCallbackFlags gc_callback_flags = GCCallbackFlags::kNoGCCallbackFlags);

  void StartIncrementalMarkingIfAllocationLimitIsReached(
      int gc_flags,
      GCCallbackFlags gc_callback_flags = GCCallbackFlags::kNoGCCallbackFlags);

  void FinalizeIncrementalMarkingIfComplete(GarbageCollectionReason gc_reason);

  void RegisterDeserializedObjectsForBlackAllocation(
      Reservation* reservations, List<HeapObject*>* large_objects);

  IncrementalMarking* incremental_marking() { return incremental_marking_; }

  // ===========================================================================
  // Concurrent marking API. ===================================================
  // ===========================================================================

  ConcurrentMarking* concurrent_marking() { return concurrent_marking_; }

  // The runtime uses this function to notify potentially unsafe object layout
  // changes that require special synchronization with the concurrent marker.
  void NotifyObjectLayoutChange(HeapObject* object,
                                const DisallowHeapAllocation&);

#ifdef VERIFY_HEAP
  // This function checks that either
  // - the map transition is safe,
  // - or it was communicated to GC using NotifyObjectLayoutChange.
  void VerifyObjectLayoutChange(HeapObject* object, Map* new_map);
#endif

  // ===========================================================================
  // Embedder heap tracer support. =============================================
  // ===========================================================================

  LocalEmbedderHeapTracer* local_embedder_heap_tracer() {
    return local_embedder_heap_tracer_;
  }
  void SetEmbedderHeapTracer(EmbedderHeapTracer* tracer);
  void TracePossibleWrapper(JSObject* js_object);
  void RegisterExternallyReferencedObject(Object** object);

  // ===========================================================================
  // External string table API. ================================================
  // ===========================================================================

  // Registers an external string.
  inline void RegisterExternalString(String* string);

  // Finalizes an external string by deleting the associated external
  // data and clearing the resource pointer.
  inline void FinalizeExternalString(String* string);

  // ===========================================================================
  // Methods checking/returning the space of a given object/address. ===========
  // ===========================================================================

  // Returns whether the object resides in new space.
  inline bool InNewSpace(Object* object);
  inline bool InFromSpace(Object* object);
  inline bool InToSpace(Object* object);

  // Returns whether the object resides in old space.
  inline bool InOldSpace(Object* object);

  // Checks whether an address/object in the heap (including auxiliary
  // area and unused area).
  bool Contains(HeapObject* value);

  // Checks whether an address/object in a space.
  // Currently used by tests, serialization and heap verification only.
  bool InSpace(HeapObject* value, AllocationSpace space);

  // Slow methods that can be used for verification as they can also be used
  // with off-heap Addresses.
  bool ContainsSlow(Address addr);
  bool InSpaceSlow(Address addr, AllocationSpace space);
  inline bool InNewSpaceSlow(Address address);
  inline bool InOldSpaceSlow(Address address);

  // ===========================================================================
  // Object statistics tracking. ===============================================
  // ===========================================================================

  // Returns the number of buckets used by object statistics tracking during a
  // major GC. Note that the following methods fail gracefully when the bounds
  // are exceeded though.
  size_t NumberOfTrackedHeapObjectTypes();

  // Returns object statistics about count and size at the last major GC.
  // Objects are being grouped into buckets that roughly resemble existing
  // instance types.
  size_t ObjectCountAtLastGC(size_t index);
  size_t ObjectSizeAtLastGC(size_t index);

  // Retrieves names of buckets used by object statistics tracking.
  bool GetObjectTypeName(size_t index, const char** object_type,
                         const char** object_sub_type);

  // ===========================================================================
  // Code statistics. ==========================================================
  // ===========================================================================

  // Collect code (Code and BytecodeArray objects) statistics.
  void CollectCodeStatistics();

  // ===========================================================================
  // GC statistics. ============================================================
  // ===========================================================================

  // Returns the maximum amount of memory reserved for the heap.
  size_t MaxReserved() {
    return 2 * max_semi_space_size_ + max_old_generation_size_;
  }
  size_t MaxSemiSpaceSize() { return max_semi_space_size_; }
  size_t InitialSemiSpaceSize() { return initial_semispace_size_; }
  size_t MaxOldGenerationSize() { return max_old_generation_size_; }

  // Returns the capacity of the heap in bytes w/o growing. Heap grows when
  // more spaces are needed until it reaches the limit.
  size_t Capacity();

  // Returns the capacity of the old generation.
  size_t OldGenerationCapacity();

  // Returns the amount of memory currently committed for the heap.
  size_t CommittedMemory();

  // Returns the amount of memory currently committed for the old space.
  size_t CommittedOldGenerationMemory();

  // Returns the amount of executable memory currently committed for the heap.
  size_t CommittedMemoryExecutable();

  // Returns the amount of phyical memory currently committed for the heap.
  size_t CommittedPhysicalMemory();

  // Returns the maximum amount of memory ever committed for the heap.
  size_t MaximumCommittedMemory() { return maximum_committed_; }

  // Updates the maximum committed memory for the heap. Should be called
  // whenever a space grows.
  void UpdateMaximumCommitted();

  // Returns the available bytes in space w/o growing.
  // Heap doesn't guarantee that it can allocate an object that requires
  // all available bytes. Check MaxHeapObjectSize() instead.
  size_t Available();

  // Returns of size of all objects residing in the heap.
  size_t SizeOfObjects();

  void UpdateSurvivalStatistics(int start_new_space_size);

  inline void IncrementPromotedObjectsSize(size_t object_size) {
    promoted_objects_size_ += object_size;
  }
  inline size_t promoted_objects_size() { return promoted_objects_size_; }

  inline void IncrementSemiSpaceCopiedObjectSize(size_t object_size) {
    semi_space_copied_object_size_ += object_size;
  }
  inline size_t semi_space_copied_object_size() {
    return semi_space_copied_object_size_;
  }

  inline size_t SurvivedNewSpaceObjectSize() {
    return promoted_objects_size_ + semi_space_copied_object_size_;
  }

  inline void IncrementNodesDiedInNewSpace() { nodes_died_in_new_space_++; }

  inline void IncrementNodesCopiedInNewSpace() { nodes_copied_in_new_space_++; }

  inline void IncrementNodesPromoted() { nodes_promoted_++; }

  inline void IncrementYoungSurvivorsCounter(size_t survived) {
    survived_last_scavenge_ = survived;
    survived_since_last_expansion_ += survived;
  }

  inline uint64_t PromotedTotalSize() {
    return PromotedSpaceSizeOfObjects() + PromotedExternalMemorySize();
  }

  inline void UpdateNewSpaceAllocationCounter();

  inline size_t NewSpaceAllocationCounter();

  // This should be used only for testing.
  void set_new_space_allocation_counter(size_t new_value) {
    new_space_allocation_counter_ = new_value;
  }

  void UpdateOldGenerationAllocationCounter() {
    old_generation_allocation_counter_at_last_gc_ =
        OldGenerationAllocationCounter();
  }

  size_t OldGenerationAllocationCounter() {
    return old_generation_allocation_counter_at_last_gc_ +
           PromotedSinceLastGC();
  }

  // This should be used only for testing.
  void set_old_generation_allocation_counter_at_last_gc(size_t new_value) {
    old_generation_allocation_counter_at_last_gc_ = new_value;
  }

  size_t PromotedSinceLastGC() {
    return PromotedSpaceSizeOfObjects() - old_generation_size_at_last_gc_;
  }

  int gc_count() const { return gc_count_; }

  // Returns the size of objects residing in non new spaces.
  size_t PromotedSpaceSizeOfObjects();

  // ===========================================================================
  // Prologue/epilogue callback methods.========================================
  // ===========================================================================

  void AddGCPrologueCallback(v8::Isolate::GCCallback callback,
                             GCType gc_type_filter, bool pass_isolate = true);
  void RemoveGCPrologueCallback(v8::Isolate::GCCallback callback);

  void AddGCEpilogueCallback(v8::Isolate::GCCallback callback,
                             GCType gc_type_filter, bool pass_isolate = true);
  void RemoveGCEpilogueCallback(v8::Isolate::GCCallback callback);

  void CallGCPrologueCallbacks(GCType gc_type, GCCallbackFlags flags);
  void CallGCEpilogueCallbacks(GCType gc_type, GCCallbackFlags flags);

  // ===========================================================================
  // Allocation methods. =======================================================
  // ===========================================================================

  // Creates a filler object and returns a heap object immediately after it.
  MUST_USE_RESULT HeapObject* PrecedeWithFiller(HeapObject* object,
                                                int filler_size);

  // Creates a filler object if needed for alignment and returns a heap object
  // immediately after it. If any space is left after the returned object,
  // another filler object is created so the over allocated memory is iterable.
  MUST_USE_RESULT HeapObject* AlignWithFiller(HeapObject* object,
                                              int object_size,
                                              int allocation_size,
                                              AllocationAlignment alignment);

  // ===========================================================================
  // ArrayBuffer tracking. =====================================================
  // ===========================================================================

  // TODO(gc): API usability: encapsulate mutation of JSArrayBuffer::is_external
  // in the registration/unregistration APIs. Consider dropping the "New" from
  // "RegisterNewArrayBuffer" because one can re-register a previously
  // unregistered buffer, too, and the name is confusing.
  void RegisterNewArrayBuffer(JSArrayBuffer* buffer);
  void UnregisterArrayBuffer(JSArrayBuffer* buffer);

  // ===========================================================================
  // Allocation site tracking. =================================================
  // ===========================================================================

  // Updates the AllocationSite of a given {object}. If the global prenuring
  // storage is passed as {pretenuring_feedback} the memento found count on
  // the corresponding allocation site is immediately updated and an entry
  // in the hash map is created. Otherwise the entry (including a the count
  // value) is cached on the local pretenuring feedback.
  template <UpdateAllocationSiteMode mode>
  inline void UpdateAllocationSite(HeapObject* object,
                                   base::HashMap* pretenuring_feedback);

  // Removes an entry from the global pretenuring storage.
  inline void RemoveAllocationSitePretenuringFeedback(AllocationSite* site);

  // Merges local pretenuring feedback into the global one. Note that this
  // method needs to be called after evacuation, as allocation sites may be
  // evacuated and this method resolves forward pointers accordingly.
  void MergeAllocationSitePretenuringFeedback(
      const base::HashMap& local_pretenuring_feedback);

// =============================================================================

#ifdef VERIFY_HEAP
  // Verify the heap is in its normal state before or after a GC.
  void Verify();
  void VerifyRememberedSetFor(HeapObject* object);
#endif

#ifdef DEBUG
  void set_allocation_timeout(int timeout) { allocation_timeout_ = timeout; }

  void Print();
  void PrintHandles();

  // Report heap statistics.
  void ReportHeapStatistics(const char* title);
  void ReportCodeStatistics(const char* title);
#endif

  static const char* GarbageCollectionReasonToString(
      GarbageCollectionReason gc_reason);

 private:
  class SkipStoreBufferScope;
  class PretenuringScope;

  // External strings table is a place where all external strings are
  // registered.  We need to keep track of such strings to properly
  // finalize them.
  class ExternalStringTable {
   public:
    // Registers an external string.
    inline void AddString(String* string);

    inline void IterateAll(RootVisitor* v);
    inline void IterateNewSpaceStrings(RootVisitor* v);
    inline void PromoteAllNewSpaceStrings();

    // Restores internal invariant and gets rid of collected strings. Must be
    // called after each Iterate*() that modified the strings.
    void CleanUpAll();
    void CleanUpNewSpaceStrings();

    // Destroys all allocated memory.
    void TearDown();

   private:
    explicit ExternalStringTable(Heap* heap) : heap_(heap) {}

    inline void Verify();

    inline void AddOldString(String* string);

    // Notifies the table that only a prefix of the new list is valid.
    inline void ShrinkNewStrings(int position);

    // To speed up scavenge collections new space string are kept
    // separate from old space strings.
    List<Object*> new_space_strings_;
    List<Object*> old_space_strings_;

    Heap* heap_;

    friend class Heap;

    DISALLOW_COPY_AND_ASSIGN(ExternalStringTable);
  };

  struct StrongRootsList;

  struct StringTypeTable {
    InstanceType type;
    int size;
    RootListIndex index;
  };

  struct ConstantStringTable {
    const char* contents;
    RootListIndex index;
  };

  struct StructTable {
    InstanceType type;
    int size;
    RootListIndex index;
  };

  struct GCCallbackPair {
    GCCallbackPair(v8::Isolate::GCCallback callback, GCType gc_type,
                   bool pass_isolate)
        : callback(callback), gc_type(gc_type), pass_isolate(pass_isolate) {}

    bool operator==(const GCCallbackPair& other) const {
      return other.callback == callback;
    }

    v8::Isolate::GCCallback callback;
    GCType gc_type;
    bool pass_isolate;
  };

  typedef String* (*ExternalStringTableUpdaterCallback)(Heap* heap,
                                                        Object** pointer);

  static const int kInitialStringTableSize = 2048;
  static const int kInitialEvalCacheSize = 64;
  static const int kInitialNumberStringCacheSize = 256;

  static const int kRememberedUnmappedPages = 128;

  static const StringTypeTable string_type_table[];
  static const ConstantStringTable constant_string_table[];
  static const StructTable struct_table[];

  static const int kYoungSurvivalRateHighThreshold = 90;
  static const int kYoungSurvivalRateAllowedDeviation = 15;
  static const int kOldSurvivalRateLowThreshold = 10;

  static const int kMaxMarkCompactsInIdleRound = 7;
  static const int kIdleScavengeThreshold = 5;

  static const int kInitialFeedbackCapacity = 256;

  Heap();

  static String* UpdateNewSpaceReferenceInExternalStringTableEntry(
      Heap* heap, Object** pointer);

  // Selects the proper allocation space based on the pretenuring decision.
  static AllocationSpace SelectSpace(PretenureFlag pretenure) {
    return (pretenure == TENURED) ? OLD_SPACE : NEW_SPACE;
  }

#define ROOT_ACCESSOR(type, name, camel_name) \
  inline void set_##name(type* value);
  ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

  StoreBuffer* store_buffer() { return store_buffer_; }

  void set_current_gc_flags(int flags) {
    current_gc_flags_ = flags;
    DCHECK(!ShouldFinalizeIncrementalMarking() ||
           !ShouldAbortIncrementalMarking());
  }

  inline bool ShouldReduceMemory() const {
    return (current_gc_flags_ & kReduceMemoryFootprintMask) != 0;
  }

  inline bool ShouldAbortIncrementalMarking() const {
    return (current_gc_flags_ & kAbortIncrementalMarkingMask) != 0;
  }

  inline bool ShouldFinalizeIncrementalMarking() const {
    return (current_gc_flags_ & kFinalizeIncrementalMarkingMask) != 0;
  }

  void PreprocessStackTraces();

  // Checks whether a global GC is necessary
  GarbageCollector SelectGarbageCollector(AllocationSpace space,
                                          const char** reason);

  // Make sure there is a filler value behind the top of the new space
  // so that the GC does not confuse some unintialized/stale memory
  // with the allocation memento of the object at the top
  void EnsureFillerObjectAtTop();

  // Ensure that we have swept all spaces in such a way that we can iterate
  // over all objects.  May cause a GC.
  void MakeHeapIterable();

  // Performs garbage collection operation.
  // Returns whether there is a chance that another major GC could
  // collect more garbage.
  bool CollectGarbage(
      GarbageCollector collector, GarbageCollectionReason gc_reason,
      const char* collector_reason,
      const GCCallbackFlags gc_callback_flags = kNoGCCallbackFlags);

  // Performs garbage collection
  // Returns whether there is a chance another major GC could
  // collect more garbage.
  bool PerformGarbageCollection(
      GarbageCollector collector,
      const GCCallbackFlags gc_callback_flags = kNoGCCallbackFlags);

  inline void UpdateOldSpaceLimits();

  // Initializes a JSObject based on its map.
  void InitializeJSObjectFromMap(JSObject* obj, FixedArray* properties,
                                 Map* map);

  // Initializes JSObject body starting at given offset.
  void InitializeJSObjectBody(JSObject* obj, Map* map, int start_offset);

  void InitializeAllocationMemento(AllocationMemento* memento,
                                   AllocationSite* allocation_site);

  bool CreateInitialMaps();
  void CreateInitialObjects();

  // These five Create*EntryStub functions are here and forced to not be inlined
  // because of a gcc-4.4 bug that assigns wrong vtable entries.
  NO_INLINE(void CreateJSEntryStub());
  NO_INLINE(void CreateJSConstructEntryStub());

  void CreateFixedStubs();

  HeapObject* DoubleAlignForDeserialization(HeapObject* object, int size);

  // Commits from space if it is uncommitted.
  void EnsureFromSpaceIsCommitted();

  // Uncommit unused semi space.
  bool UncommitFromSpace();

  // Fill in bogus values in from space
  void ZapFromSpace();

  // Deopts all code that contains allocation instruction which are tenured or
  // not tenured. Moreover it clears the pretenuring allocation site statistics.
  void ResetAllAllocationSitesDependentCode(PretenureFlag flag);

  // Evaluates local pretenuring for the old space and calls
  // ResetAllTenuredAllocationSitesDependentCode if too many objects died in
  // the old space.
  void EvaluateOldSpaceLocalPretenuring(uint64_t size_of_objects_before_gc);

  // Record statistics before and after garbage collection.
  void ReportStatisticsBeforeGC();
  void ReportStatisticsAfterGC();

  // Creates and installs the full-sized number string cache.
  int FullSizeNumberStringCacheLength();
  // Flush the number to string cache.
  void FlushNumberStringCache();

  void ConfigureInitialOldGenerationSize();

  bool HasLowYoungGenerationAllocationRate();
  bool HasLowOldGenerationAllocationRate();
  double YoungGenerationMutatorUtilization();
  double OldGenerationMutatorUtilization();

  void ReduceNewSpaceSize();

  GCIdleTimeHeapState ComputeHeapState();

  bool PerformIdleTimeAction(GCIdleTimeAction action,
                             GCIdleTimeHeapState heap_state,
                             double deadline_in_ms);

  void IdleNotificationEpilogue(GCIdleTimeAction action,
                                GCIdleTimeHeapState heap_state, double start_ms,
                                double deadline_in_ms);

  inline void UpdateAllocationsHash(HeapObject* object);
  inline void UpdateAllocationsHash(uint32_t value);
  void PrintAlloctionsHash();

  void AddToRingBuffer(const char* string);
  void GetFromRingBuffer(char* buffer);

  void CompactRetainedMaps(ArrayList* retained_maps);

  void CollectGarbageOnMemoryPressure();

  void InvokeOutOfMemoryCallback();

  void ComputeFastPromotionMode(double survival_rate);

  // Attempt to over-approximate the weak closure by marking object groups and
  // implicit references from global handles, but don't atomically complete
  // marking. If we continue to mark incrementally, we might have marked
  // objects that die later.
  void FinalizeIncrementalMarking(GarbageCollectionReason gc_reason);

  // Returns the timer used for a given GC type.
  // - GCScavenger: young generation GC
  // - GCCompactor: full GC
  // - GCFinalzeMC: finalization of incremental full GC
  // - GCFinalizeMCReduceMemory: finalization of incremental full GC with
  // memory reduction
  HistogramTimer* GCTypeTimer(GarbageCollector collector);

  // ===========================================================================
  // Pretenuring. ==============================================================
  // ===========================================================================

  // Pretenuring decisions are made based on feedback collected during new space
  // evacuation. Note that between feedback collection and calling this method
  // object in old space must not move.
  void ProcessPretenuringFeedback();

  // ===========================================================================
  // Actual GC. ================================================================
  // ===========================================================================

  // Code that should be run before and after each GC.  Includes some
  // reporting/verification activities when compiled with DEBUG set.
  void GarbageCollectionPrologue();
  void GarbageCollectionEpilogue();

  // Performs a major collection in the whole heap.
  void MarkCompact();
  // Performs a minor collection of just the young generation.
  void MinorMarkCompact();

  // Code to be run before and after mark-compact.
  void MarkCompactPrologue();
  void MarkCompactEpilogue();

  // Performs a minor collection in new generation.
  void Scavenge();
  void EvacuateYoungGeneration();

  Address DoScavenge(Address new_space_front);

  void UpdateNewSpaceReferencesInExternalStringTable(
      ExternalStringTableUpdaterCallback updater_func);

  void UpdateReferencesInExternalStringTable(
      ExternalStringTableUpdaterCallback updater_func);

  void ProcessAllWeakReferences(WeakObjectRetainer* retainer);
  void ProcessYoungWeakReferences(WeakObjectRetainer* retainer);
  void ProcessNativeContexts(WeakObjectRetainer* retainer);
  void ProcessAllocationSites(WeakObjectRetainer* retainer);
  void ProcessWeakListRoots(WeakObjectRetainer* retainer);

  // ===========================================================================
  // GC statistics. ============================================================
  // ===========================================================================

  inline size_t OldGenerationSpaceAvailable() {
    if (old_generation_allocation_limit_ <= PromotedTotalSize()) return 0;
    return old_generation_allocation_limit_ -
           static_cast<size_t>(PromotedTotalSize());
  }

  // We allow incremental marking to overshoot the allocation limit for
  // performace reasons. If the overshoot is too large then we are more
  // eager to finalize incremental marking.
  inline bool AllocationLimitOvershotByLargeMargin() {
    // This guards against too eager finalization in small heaps.
    // The number is chosen based on v8.browsing_mobile on Nexus 7v2.
    size_t kMarginForSmallHeaps = 32u * MB;
    if (old_generation_allocation_limit_ >= PromotedTotalSize()) return false;
    uint64_t overshoot = PromotedTotalSize() - old_generation_allocation_limit_;
    // Overshoot margin is 50% of allocation limit or half-way to the max heap
    // with special handling of small heaps.
    uint64_t margin =
        Min(Max(old_generation_allocation_limit_ / 2, kMarginForSmallHeaps),
            (max_old_generation_size_ - old_generation_allocation_limit_) / 2);
    return overshoot >= margin;
  }

  void UpdateTotalGCTime(double duration);

  bool MaximumSizeScavenge() { return maximum_size_scavenges_ > 0; }

  // ===========================================================================
  // Growing strategy. =========================================================
  // ===========================================================================

  // For some webpages RAIL mode does not switch from PERFORMANCE_LOAD.
  // This constant limits the effect of load RAIL mode on GC.
  // The value is arbitrary and chosen as the largest load time observed in
  // v8 browsing benchmarks.
  static const int kMaxLoadTimeMs = 7000;

  bool ShouldOptimizeForLoadTime();

  // Decrease the allocation limit if the new limit based on the given
  // parameters is lower than the current limit.
  void DampenOldGenerationAllocationLimit(size_t old_gen_size, double gc_speed,
                                          double mutator_speed);

  // Calculates the allocation limit based on a given growing factor and a
  // given old generation size.
  size_t CalculateOldGenerationAllocationLimit(double factor,
                                               size_t old_gen_size);

  // Sets the allocation limit to trigger the next full garbage collection.
  void SetOldGenerationAllocationLimit(size_t old_gen_size, double gc_speed,
                                       double mutator_speed);

  size_t MinimumAllocationLimitGrowingStep();

  size_t old_generation_allocation_limit() const {
    return old_generation_allocation_limit_;
  }

  bool always_allocate() { return always_allocate_scope_count_.Value() != 0; }

  bool CanExpandOldGeneration(size_t size) {
    if (force_oom_) return false;
    return (OldGenerationCapacity() + size) < MaxOldGenerationSize();
  }

  bool IsCloseToOutOfMemory(size_t slack) {
    return OldGenerationCapacity() + slack >= MaxOldGenerationSize();
  }

  bool ShouldExpandOldGenerationOnSlowAllocation();

  enum class IncrementalMarkingLimit { kNoLimit, kSoftLimit, kHardLimit };
  IncrementalMarkingLimit IncrementalMarkingLimitReached();

  // ===========================================================================
  // Idle notification. ========================================================
  // ===========================================================================

  bool RecentIdleNotificationHappened();
  void ScheduleIdleScavengeIfNeeded(int bytes_allocated);

  // ===========================================================================
  // HeapIterator helpers. =====================================================
  // ===========================================================================

  void heap_iterator_start() { heap_iterator_depth_++; }

  void heap_iterator_end() { heap_iterator_depth_--; }

  bool in_heap_iterator() { return heap_iterator_depth_ > 0; }

  // ===========================================================================
  // Allocation methods. =======================================================
  // ===========================================================================

  // Returns a deep copy of the JavaScript object.
  // Properties and elements are copied too.
  // Optionally takes an AllocationSite to be appended in an AllocationMemento.
  MUST_USE_RESULT AllocationResult CopyJSObject(JSObject* source,
                                                AllocationSite* site = NULL);

  // Allocates a JS Map in the heap.
  MUST_USE_RESULT AllocationResult
  AllocateMap(InstanceType instance_type, int instance_size,
              ElementsKind elements_kind = TERMINAL_FAST_ELEMENTS_KIND);

  // Allocates and initializes a new JavaScript object based on a
  // constructor.
  // If allocation_site is non-null, then a memento is emitted after the object
  // that points to the site.
  MUST_USE_RESULT AllocationResult AllocateJSObject(
      JSFunction* constructor, PretenureFlag pretenure = NOT_TENURED,
      AllocationSite* allocation_site = NULL);

  // Allocates and initializes a new JavaScript object based on a map.
  // Passing an allocation site means that a memento will be created that
  // points to the site.
  MUST_USE_RESULT AllocationResult
  AllocateJSObjectFromMap(Map* map, PretenureFlag pretenure = NOT_TENURED,
                          AllocationSite* allocation_site = NULL);

  // Allocates a HeapNumber from value.
  MUST_USE_RESULT AllocationResult AllocateHeapNumber(
      MutableMode mode = IMMUTABLE, PretenureFlag pretenure = NOT_TENURED);

  // Allocates a byte array of the specified length
  MUST_USE_RESULT AllocationResult
  AllocateByteArray(int length, PretenureFlag pretenure = NOT_TENURED);

  // Allocates a bytecode array with given contents.
  MUST_USE_RESULT AllocationResult
  AllocateBytecodeArray(int length, const byte* raw_bytecodes, int frame_size,
                        int parameter_count, FixedArray* constant_pool);

  MUST_USE_RESULT AllocationResult CopyCode(Code* code);

  MUST_USE_RESULT AllocationResult
  CopyBytecodeArray(BytecodeArray* bytecode_array);

  // Allocates a fixed array initialized with undefined values
  MUST_USE_RESULT AllocationResult
  AllocateFixedArray(int length, PretenureFlag pretenure = NOT_TENURED);

  // Allocate an uninitialized object.  The memory is non-executable if the
  // hardware and OS allow.  This is the single choke-point for allocations
  // performed by the runtime and should not be bypassed (to extend this to
  // inlined allocations, use the Heap::DisableInlineAllocation() support).
  MUST_USE_RESULT inline AllocationResult AllocateRaw(
      int size_in_bytes, AllocationSpace space,
      AllocationAlignment aligment = kWordAligned);

  // Allocates a heap object based on the map.
  MUST_USE_RESULT AllocationResult
      Allocate(Map* map, AllocationSpace space,
               AllocationSite* allocation_site = NULL);

  // Allocates a partial map for bootstrapping.
  MUST_USE_RESULT AllocationResult
      AllocatePartialMap(InstanceType instance_type, int instance_size);

  // Allocate a block of memory in the given space (filled with a filler).
  // Used as a fall-back for generated code when the space is full.
  MUST_USE_RESULT AllocationResult
      AllocateFillerObject(int size, bool double_align, AllocationSpace space);

  // Allocate an uninitialized fixed array.
  MUST_USE_RESULT AllocationResult
      AllocateRawFixedArray(int length, PretenureFlag pretenure);

  // Allocate an uninitialized fixed double array.
  MUST_USE_RESULT AllocationResult
      AllocateRawFixedDoubleArray(int length, PretenureFlag pretenure);

  // Allocate an initialized fixed array with the given filler value.
  MUST_USE_RESULT AllocationResult
      AllocateFixedArrayWithFiller(int length, PretenureFlag pretenure,
                                   Object* filler);

  // Allocate and partially initializes a String.  There are two String
  // encodings: one-byte and two-byte.  These functions allocate a string of
  // the given length and set its map and length fields.  The characters of
  // the string are uninitialized.
  MUST_USE_RESULT AllocationResult
      AllocateRawOneByteString(int length, PretenureFlag pretenure);
  MUST_USE_RESULT AllocationResult
      AllocateRawTwoByteString(int length, PretenureFlag pretenure);

  // Allocates an internalized string in old space based on the character
  // stream.
  MUST_USE_RESULT inline AllocationResult AllocateInternalizedStringFromUtf8(
      Vector<const char> str, int chars, uint32_t hash_field);

  MUST_USE_RESULT inline AllocationResult AllocateOneByteInternalizedString(
      Vector<const uint8_t> str, uint32_t hash_field);

  MUST_USE_RESULT inline AllocationResult AllocateTwoByteInternalizedString(
      Vector<const uc16> str, uint32_t hash_field);

  template <bool is_one_byte, typename T>
  MUST_USE_RESULT AllocationResult
      AllocateInternalizedStringImpl(T t, int chars, uint32_t hash_field);

  template <typename T>
  MUST_USE_RESULT inline AllocationResult AllocateInternalizedStringImpl(
      T t, int chars, uint32_t hash_field);

  // Allocates an uninitialized fixed array. It must be filled by the caller.
  MUST_USE_RESULT AllocationResult AllocateUninitializedFixedArray(int length);

  // Make a copy of src and return it.
  MUST_USE_RESULT inline AllocationResult CopyFixedArray(FixedArray* src);

  // Make a copy of src, also grow the copy, and return the copy.
  MUST_USE_RESULT AllocationResult
  CopyFixedArrayAndGrow(FixedArray* src, int grow_by, PretenureFlag pretenure);

  // Make a copy of src, also grow the copy, and return the copy.
  MUST_USE_RESULT AllocationResult CopyFixedArrayUpTo(FixedArray* src,
                                                      int new_len,
                                                      PretenureFlag pretenure);

  // Make a copy of src, set the map, and return the copy.
  MUST_USE_RESULT AllocationResult
      CopyFixedArrayWithMap(FixedArray* src, Map* map);

  // Make a copy of src and return it.
  MUST_USE_RESULT inline AllocationResult CopyFixedDoubleArray(
      FixedDoubleArray* src);

  // Computes a single character string where the character has code.
  // A cache is used for one-byte (Latin1) codes.
  MUST_USE_RESULT AllocationResult
      LookupSingleCharacterStringFromCode(uint16_t code);

  // Allocate a symbol in old space.
  MUST_USE_RESULT AllocationResult AllocateSymbol();

  // Allocates an external array of the specified length and type.
  MUST_USE_RESULT AllocationResult AllocateFixedTypedArrayWithExternalPointer(
      int length, ExternalArrayType array_type, void* external_pointer,
      PretenureFlag pretenure);

  // Allocates a fixed typed array of the specified length and type.
  MUST_USE_RESULT AllocationResult
  AllocateFixedTypedArray(int length, ExternalArrayType array_type,
                          bool initialize, PretenureFlag pretenure);

  // Make a copy of src and return it.
  MUST_USE_RESULT AllocationResult CopyAndTenureFixedCOWArray(FixedArray* src);

  // Make a copy of src, set the map, and return the copy.
  MUST_USE_RESULT AllocationResult
      CopyFixedDoubleArrayWithMap(FixedDoubleArray* src, Map* map);

  // Allocates a fixed double array with uninitialized values. Returns
  MUST_USE_RESULT AllocationResult AllocateUninitializedFixedDoubleArray(
      int length, PretenureFlag pretenure = NOT_TENURED);

  // Allocate empty fixed array.
  MUST_USE_RESULT AllocationResult AllocateEmptyFixedArray();

  // Allocate empty scope info.
  MUST_USE_RESULT AllocationResult AllocateEmptyScopeInfo();

  // Allocate empty fixed typed array of given type.
  MUST_USE_RESULT AllocationResult
      AllocateEmptyFixedTypedArray(ExternalArrayType array_type);

  // Allocate a tenured simple cell.
  MUST_USE_RESULT AllocationResult AllocateCell(Object* value);

  // Allocate a tenured JS global property cell initialized with the hole.
  MUST_USE_RESULT AllocationResult AllocatePropertyCell();

  MUST_USE_RESULT AllocationResult AllocateWeakCell(HeapObject* value);

  MUST_USE_RESULT AllocationResult AllocateTransitionArray(int capacity);

  // Allocates a new utility object in the old generation.
  MUST_USE_RESULT AllocationResult AllocateStruct(InstanceType type);

  // Allocates a new foreign object.
  MUST_USE_RESULT AllocationResult
      AllocateForeign(Address address, PretenureFlag pretenure = NOT_TENURED);

  MUST_USE_RESULT AllocationResult
      AllocateCode(int object_size, bool immovable);

  // ===========================================================================

  void set_force_oom(bool value) { force_oom_ = value; }

  // The amount of external memory registered through the API.
  int64_t external_memory_;

  // The limit when to trigger memory pressure from the API.
  int64_t external_memory_limit_;

  // Caches the amount of external memory registered at the last MC.
  int64_t external_memory_at_last_mark_compact_;

  // The amount of memory that has been freed concurrently.
  base::AtomicNumber<intptr_t> external_memory_concurrently_freed_;

  // This can be calculated directly from a pointer to the heap; however, it is
  // more expedient to get at the isolate directly from within Heap methods.
  Isolate* isolate_;

  Object* roots_[kRootListLength];

  size_t code_range_size_;
  size_t max_semi_space_size_;
  size_t initial_semispace_size_;
  size_t max_old_generation_size_;
  size_t initial_max_old_generation_size_;
  size_t initial_old_generation_size_;
  bool old_generation_size_configured_;
  size_t maximum_committed_;

  // For keeping track of how much data has survived
  // scavenge since last new space expansion.
  size_t survived_since_last_expansion_;

  // ... and since the last scavenge.
  size_t survived_last_scavenge_;

  // This is not the depth of nested AlwaysAllocateScope's but rather a single
  // count, as scopes can be acquired from multiple tasks (read: threads).
  base::AtomicNumber<size_t> always_allocate_scope_count_;

  // Stores the memory pressure level that set by MemoryPressureNotification
  // and reset by a mark-compact garbage collection.
  base::AtomicValue<MemoryPressureLevel> memory_pressure_level_;

  v8::debug::OutOfMemoryCallback out_of_memory_callback_;
  void* out_of_memory_callback_data_;

  // For keeping track of context disposals.
  int contexts_disposed_;

  // The length of the retained_maps array at the time of context disposal.
  // This separates maps in the retained_maps array that were created before
  // and after context disposal.
  int number_of_disposed_maps_;

  int global_ic_age_;

  NewSpace* new_space_;
  OldSpace* old_space_;
  OldSpace* code_space_;
  MapSpace* map_space_;
  LargeObjectSpace* lo_space_;
  // Map from the space id to the space.
  Space* space_[LAST_SPACE + 1];
  HeapState gc_state_;
  int gc_post_processing_depth_;
  Address new_space_top_after_last_gc_;

  // Returns the amount of external memory registered since last global gc.
  uint64_t PromotedExternalMemorySize();

  // How many "runtime allocations" happened.
  uint32_t allocations_count_;

  // Running hash over allocations performed.
  uint32_t raw_allocations_hash_;

  // How many mark-sweep collections happened.
  unsigned int ms_count_;

  // How many gc happened.
  unsigned int gc_count_;

  // For post mortem debugging.
  int remembered_unmapped_pages_index_;
  Address remembered_unmapped_pages_[kRememberedUnmappedPages];

#ifdef DEBUG
  // If the --gc-interval flag is set to a positive value, this
  // variable holds the value indicating the number of allocations
  // remain until the next failure and garbage collection.
  int allocation_timeout_;
#endif  // DEBUG

  // Limit that triggers a global GC on the next (normally caused) GC.  This
  // is checked when we have already decided to do a GC to help determine
  // which collector to invoke, before expanding a paged space in the old
  // generation and on every allocation in large object space.
  size_t old_generation_allocation_limit_;

  // Indicates that inline bump-pointer allocation has been globally disabled
  // for all spaces. This is used to disable allocations in generated code.
  bool inline_allocation_disabled_;

  // Weak list heads, threaded through the objects.
  // List heads are initialized lazily and contain the undefined_value at start.
  Object* native_contexts_list_;
  Object* allocation_sites_list_;

  // List of encountered weak collections (JSWeakMap and JSWeakSet) during
  // marking. It is initialized during marking, destroyed after marking and
  // contains Smi(0) while marking is not active.
  Object* encountered_weak_collections_;

  Object* encountered_weak_cells_;

  Object* encountered_transition_arrays_;

  List<GCCallbackPair> gc_epilogue_callbacks_;
  List<GCCallbackPair> gc_prologue_callbacks_;

  int deferred_counters_[v8::Isolate::kUseCounterFeatureCount];

  GCTracer* tracer_;

  size_t promoted_objects_size_;
  double promotion_ratio_;
  double promotion_rate_;
  size_t semi_space_copied_object_size_;
  size_t previous_semi_space_copied_object_size_;
  double semi_space_copied_rate_;
  int nodes_died_in_new_space_;
  int nodes_copied_in_new_space_;
  int nodes_promoted_;

  // This is the pretenuring trigger for allocation sites that are in maybe
  // tenure state. When we switched to the maximum new space size we deoptimize
  // the code that belongs to the allocation site and derive the lifetime
  // of the allocation site.
  unsigned int maximum_size_scavenges_;

  // Total time spent in GC.
  double total_gc_time_ms_;

  // Last time an idle notification happened.
  double last_idle_notification_time_;

  // Last time a garbage collection happened.
  double last_gc_time_;

  Scavenger* scavenge_collector_;

  MarkCompactCollector* mark_compact_collector_;
  MinorMarkCompactCollector* minor_mark_compact_collector_;

  MemoryAllocator* memory_allocator_;

  StoreBuffer* store_buffer_;

  IncrementalMarking* incremental_marking_;
  ConcurrentMarking* concurrent_marking_;

  GCIdleTimeHandler* gc_idle_time_handler_;

  MemoryReducer* memory_reducer_;

  ObjectStats* live_object_stats_;
  ObjectStats* dead_object_stats_;

  ScavengeJob* scavenge_job_;

  AllocationObserver* idle_scavenge_observer_;

  // This counter is increased before each GC and never reset.
  // To account for the bytes allocated since the last GC, use the
  // NewSpaceAllocationCounter() function.
  size_t new_space_allocation_counter_;

  // This counter is increased before each GC and never reset. To
  // account for the bytes allocated since the last GC, use the
  // OldGenerationAllocationCounter() function.
  size_t old_generation_allocation_counter_at_last_gc_;

  // The size of objects in old generation after the last MarkCompact GC.
  size_t old_generation_size_at_last_gc_;

  // If the --deopt_every_n_garbage_collections flag is set to a positive value,
  // this variable holds the number of garbage collections since the last
  // deoptimization triggered by garbage collection.
  int gcs_since_last_deopt_;

  // The feedback storage is used to store allocation sites (keys) and how often
  // they have been visited (values) by finding a memento behind an object. The
  // storage is only alive temporary during a GC. The invariant is that all
  // pointers in this map are already fixed, i.e., they do not point to
  // forwarding pointers.
  base::HashMap* global_pretenuring_feedback_;

  char trace_ring_buffer_[kTraceRingBufferSize];
  // If it's not full then the data is from 0 to ring_buffer_end_.  If it's
  // full then the data is from ring_buffer_end_ to the end of the buffer and
  // from 0 to ring_buffer_end_.
  bool ring_buffer_full_;
  size_t ring_buffer_end_;

  // Shared state read by the scavenge collector and set by ScavengeObject.
  PromotionQueue promotion_queue_;

  // Flag is set when the heap has been configured.  The heap can be repeatedly
  // configured through the API until it is set up.
  bool configured_;

  // Currently set GC flags that are respected by all GC components.
  int current_gc_flags_;

  // Currently set GC callback flags that are used to pass information between
  // the embedder and V8's GC.
  GCCallbackFlags current_gc_callback_flags_;

  ExternalStringTable external_string_table_;

  base::Mutex relocation_mutex_;

  int gc_callbacks_depth_;

  bool deserialization_complete_;

  StrongRootsList* strong_roots_list_;

  // The depth of HeapIterator nestings.
  int heap_iterator_depth_;

  LocalEmbedderHeapTracer* local_embedder_heap_tracer_;

  bool fast_promotion_mode_;

  // Used for testing purposes.
  bool force_oom_;
  bool delay_sweeper_tasks_for_testing_;

  HeapObject* pending_layout_change_object_;

  // Classes in "heap" can be friends.
  friend class AlwaysAllocateScope;
  friend class ConcurrentMarking;
  friend class GCCallbacksScope;
  friend class GCTracer;
  friend class HeapIterator;
  friend class IdleScavengeObserver;
  friend class IncrementalMarking;
  friend class IncrementalMarkingJob;
  friend class LargeObjectSpace;
  friend class MarkCompactCollector;
  friend class MarkCompactCollectorBase;
  friend class MinorMarkCompactCollector;
  friend class MarkCompactMarkingVisitor;
  friend class NewSpace;
  friend class ObjectStatsCollector;
  friend class Page;
  friend class PagedSpace;
  friend class Scavenger;
  friend class StoreBuffer;
  friend class TestMemoryAllocatorScope;

  // The allocator interface.
  friend class Factory;

  // The Isolate constructs us.
  friend class Isolate;

  // Used in cctest.
  friend class HeapTester;

  DISALLOW_COPY_AND_ASSIGN(Heap);
};


class HeapStats {
 public:
  static const int kStartMarker = 0xDECADE00;
  static const int kEndMarker = 0xDECADE01;

  intptr_t* start_marker;                  //  0
  size_t* new_space_size;                  //  1
  size_t* new_space_capacity;              //  2
  size_t* old_space_size;                  //  3
  size_t* old_space_capacity;              //  4
  size_t* code_space_size;                 //  5
  size_t* code_space_capacity;             //  6
  size_t* map_space_size;                  //  7
  size_t* map_space_capacity;              //  8
  size_t* lo_space_size;                   //  9
  size_t* global_handle_count;             // 10
  size_t* weak_global_handle_count;        // 11
  size_t* pending_global_handle_count;     // 12
  size_t* near_death_global_handle_count;  // 13
  size_t* free_global_handle_count;        // 14
  size_t* memory_allocator_size;           // 15
  size_t* memory_allocator_capacity;       // 16
  size_t* malloced_memory;                 // 17
  size_t* malloced_peak_memory;            // 18
  size_t* objects_per_type;                // 19
  size_t* size_per_type;                   // 20
  int* os_error;                           // 21
  char* last_few_messages;                 // 22
  char* js_stacktrace;                     // 23
  intptr_t* end_marker;                    // 24
};


class AlwaysAllocateScope {
 public:
  explicit inline AlwaysAllocateScope(Isolate* isolate);
  inline ~AlwaysAllocateScope();

 private:
  Heap* heap_;
};


// Visitor class to verify interior pointers in spaces that do not contain
// or care about intergenerational references. All heap object pointers have to
// point into the heap to a location that has a map pointer at its first word.
// Caveat: Heap::Contains is an approximation because it can return true for
// objects in a heap space but above the allocation pointer.
class VerifyPointersVisitor : public ObjectVisitor, public RootVisitor {
 public:
  inline void VisitPointers(HeapObject* host, Object** start,
                            Object** end) override;
  inline void VisitRootPointers(Root root, Object** start,
                                Object** end) override;

 private:
  inline void VerifyPointers(Object** start, Object** end);
};


// Verify that all objects are Smis.
class VerifySmisVisitor : public RootVisitor {
 public:
  inline void VisitRootPointers(Root root, Object** start,
                                Object** end) override;
};


// Space iterator for iterating over all spaces of the heap.  Returns each space
// in turn, and null when it is done.
class AllSpaces BASE_EMBEDDED {
 public:
  explicit AllSpaces(Heap* heap) : heap_(heap), counter_(FIRST_SPACE) {}
  Space* next();

 private:
  Heap* heap_;
  int counter_;
};


// Space iterator for iterating over all old spaces of the heap: Old space
// and code space.  Returns each space in turn, and null when it is done.
class V8_EXPORT_PRIVATE OldSpaces BASE_EMBEDDED {
 public:
  explicit OldSpaces(Heap* heap) : heap_(heap), counter_(OLD_SPACE) {}
  OldSpace* next();

 private:
  Heap* heap_;
  int counter_;
};


// Space iterator for iterating over all the paged spaces of the heap: Map
// space, old space, code space and cell space.  Returns
// each space in turn, and null when it is done.
class PagedSpaces BASE_EMBEDDED {
 public:
  explicit PagedSpaces(Heap* heap) : heap_(heap), counter_(OLD_SPACE) {}
  PagedSpace* next();

 private:
  Heap* heap_;
  int counter_;
};


class SpaceIterator : public Malloced {
 public:
  explicit SpaceIterator(Heap* heap);
  virtual ~SpaceIterator();

  bool has_next();
  Space* next();

 private:
  Heap* heap_;
  int current_space_;         // from enum AllocationSpace.
};


// A HeapIterator provides iteration over the whole heap. It
// aggregates the specific iterators for the different spaces as
// these can only iterate over one space only.
//
// HeapIterator ensures there is no allocation during its lifetime
// (using an embedded DisallowHeapAllocation instance).
//
// HeapIterator can skip free list nodes (that is, de-allocated heap
// objects that still remain in the heap). As implementation of free
// nodes filtering uses GC marks, it can't be used during MS/MC GC
// phases. Also, it is forbidden to interrupt iteration in this mode,
// as this will leave heap objects marked (and thus, unusable).
class HeapIterator BASE_EMBEDDED {
 public:
  enum HeapObjectsFiltering { kNoFiltering, kFilterUnreachable };

  explicit HeapIterator(Heap* heap,
                        HeapObjectsFiltering filtering = kNoFiltering);
  ~HeapIterator();

  HeapObject* next();

 private:
  HeapObject* NextObject();

  DisallowHeapAllocation no_heap_allocation_;

  Heap* heap_;
  HeapObjectsFiltering filtering_;
  HeapObjectsFilter* filter_;
  // Space iterator for iterating all the spaces.
  SpaceIterator* space_iterator_;
  // Object iterator for the space currently being iterated.
  std::unique_ptr<ObjectIterator> object_iterator_;
};

// Abstract base class for checking whether a weak object should be retained.
class WeakObjectRetainer {
 public:
  virtual ~WeakObjectRetainer() {}

  // Return whether this object should be retained. If NULL is returned the
  // object has no references. Otherwise the address of the retained object
  // should be returned as in some GC situations the object has been moved.
  virtual Object* RetainAs(Object* object) = 0;
};

// -----------------------------------------------------------------------------
// Allows observation of allocations.
class AllocationObserver {
 public:
  explicit AllocationObserver(intptr_t step_size)
      : step_size_(step_size), bytes_to_next_step_(step_size) {
    DCHECK(step_size >= kPointerSize);
  }
  virtual ~AllocationObserver() {}

  // Called each time the observed space does an allocation step. This may be
  // more frequently than the step_size we are monitoring (e.g. when there are
  // multiple observers, or when page or space boundary is encountered.)
  void AllocationStep(int bytes_allocated, Address soon_object, size_t size) {
    bytes_to_next_step_ -= bytes_allocated;
    if (bytes_to_next_step_ <= 0) {
      Step(static_cast<int>(step_size_ - bytes_to_next_step_), soon_object,
           size);
      step_size_ = GetNextStepSize();
      bytes_to_next_step_ = step_size_;
    }
  }

 protected:
  intptr_t step_size() const { return step_size_; }
  intptr_t bytes_to_next_step() const { return bytes_to_next_step_; }

  // Pure virtual method provided by the subclasses that gets called when at
  // least step_size bytes have been allocated. soon_object is the address just
  // allocated (but not yet initialized.) size is the size of the object as
  // requested (i.e. w/o the alignment fillers). Some complexities to be aware
  // of:
  // 1) soon_object will be nullptr in cases where we end up observing an
  //    allocation that happens to be a filler space (e.g. page boundaries.)
  // 2) size is the requested size at the time of allocation. Right-trimming
  //    may change the object size dynamically.
  // 3) soon_object may actually be the first object in an allocation-folding
  //    group. In such a case size is the size of the group rather than the
  //    first object.
  virtual void Step(int bytes_allocated, Address soon_object, size_t size) = 0;

  // Subclasses can override this method to make step size dynamic.
  virtual intptr_t GetNextStepSize() { return step_size_; }

  intptr_t step_size_;
  intptr_t bytes_to_next_step_;

 private:
  friend class LargeObjectSpace;
  friend class NewSpace;
  friend class PagedSpace;
  DISALLOW_COPY_AND_ASSIGN(AllocationObserver);
};

V8_EXPORT_PRIVATE const char* AllocationSpaceName(AllocationSpace space);

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_HEAP_H_
