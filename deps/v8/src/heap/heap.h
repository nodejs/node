// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_HEAP_H_
#define V8_HEAP_HEAP_H_

#include <cmath>

#include "src/allocation.h"
#include "src/assert-scope.h"
#include "src/counters.h"
#include "src/globals.h"
#include "src/heap/gc-idle-time-handler.h"
#include "src/heap/gc-tracer.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/mark-compact.h"
#include "src/heap/objects-visiting.h"
#include "src/heap/spaces.h"
#include "src/heap/store-buffer.h"
#include "src/list.h"
#include "src/splay-tree-inl.h"

namespace v8 {
namespace internal {

// Defines all the roots in Heap.
#define STRONG_ROOT_LIST(V)                                                    \
  V(Map, byte_array_map, ByteArrayMap)                                         \
  V(Map, free_space_map, FreeSpaceMap)                                         \
  V(Map, one_pointer_filler_map, OnePointerFillerMap)                          \
  V(Map, two_pointer_filler_map, TwoPointerFillerMap)                          \
  /* Cluster the most popular ones in a few cache lines here at the top.    */ \
  V(Smi, store_buffer_top, StoreBufferTop)                                     \
  V(Oddball, undefined_value, UndefinedValue)                                  \
  V(Oddball, the_hole_value, TheHoleValue)                                     \
  V(Oddball, null_value, NullValue)                                            \
  V(Oddball, true_value, TrueValue)                                            \
  V(Oddball, false_value, FalseValue)                                          \
  V(Oddball, uninitialized_value, UninitializedValue)                          \
  V(Oddball, exception, Exception)                                             \
  V(Map, cell_map, CellMap)                                                    \
  V(Map, global_property_cell_map, GlobalPropertyCellMap)                      \
  V(Map, shared_function_info_map, SharedFunctionInfoMap)                      \
  V(Map, meta_map, MetaMap)                                                    \
  V(Map, heap_number_map, HeapNumberMap)                                       \
  V(Map, mutable_heap_number_map, MutableHeapNumberMap)                        \
  V(Map, native_context_map, NativeContextMap)                                 \
  V(Map, fixed_array_map, FixedArrayMap)                                       \
  V(Map, code_map, CodeMap)                                                    \
  V(Map, scope_info_map, ScopeInfoMap)                                         \
  V(Map, fixed_cow_array_map, FixedCOWArrayMap)                                \
  V(Map, fixed_double_array_map, FixedDoubleArrayMap)                          \
  V(Map, constant_pool_array_map, ConstantPoolArrayMap)                        \
  V(Map, weak_cell_map, WeakCellMap)                                           \
  V(Oddball, no_interceptor_result_sentinel, NoInterceptorResultSentinel)      \
  V(Map, hash_table_map, HashTableMap)                                         \
  V(Map, ordered_hash_table_map, OrderedHashTableMap)                          \
  V(FixedArray, empty_fixed_array, EmptyFixedArray)                            \
  V(ByteArray, empty_byte_array, EmptyByteArray)                               \
  V(DescriptorArray, empty_descriptor_array, EmptyDescriptorArray)             \
  V(ConstantPoolArray, empty_constant_pool_array, EmptyConstantPoolArray)      \
  V(Oddball, arguments_marker, ArgumentsMarker)                                \
  /* The roots above this line should be boring from a GC point of view.    */ \
  /* This means they are never in new space and never on a page that is     */ \
  /* being compacted.                                                       */ \
  V(FixedArray, number_string_cache, NumberStringCache)                        \
  V(Object, instanceof_cache_function, InstanceofCacheFunction)                \
  V(Object, instanceof_cache_map, InstanceofCacheMap)                          \
  V(Object, instanceof_cache_answer, InstanceofCacheAnswer)                    \
  V(FixedArray, single_character_string_cache, SingleCharacterStringCache)     \
  V(FixedArray, string_split_cache, StringSplitCache)                          \
  V(FixedArray, regexp_multiple_cache, RegExpMultipleCache)                    \
  V(Oddball, termination_exception, TerminationException)                      \
  V(Smi, hash_seed, HashSeed)                                                  \
  V(Map, symbol_map, SymbolMap)                                                \
  V(Map, string_map, StringMap)                                                \
  V(Map, one_byte_string_map, OneByteStringMap)                                \
  V(Map, cons_string_map, ConsStringMap)                                       \
  V(Map, cons_one_byte_string_map, ConsOneByteStringMap)                       \
  V(Map, sliced_string_map, SlicedStringMap)                                   \
  V(Map, sliced_one_byte_string_map, SlicedOneByteStringMap)                   \
  V(Map, external_string_map, ExternalStringMap)                               \
  V(Map, external_string_with_one_byte_data_map,                               \
    ExternalStringWithOneByteDataMap)                                          \
  V(Map, external_one_byte_string_map, ExternalOneByteStringMap)               \
  V(Map, native_source_string_map, NativeSourceStringMap)                      \
  V(Map, short_external_string_map, ShortExternalStringMap)                    \
  V(Map, short_external_string_with_one_byte_data_map,                         \
    ShortExternalStringWithOneByteDataMap)                                     \
  V(Map, internalized_string_map, InternalizedStringMap)                       \
  V(Map, one_byte_internalized_string_map, OneByteInternalizedStringMap)       \
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
  V(Map, undetectable_string_map, UndetectableStringMap)                       \
  V(Map, undetectable_one_byte_string_map, UndetectableOneByteStringMap)       \
  V(Map, external_int8_array_map, ExternalInt8ArrayMap)                        \
  V(Map, external_uint8_array_map, ExternalUint8ArrayMap)                      \
  V(Map, external_int16_array_map, ExternalInt16ArrayMap)                      \
  V(Map, external_uint16_array_map, ExternalUint16ArrayMap)                    \
  V(Map, external_int32_array_map, ExternalInt32ArrayMap)                      \
  V(Map, external_uint32_array_map, ExternalUint32ArrayMap)                    \
  V(Map, external_float32_array_map, ExternalFloat32ArrayMap)                  \
  V(Map, external_float64_array_map, ExternalFloat64ArrayMap)                  \
  V(Map, external_uint8_clamped_array_map, ExternalUint8ClampedArrayMap)       \
  V(ExternalArray, empty_external_int8_array, EmptyExternalInt8Array)          \
  V(ExternalArray, empty_external_uint8_array, EmptyExternalUint8Array)        \
  V(ExternalArray, empty_external_int16_array, EmptyExternalInt16Array)        \
  V(ExternalArray, empty_external_uint16_array, EmptyExternalUint16Array)      \
  V(ExternalArray, empty_external_int32_array, EmptyExternalInt32Array)        \
  V(ExternalArray, empty_external_uint32_array, EmptyExternalUint32Array)      \
  V(ExternalArray, empty_external_float32_array, EmptyExternalFloat32Array)    \
  V(ExternalArray, empty_external_float64_array, EmptyExternalFloat64Array)    \
  V(ExternalArray, empty_external_uint8_clamped_array,                         \
    EmptyExternalUint8ClampedArray)                                            \
  V(Map, fixed_uint8_array_map, FixedUint8ArrayMap)                            \
  V(Map, fixed_int8_array_map, FixedInt8ArrayMap)                              \
  V(Map, fixed_uint16_array_map, FixedUint16ArrayMap)                          \
  V(Map, fixed_int16_array_map, FixedInt16ArrayMap)                            \
  V(Map, fixed_uint32_array_map, FixedUint32ArrayMap)                          \
  V(Map, fixed_int32_array_map, FixedInt32ArrayMap)                            \
  V(Map, fixed_float32_array_map, FixedFloat32ArrayMap)                        \
  V(Map, fixed_float64_array_map, FixedFloat64ArrayMap)                        \
  V(Map, fixed_uint8_clamped_array_map, FixedUint8ClampedArrayMap)             \
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
  V(Map, sloppy_arguments_elements_map, SloppyArgumentsElementsMap)            \
  V(Map, function_context_map, FunctionContextMap)                             \
  V(Map, catch_context_map, CatchContextMap)                                   \
  V(Map, with_context_map, WithContextMap)                                     \
  V(Map, block_context_map, BlockContextMap)                                   \
  V(Map, module_context_map, ModuleContextMap)                                 \
  V(Map, global_context_map, GlobalContextMap)                                 \
  V(Map, undefined_map, UndefinedMap)                                          \
  V(Map, the_hole_map, TheHoleMap)                                             \
  V(Map, null_map, NullMap)                                                    \
  V(Map, boolean_map, BooleanMap)                                              \
  V(Map, uninitialized_map, UninitializedMap)                                  \
  V(Map, arguments_marker_map, ArgumentsMarkerMap)                             \
  V(Map, no_interceptor_result_sentinel_map, NoInterceptorResultSentinelMap)   \
  V(Map, exception_map, ExceptionMap)                                          \
  V(Map, termination_exception_map, TerminationExceptionMap)                   \
  V(Map, message_object_map, JSMessageObjectMap)                               \
  V(Map, foreign_map, ForeignMap)                                              \
  V(HeapNumber, nan_value, NanValue)                                           \
  V(HeapNumber, infinity_value, InfinityValue)                                 \
  V(HeapNumber, minus_zero_value, MinusZeroValue)                              \
  V(Map, neander_map, NeanderMap)                                              \
  V(JSObject, message_listeners, MessageListeners)                             \
  V(UnseededNumberDictionary, code_stubs, CodeStubs)                           \
  V(UnseededNumberDictionary, non_monomorphic_cache, NonMonomorphicCache)      \
  V(PolymorphicCodeCache, polymorphic_code_cache, PolymorphicCodeCache)        \
  V(Code, js_entry_code, JsEntryCode)                                          \
  V(Code, js_construct_entry_code, JsConstructEntryCode)                       \
  V(FixedArray, natives_source_cache, NativesSourceCache)                      \
  V(Script, empty_script, EmptyScript)                                         \
  V(NameDictionary, intrinsic_function_names, IntrinsicFunctionNames)          \
  V(Cell, undefined_cell, UndefineCell)                                        \
  V(JSObject, observation_state, ObservationState)                             \
  V(Map, external_map, ExternalMap)                                            \
  V(Object, symbol_registry, SymbolRegistry)                                   \
  V(SeededNumberDictionary, empty_slow_element_dictionary,                     \
    EmptySlowElementDictionary)                                                \
  V(FixedArray, materialized_objects, MaterializedObjects)                     \
  V(FixedArray, allocation_sites_scratchpad, AllocationSitesScratchpad)        \
  V(FixedArray, microtask_queue, MicrotaskQueue)

// Entries in this list are limited to Smis and are not visited during GC.
#define SMI_ROOT_LIST(V)                                                   \
  V(Smi, stack_limit, StackLimit)                                          \
  V(Smi, real_stack_limit, RealStackLimit)                                 \
  V(Smi, last_script_id, LastScriptId)                                     \
  V(Smi, arguments_adaptor_deopt_pc_offset, ArgumentsAdaptorDeoptPCOffset) \
  V(Smi, construct_stub_deopt_pc_offset, ConstructStubDeoptPCOffset)       \
  V(Smi, getter_stub_deopt_pc_offset, GetterStubDeoptPCOffset)             \
  V(Smi, setter_stub_deopt_pc_offset, SetterStubDeoptPCOffset)

#define ROOT_LIST(V)  \
  STRONG_ROOT_LIST(V) \
  SMI_ROOT_LIST(V)    \
  V(StringTable, string_table, StringTable)

#define INTERNALIZED_STRING_LIST(V)                        \
  V(Object_string, "Object")                               \
  V(proto_string, "__proto__")                             \
  V(arguments_string, "arguments")                         \
  V(Arguments_string, "Arguments")                         \
  V(caller_string, "caller")                               \
  V(boolean_string, "boolean")                             \
  V(Boolean_string, "Boolean")                             \
  V(callee_string, "callee")                               \
  V(constructor_string, "constructor")                     \
  V(dot_result_string, ".result")                          \
  V(dot_for_string, ".for.")                               \
  V(eval_string, "eval")                                   \
  V(empty_string, "")                                      \
  V(function_string, "function")                           \
  V(Function_string, "Function")                           \
  V(length_string, "length")                               \
  V(name_string, "name")                                   \
  V(null_string, "null")                                   \
  V(number_string, "number")                               \
  V(Number_string, "Number")                               \
  V(nan_string, "NaN")                                     \
  V(source_string, "source")                               \
  V(source_url_string, "source_url")                       \
  V(source_mapping_url_string, "source_mapping_url")       \
  V(global_string, "global")                               \
  V(ignore_case_string, "ignoreCase")                      \
  V(multiline_string, "multiline")                         \
  V(sticky_string, "sticky")                               \
  V(harmony_regexps_string, "harmony_regexps")             \
  V(input_string, "input")                                 \
  V(index_string, "index")                                 \
  V(last_index_string, "lastIndex")                        \
  V(object_string, "object")                               \
  V(prototype_string, "prototype")                         \
  V(string_string, "string")                               \
  V(String_string, "String")                               \
  V(symbol_string, "symbol")                               \
  V(Symbol_string, "Symbol")                               \
  V(Map_string, "Map")                                     \
  V(Set_string, "Set")                                     \
  V(WeakMap_string, "WeakMap")                             \
  V(WeakSet_string, "WeakSet")                             \
  V(for_string, "for")                                     \
  V(for_api_string, "for_api")                             \
  V(for_intern_string, "for_intern")                       \
  V(private_api_string, "private_api")                     \
  V(private_intern_string, "private_intern")               \
  V(Date_string, "Date")                                   \
  V(char_at_string, "CharAt")                              \
  V(undefined_string, "undefined")                         \
  V(value_of_string, "valueOf")                            \
  V(stack_string, "stack")                                 \
  V(toJSON_string, "toJSON")                               \
  V(KeyedLoadMonomorphic_string, "KeyedLoadMonomorphic")   \
  V(KeyedStoreMonomorphic_string, "KeyedStoreMonomorphic") \
  V(stack_overflow_string, "kStackOverflowBoilerplate")    \
  V(illegal_access_string, "illegal access")               \
  V(cell_value_string, "%cell_value")                      \
  V(illegal_argument_string, "illegal argument")           \
  V(identity_hash_string, "v8::IdentityHash")              \
  V(closure_string, "(closure)")                           \
  V(dot_string, ".")                                       \
  V(compare_ic_string, "==")                               \
  V(strict_compare_ic_string, "===")                       \
  V(infinity_string, "Infinity")                           \
  V(minus_infinity_string, "-Infinity")                    \
  V(query_colon_string, "(?:)")                            \
  V(Generator_string, "Generator")                         \
  V(throw_string, "throw")                                 \
  V(done_string, "done")                                   \
  V(value_string, "value")                                 \
  V(next_string, "next")                                   \
  V(byte_length_string, "byteLength")                      \
  V(byte_offset_string, "byteOffset")                      \
  V(minus_zero_string, "-0")                               \
  V(Array_string, "Array")                                 \
  V(Error_string, "Error")                                 \
  V(RegExp_string, "RegExp")

#define PRIVATE_SYMBOL_LIST(V)      \
  V(frozen_symbol)                  \
  V(nonexistent_symbol)             \
  V(elements_transition_symbol)     \
  V(observed_symbol)                \
  V(uninitialized_symbol)           \
  V(megamorphic_symbol)             \
  V(premonomorphic_symbol)          \
  V(generic_symbol)                 \
  V(stack_trace_symbol)             \
  V(detailed_stack_trace_symbol)    \
  V(normal_ic_symbol)               \
  V(home_object_symbol)             \
  V(intl_initialized_marker_symbol) \
  V(intl_impl_object_symbol)        \
  V(promise_debug_marker_symbol)    \
  V(promise_has_handler_symbol)     \
  V(class_script_symbol)            \
  V(class_start_position_symbol)    \
  V(class_end_position_symbol)

// Heap roots that are known to be immortal immovable, for which we can safely
// skip write barriers. This list is not complete and has omissions.
#define IMMORTAL_IMMOVABLE_ROOT_LIST(V) \
  V(ByteArrayMap)                       \
  V(FreeSpaceMap)                       \
  V(OnePointerFillerMap)                \
  V(TwoPointerFillerMap)                \
  V(UndefinedValue)                     \
  V(TheHoleValue)                       \
  V(NullValue)                          \
  V(TrueValue)                          \
  V(FalseValue)                         \
  V(UninitializedValue)                 \
  V(CellMap)                            \
  V(GlobalPropertyCellMap)              \
  V(SharedFunctionInfoMap)              \
  V(MetaMap)                            \
  V(HeapNumberMap)                      \
  V(MutableHeapNumberMap)               \
  V(NativeContextMap)                   \
  V(FixedArrayMap)                      \
  V(CodeMap)                            \
  V(ScopeInfoMap)                       \
  V(FixedCOWArrayMap)                   \
  V(FixedDoubleArrayMap)                \
  V(ConstantPoolArrayMap)               \
  V(WeakCellMap)                        \
  V(NoInterceptorResultSentinel)        \
  V(HashTableMap)                       \
  V(OrderedHashTableMap)                \
  V(EmptyFixedArray)                    \
  V(EmptyByteArray)                     \
  V(EmptyDescriptorArray)               \
  V(EmptyConstantPoolArray)             \
  V(ArgumentsMarker)                    \
  V(SymbolMap)                          \
  V(SloppyArgumentsElementsMap)         \
  V(FunctionContextMap)                 \
  V(CatchContextMap)                    \
  V(WithContextMap)                     \
  V(BlockContextMap)                    \
  V(ModuleContextMap)                   \
  V(GlobalContextMap)                   \
  V(UndefinedMap)                       \
  V(TheHoleMap)                         \
  V(NullMap)                            \
  V(BooleanMap)                         \
  V(UninitializedMap)                   \
  V(ArgumentsMarkerMap)                 \
  V(JSMessageObjectMap)                 \
  V(ForeignMap)                         \
  V(NeanderMap)                         \
  PRIVATE_SYMBOL_LIST(V)

// Forward declarations.
class HeapStats;
class Isolate;
class WeakObjectRetainer;


typedef String* (*ExternalStringTableUpdaterCallback)(Heap* heap,
                                                      Object** pointer);

class StoreBufferRebuilder {
 public:
  explicit StoreBufferRebuilder(StoreBuffer* store_buffer)
      : store_buffer_(store_buffer) {}

  void Callback(MemoryChunk* page, StoreBufferEvent event);

 private:
  StoreBuffer* store_buffer_;

  // We record in this variable how full the store buffer was when we started
  // iterating over the current page, finding pointers to new space.  If the
  // store buffer overflows again we can exempt the page from the store buffer
  // by rewinding to this point instead of having to search the store buffer.
  Object*** start_of_current_page_;
  // The current page we are scanning in the store buffer iterator.
  MemoryChunk* current_page_;
};


// A queue of objects promoted during scavenge. Each object is accompanied
// by it's size to avoid dereferencing a map pointer for scanning.
class PromotionQueue {
 public:
  explicit PromotionQueue(Heap* heap)
      : front_(NULL),
        rear_(NULL),
        limit_(NULL),
        emergency_stack_(0),
        heap_(heap) {}

  void Initialize();

  void Destroy() {
    DCHECK(is_empty());
    delete emergency_stack_;
    emergency_stack_ = NULL;
  }

  Page* GetHeadPage() {
    return Page::FromAllocationTop(reinterpret_cast<Address>(rear_));
  }

  void SetNewLimit(Address limit) {
    limit_ = reinterpret_cast<intptr_t*>(limit);

    if (limit_ <= rear_) {
      return;
    }

    RelocateQueueHead();
  }

  bool IsBelowPromotionQueue(Address to_space_top) {
    // If the given to-space top pointer and the head of the promotion queue
    // are not on the same page, then the to-space objects are below the
    // promotion queue.
    if (GetHeadPage() != Page::FromAddress(to_space_top)) {
      return true;
    }
    // If the to space top pointer is smaller or equal than the promotion
    // queue head, then the to-space objects are below the promotion queue.
    return reinterpret_cast<intptr_t*>(to_space_top) <= rear_;
  }

  bool is_empty() {
    return (front_ == rear_) &&
           (emergency_stack_ == NULL || emergency_stack_->length() == 0);
  }

  inline void insert(HeapObject* target, int size);

  void remove(HeapObject** target, int* size) {
    DCHECK(!is_empty());
    if (front_ == rear_) {
      Entry e = emergency_stack_->RemoveLast();
      *target = e.obj_;
      *size = e.size_;
      return;
    }

    if (NewSpacePage::IsAtStart(reinterpret_cast<Address>(front_))) {
      NewSpacePage* front_page =
          NewSpacePage::FromAddress(reinterpret_cast<Address>(front_));
      DCHECK(!front_page->prev_page()->is_anchor());
      front_ = reinterpret_cast<intptr_t*>(front_page->prev_page()->area_end());
    }
    *target = reinterpret_cast<HeapObject*>(*(--front_));
    *size = static_cast<int>(*(--front_));
    // Assert no underflow.
    SemiSpace::AssertValidRange(reinterpret_cast<Address>(rear_),
                                reinterpret_cast<Address>(front_));
  }

 private:
  // The front of the queue is higher in the memory page chain than the rear.
  intptr_t* front_;
  intptr_t* rear_;
  intptr_t* limit_;

  static const int kEntrySizeInWords = 2;

  struct Entry {
    Entry(HeapObject* obj, int size) : obj_(obj), size_(size) {}

    HeapObject* obj_;
    int size_;
  };
  List<Entry>* emergency_stack_;

  Heap* heap_;

  void RelocateQueueHead();

  DISALLOW_COPY_AND_ASSIGN(PromotionQueue);
};


typedef void (*ScavengingCallback)(Map* map, HeapObject** slot,
                                   HeapObject* object);


// External strings table is a place where all external strings are
// registered.  We need to keep track of such strings to properly
// finalize them.
class ExternalStringTable {
 public:
  // Registers an external string.
  inline void AddString(String* string);

  inline void Iterate(ObjectVisitor* v);

  // Restores internal invariant and gets rid of collected strings.
  // Must be called after each Iterate() that modified the strings.
  void CleanUp();

  // Destroys all allocated memory.
  void TearDown();

 private:
  explicit ExternalStringTable(Heap* heap) : heap_(heap) {}

  friend class Heap;

  inline void Verify();

  inline void AddOldString(String* string);

  // Notifies the table that only a prefix of the new list is valid.
  inline void ShrinkNewStrings(int position);

  // To speed up scavenge collections new space string are kept
  // separate from old space strings.
  List<Object*> new_space_strings_;
  List<Object*> old_space_strings_;

  Heap* heap_;

  DISALLOW_COPY_AND_ASSIGN(ExternalStringTable);
};


enum ArrayStorageAllocationMode {
  DONT_INITIALIZE_ARRAY_ELEMENTS,
  INITIALIZE_ARRAY_ELEMENTS_WITH_HOLE
};


class Heap {
 public:
  // Configure heap size in MB before setup. Return false if the heap has been
  // set up already.
  bool ConfigureHeap(int max_semi_space_size, int max_old_space_size,
                     int max_executable_size, size_t code_range_size);
  bool ConfigureHeapDefault();

  // Prepares the heap, setting up memory areas that are needed in the isolate
  // without actually creating any objects.
  bool SetUp();

  // Bootstraps the object heap with the core set of objects required to run.
  // Returns whether it succeeded.
  bool CreateHeapObjects();

  // Destroys all memory allocated by the heap.
  void TearDown();

  // Set the stack limit in the roots_ array.  Some architectures generate
  // code that looks here, because it is faster than loading from the static
  // jslimit_/real_jslimit_ variable in the StackGuard.
  void SetStackLimits();

  // Notifies the heap that is ok to start marking or other activities that
  // should not happen during deserialization.
  void NotifyDeserializationComplete();

  // Returns whether SetUp has been called.
  bool HasBeenSetUp();

  // Returns the maximum amount of memory reserved for the heap.  For
  // the young generation, we reserve 4 times the amount needed for a
  // semi space.  The young generation consists of two semi spaces and
  // we reserve twice the amount needed for those in order to ensure
  // that new space can be aligned to its size.
  intptr_t MaxReserved() {
    return 4 * reserved_semispace_size_ + max_old_generation_size_;
  }
  int MaxSemiSpaceSize() { return max_semi_space_size_; }
  int ReservedSemiSpaceSize() { return reserved_semispace_size_; }
  int InitialSemiSpaceSize() { return initial_semispace_size_; }
  int TargetSemiSpaceSize() { return target_semispace_size_; }
  intptr_t MaxOldGenerationSize() { return max_old_generation_size_; }
  intptr_t MaxExecutableSize() { return max_executable_size_; }

  // Returns the capacity of the heap in bytes w/o growing. Heap grows when
  // more spaces are needed until it reaches the limit.
  intptr_t Capacity();

  // Returns the amount of memory currently committed for the heap.
  intptr_t CommittedMemory();

  // Returns the amount of executable memory currently committed for the heap.
  intptr_t CommittedMemoryExecutable();

  // Returns the amount of phyical memory currently committed for the heap.
  size_t CommittedPhysicalMemory();

  // Returns the maximum amount of memory ever committed for the heap.
  intptr_t MaximumCommittedMemory() { return maximum_committed_; }

  // Updates the maximum committed memory for the heap. Should be called
  // whenever a space grows.
  void UpdateMaximumCommitted();

  // Returns the available bytes in space w/o growing.
  // Heap doesn't guarantee that it can allocate an object that requires
  // all available bytes. Check MaxHeapObjectSize() instead.
  intptr_t Available();

  // Returns of size of all objects residing in the heap.
  intptr_t SizeOfObjects();

  // Return the starting address and a mask for the new space.  And-masking an
  // address with the mask will result in the start address of the new space
  // for all addresses in either semispace.
  Address NewSpaceStart() { return new_space_.start(); }
  uintptr_t NewSpaceMask() { return new_space_.mask(); }
  Address NewSpaceTop() { return new_space_.top(); }

  NewSpace* new_space() { return &new_space_; }
  OldSpace* old_pointer_space() { return old_pointer_space_; }
  OldSpace* old_data_space() { return old_data_space_; }
  OldSpace* code_space() { return code_space_; }
  MapSpace* map_space() { return map_space_; }
  CellSpace* cell_space() { return cell_space_; }
  PropertyCellSpace* property_cell_space() { return property_cell_space_; }
  LargeObjectSpace* lo_space() { return lo_space_; }
  PagedSpace* paged_space(int idx) {
    switch (idx) {
      case OLD_POINTER_SPACE:
        return old_pointer_space();
      case OLD_DATA_SPACE:
        return old_data_space();
      case MAP_SPACE:
        return map_space();
      case CELL_SPACE:
        return cell_space();
      case PROPERTY_CELL_SPACE:
        return property_cell_space();
      case CODE_SPACE:
        return code_space();
      case NEW_SPACE:
      case LO_SPACE:
        UNREACHABLE();
    }
    return NULL;
  }

  bool always_allocate() { return always_allocate_scope_depth_ != 0; }
  Address always_allocate_scope_depth_address() {
    return reinterpret_cast<Address>(&always_allocate_scope_depth_);
  }

  Address* NewSpaceAllocationTopAddress() {
    return new_space_.allocation_top_address();
  }
  Address* NewSpaceAllocationLimitAddress() {
    return new_space_.allocation_limit_address();
  }

  Address* OldPointerSpaceAllocationTopAddress() {
    return old_pointer_space_->allocation_top_address();
  }
  Address* OldPointerSpaceAllocationLimitAddress() {
    return old_pointer_space_->allocation_limit_address();
  }

  Address* OldDataSpaceAllocationTopAddress() {
    return old_data_space_->allocation_top_address();
  }
  Address* OldDataSpaceAllocationLimitAddress() {
    return old_data_space_->allocation_limit_address();
  }

  // Returns a deep copy of the JavaScript object.
  // Properties and elements are copied too.
  // Optionally takes an AllocationSite to be appended in an AllocationMemento.
  MUST_USE_RESULT AllocationResult
      CopyJSObject(JSObject* source, AllocationSite* site = NULL);

  // Clear the Instanceof cache (used when a prototype changes).
  inline void ClearInstanceofCache();

  // Iterates the whole code space to clear all ICs of the given kind.
  void ClearAllICsByKind(Code::Kind kind);

  // For use during bootup.
  void RepairFreeListsAfterBoot();

  template <typename T>
  static inline bool IsOneByte(T t, int chars);

  // Move len elements within a given array from src_index index to dst_index
  // index.
  void MoveElements(FixedArray* array, int dst_index, int src_index, int len);

  // Sloppy mode arguments object size.
  static const int kSloppyArgumentsObjectSize =
      JSObject::kHeaderSize + 2 * kPointerSize;
  // Strict mode arguments has no callee so it is smaller.
  static const int kStrictArgumentsObjectSize =
      JSObject::kHeaderSize + 1 * kPointerSize;
  // Indicies for direct access into argument objects.
  static const int kArgumentsLengthIndex = 0;
  // callee is only valid in sloppy mode.
  static const int kArgumentsCalleeIndex = 1;

  // Finalizes an external string by deleting the associated external
  // data and clearing the resource pointer.
  inline void FinalizeExternalString(String* string);

  // Initialize a filler object to keep the ability to iterate over the heap
  // when introducing gaps within pages.
  void CreateFillerObjectAt(Address addr, int size);

  bool CanMoveObjectStart(HeapObject* object);

  // Indicates whether live bytes adjustment is triggered from within the GC
  // code or from mutator code.
  enum InvocationMode { FROM_GC, FROM_MUTATOR };

  // Maintain consistency of live bytes during incremental marking.
  void AdjustLiveBytes(Address address, int by, InvocationMode mode);

  // Trim the given array from the left. Note that this relocates the object
  // start and hence is only valid if there is only a single reference to it.
  FixedArrayBase* LeftTrimFixedArray(FixedArrayBase* obj, int elements_to_trim);

  // Trim the given array from the right.
  template<Heap::InvocationMode mode>
  void RightTrimFixedArray(FixedArrayBase* obj, int elements_to_trim);

  // Converts the given boolean condition to JavaScript boolean value.
  inline Object* ToBoolean(bool condition);

  // Performs garbage collection operation.
  // Returns whether there is a chance that another major GC could
  // collect more garbage.
  inline bool CollectGarbage(
      AllocationSpace space, const char* gc_reason = NULL,
      const GCCallbackFlags gc_callback_flags = kNoGCCallbackFlags);

  static const int kNoGCFlags = 0;
  static const int kReduceMemoryFootprintMask = 1;
  static const int kAbortIncrementalMarkingMask = 2;

  // Making the heap iterable requires us to abort incremental marking.
  static const int kMakeHeapIterableMask = kAbortIncrementalMarkingMask;

  // Performs a full garbage collection.  If (flags & kMakeHeapIterableMask) is
  // non-zero, then the slower precise sweeper is used, which leaves the heap
  // in a state where we can iterate over the heap visiting all objects.
  void CollectAllGarbage(
      int flags, const char* gc_reason = NULL,
      const GCCallbackFlags gc_callback_flags = kNoGCCallbackFlags);

  // Last hope GC, should try to squeeze as much as possible.
  void CollectAllAvailableGarbage(const char* gc_reason = NULL);

  // Check whether the heap is currently iterable.
  bool IsHeapIterable();

  // Notify the heap that a context has been disposed.
  int NotifyContextDisposed();

  inline void increment_scan_on_scavenge_pages() {
    scan_on_scavenge_pages_++;
    if (FLAG_gc_verbose) {
      PrintF("Scan-on-scavenge pages: %d\n", scan_on_scavenge_pages_);
    }
  }

  inline void decrement_scan_on_scavenge_pages() {
    scan_on_scavenge_pages_--;
    if (FLAG_gc_verbose) {
      PrintF("Scan-on-scavenge pages: %d\n", scan_on_scavenge_pages_);
    }
  }

  PromotionQueue* promotion_queue() { return &promotion_queue_; }

  void AddGCPrologueCallback(v8::Isolate::GCPrologueCallback callback,
                             GCType gc_type_filter, bool pass_isolate = true);
  void RemoveGCPrologueCallback(v8::Isolate::GCPrologueCallback callback);

  void AddGCEpilogueCallback(v8::Isolate::GCEpilogueCallback callback,
                             GCType gc_type_filter, bool pass_isolate = true);
  void RemoveGCEpilogueCallback(v8::Isolate::GCEpilogueCallback callback);

// Heap root getters.  We have versions with and without type::cast() here.
// You can't use type::cast during GC because the assert fails.
// TODO(1490): Try removing the unchecked accessors, now that GC marking does
// not corrupt the map.
#define ROOT_ACCESSOR(type, name, camel_name)                           \
  type* name() { return type::cast(roots_[k##camel_name##RootIndex]); } \
  type* raw_unchecked_##name() {                                        \
    return reinterpret_cast<type*>(roots_[k##camel_name##RootIndex]);   \
  }
  ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

// Utility type maps
#define STRUCT_MAP_ACCESSOR(NAME, Name, name) \
  Map* name##_map() { return Map::cast(roots_[k##Name##MapRootIndex]); }
  STRUCT_LIST(STRUCT_MAP_ACCESSOR)
#undef STRUCT_MAP_ACCESSOR

#define STRING_ACCESSOR(name, str) \
  String* name() { return String::cast(roots_[k##name##RootIndex]); }
  INTERNALIZED_STRING_LIST(STRING_ACCESSOR)
#undef STRING_ACCESSOR

#define SYMBOL_ACCESSOR(name) \
  Symbol* name() { return Symbol::cast(roots_[k##name##RootIndex]); }
  PRIVATE_SYMBOL_LIST(SYMBOL_ACCESSOR)
#undef SYMBOL_ACCESSOR

  // The hidden_string is special because it is the empty string, but does
  // not match the empty string.
  String* hidden_string() { return hidden_string_; }

  void set_native_contexts_list(Object* object) {
    native_contexts_list_ = object;
  }
  Object* native_contexts_list() const { return native_contexts_list_; }

  void set_array_buffers_list(Object* object) { array_buffers_list_ = object; }
  Object* array_buffers_list() const { return array_buffers_list_; }

  void set_allocation_sites_list(Object* object) {
    allocation_sites_list_ = object;
  }
  Object* allocation_sites_list() { return allocation_sites_list_; }

  // Used in CreateAllocationSiteStub and the (de)serializer.
  Object** allocation_sites_list_address() { return &allocation_sites_list_; }

  Object* weak_object_to_code_table() { return weak_object_to_code_table_; }

  void set_encountered_weak_collections(Object* weak_collection) {
    encountered_weak_collections_ = weak_collection;
  }
  Object* encountered_weak_collections() const {
    return encountered_weak_collections_;
  }

  void set_encountered_weak_cells(Object* weak_cell) {
    encountered_weak_cells_ = weak_cell;
  }
  Object* encountered_weak_cells() const { return encountered_weak_cells_; }

  // Number of mark-sweeps.
  unsigned int ms_count() { return ms_count_; }

  // Iterates over all roots in the heap.
  void IterateRoots(ObjectVisitor* v, VisitMode mode);
  // Iterates over all strong roots in the heap.
  void IterateStrongRoots(ObjectVisitor* v, VisitMode mode);
  // Iterates over entries in the smi roots list.  Only interesting to the
  // serializer/deserializer, since GC does not care about smis.
  void IterateSmiRoots(ObjectVisitor* v);
  // Iterates over all the other roots in the heap.
  void IterateWeakRoots(ObjectVisitor* v, VisitMode mode);

  // Iterate pointers to from semispace of new space found in memory interval
  // from start to end.
  void IterateAndMarkPointersToFromSpace(Address start, Address end,
                                         ObjectSlotCallback callback);

  // Returns whether the object resides in new space.
  inline bool InNewSpace(Object* object);
  inline bool InNewSpace(Address address);
  inline bool InNewSpacePage(Address address);
  inline bool InFromSpace(Object* object);
  inline bool InToSpace(Object* object);

  // Returns whether the object resides in old pointer space.
  inline bool InOldPointerSpace(Address address);
  inline bool InOldPointerSpace(Object* object);

  // Returns whether the object resides in old data space.
  inline bool InOldDataSpace(Address address);
  inline bool InOldDataSpace(Object* object);

  // Checks whether an address/object in the heap (including auxiliary
  // area and unused area).
  bool Contains(Address addr);
  bool Contains(HeapObject* value);

  // Checks whether an address/object in a space.
  // Currently used by tests, serialization and heap verification only.
  bool InSpace(Address addr, AllocationSpace space);
  bool InSpace(HeapObject* value, AllocationSpace space);

  // Finds out which space an object should get promoted to based on its type.
  inline OldSpace* TargetSpace(HeapObject* object);
  static inline AllocationSpace TargetSpaceId(InstanceType type);

  // Checks whether the given object is allowed to be migrated from it's
  // current space into the given destination space. Used for debugging.
  inline bool AllowedToBeMigrated(HeapObject* object, AllocationSpace dest);

  // Sets the stub_cache_ (only used when expanding the dictionary).
  void public_set_code_stubs(UnseededNumberDictionary* value) {
    roots_[kCodeStubsRootIndex] = value;
  }

  // Support for computing object sizes for old objects during GCs. Returns
  // a function that is guaranteed to be safe for computing object sizes in
  // the current GC phase.
  HeapObjectCallback GcSafeSizeOfOldObjectFunction() {
    return gc_safe_size_of_old_object_;
  }

  // Sets the non_monomorphic_cache_ (only used when expanding the dictionary).
  void public_set_non_monomorphic_cache(UnseededNumberDictionary* value) {
    roots_[kNonMonomorphicCacheRootIndex] = value;
  }

  void public_set_empty_script(Script* script) {
    roots_[kEmptyScriptRootIndex] = script;
  }

  void public_set_store_buffer_top(Address* top) {
    roots_[kStoreBufferTopRootIndex] = reinterpret_cast<Smi*>(top);
  }

  void public_set_materialized_objects(FixedArray* objects) {
    roots_[kMaterializedObjectsRootIndex] = objects;
  }

  // Generated code can embed this address to get access to the roots.
  Object** roots_array_start() { return roots_; }

  Address* store_buffer_top_address() {
    return reinterpret_cast<Address*>(&roots_[kStoreBufferTopRootIndex]);
  }

  static bool RootIsImmortalImmovable(int root_index);

#ifdef VERIFY_HEAP
  // Verify the heap is in its normal state before or after a GC.
  void Verify();


  bool weak_embedded_objects_verification_enabled() {
    return no_weak_object_verification_scope_depth_ == 0;
  }
#endif

#ifdef DEBUG
  void Print();
  void PrintHandles();

  void OldPointerSpaceCheckStoreBuffer();
  void MapSpaceCheckStoreBuffer();
  void LargeObjectSpaceCheckStoreBuffer();

  // Report heap statistics.
  void ReportHeapStatistics(const char* title);
  void ReportCodeStatistics(const char* title);
#endif

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

  // Number of "runtime allocations" done so far.
  uint32_t allocations_count() { return allocations_count_; }

  // Returns deterministic "time" value in ms. Works only with
  // FLAG_verify_predictable.
  double synthetic_time() { return allocations_count_ / 2.0; }

  // Print short heap statistics.
  void PrintShortHeapStatistics();

  // Write barrier support for address[offset] = o.
  INLINE(void RecordWrite(Address address, int offset));

  // Write barrier support for address[start : start + len[ = o.
  INLINE(void RecordWrites(Address address, int start, int len));

  enum HeapState { NOT_IN_GC, SCAVENGE, MARK_COMPACT };
  inline HeapState gc_state() { return gc_state_; }

  inline bool IsInGCPostProcessing() { return gc_post_processing_depth_ > 0; }

#ifdef DEBUG
  void set_allocation_timeout(int timeout) { allocation_timeout_ = timeout; }

  void TracePathToObjectFrom(Object* target, Object* root);
  void TracePathToObject(Object* target);
  void TracePathToGlobal();
#endif

  // Callback function passed to Heap::Iterate etc.  Copies an object if
  // necessary, the object might be promoted to an old space.  The caller must
  // ensure the precondition that the object is (a) a heap object and (b) in
  // the heap's from space.
  static inline void ScavengePointer(HeapObject** p);
  static inline void ScavengeObject(HeapObject** p, HeapObject* object);

  enum ScratchpadSlotMode { IGNORE_SCRATCHPAD_SLOT, RECORD_SCRATCHPAD_SLOT };

  // If an object has an AllocationMemento trailing it, return it, otherwise
  // return NULL;
  inline AllocationMemento* FindAllocationMemento(HeapObject* object);

  // An object may have an AllocationSite associated with it through a trailing
  // AllocationMemento. Its feedback should be updated when objects are found
  // in the heap.
  static inline void UpdateAllocationSiteFeedback(HeapObject* object,
                                                  ScratchpadSlotMode mode);

  // Support for partial snapshots.  After calling this we have a linear
  // space to write objects in each space.
  struct Chunk {
    uint32_t size;
    Address start;
    Address end;
  };

  typedef List<Chunk> Reservation;

  // Returns false if not able to reserve.
  bool ReserveSpace(Reservation* reservations);

  //
  // Support for the API.
  //

  void CreateApiObjects();

  inline intptr_t PromotedTotalSize() {
    int64_t total = PromotedSpaceSizeOfObjects() + PromotedExternalMemorySize();
    if (total > kMaxInt) return static_cast<intptr_t>(kMaxInt);
    if (total < 0) return 0;
    return static_cast<intptr_t>(total);
  }

  inline intptr_t OldGenerationSpaceAvailable() {
    return old_generation_allocation_limit_ - PromotedTotalSize();
  }

  inline intptr_t OldGenerationCapacityAvailable() {
    return max_old_generation_size_ - PromotedTotalSize();
  }

  static const intptr_t kMinimumOldGenerationAllocationLimit =
      8 * (Page::kPageSize > MB ? Page::kPageSize : MB);

  static const int kPointerMultiplier = i::kPointerSize / 4;

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
  static const int kMaxOldSpaceSizeHugeMemoryDevice = 700 * kPointerMultiplier;

  // The executable size has to be a multiple of Page::kPageSize.
  // Sizes are in MB.
  static const int kMaxExecutableSizeLowMemoryDevice = 96 * kPointerMultiplier;
  static const int kMaxExecutableSizeMediumMemoryDevice =
      192 * kPointerMultiplier;
  static const int kMaxExecutableSizeHighMemoryDevice =
      256 * kPointerMultiplier;
  static const int kMaxExecutableSizeHugeMemoryDevice =
      256 * kPointerMultiplier;

  intptr_t OldGenerationAllocationLimit(intptr_t old_gen_size,
                                        int freed_global_handles);

  // Indicates whether inline bump-pointer allocation has been disabled.
  bool inline_allocation_disabled() { return inline_allocation_disabled_; }

  // Switch whether inline bump-pointer allocation should be used.
  void EnableInlineAllocation();
  void DisableInlineAllocation();

  // Implements the corresponding V8 API function.
  bool IdleNotification(int idle_time_in_ms);

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

  Object* root(RootListIndex index) { return roots_[index]; }

  STATIC_ASSERT(kUndefinedValueRootIndex ==
                Internals::kUndefinedValueRootIndex);
  STATIC_ASSERT(kNullValueRootIndex == Internals::kNullValueRootIndex);
  STATIC_ASSERT(kTrueValueRootIndex == Internals::kTrueValueRootIndex);
  STATIC_ASSERT(kFalseValueRootIndex == Internals::kFalseValueRootIndex);
  STATIC_ASSERT(kempty_stringRootIndex == Internals::kEmptyStringRootIndex);

  // Generated code can embed direct references to non-writable roots if
  // they are in new space.
  static bool RootCanBeWrittenAfterInitialization(RootListIndex root_index);
  // Generated code can treat direct references to this root as constant.
  bool RootCanBeTreatedAsConstant(RootListIndex root_index);

  Map* MapForFixedTypedArray(ExternalArrayType array_type);
  RootListIndex RootIndexForFixedTypedArray(ExternalArrayType array_type);

  Map* MapForExternalArrayType(ExternalArrayType array_type);
  RootListIndex RootIndexForExternalArrayType(ExternalArrayType array_type);

  RootListIndex RootIndexForEmptyExternalArray(ElementsKind kind);
  RootListIndex RootIndexForEmptyFixedTypedArray(ElementsKind kind);
  ExternalArray* EmptyExternalArrayForMap(Map* map);
  FixedTypedArrayBase* EmptyFixedTypedArrayForMap(Map* map);

  void RecordStats(HeapStats* stats, bool take_snapshot = false);

  // Copy block of memory from src to dst. Size of block should be aligned
  // by pointer size.
  static inline void CopyBlock(Address dst, Address src, int byte_size);

  // Optimized version of memmove for blocks with pointer size aligned sizes and
  // pointer size aligned addresses.
  static inline void MoveBlock(Address dst, Address src, int byte_size);

  // Check new space expansion criteria and expand semispaces if it was hit.
  void CheckNewSpaceExpansionCriteria();

  inline void IncrementPromotedObjectsSize(int object_size) {
    DCHECK(object_size > 0);
    promoted_objects_size_ += object_size;
  }

  inline void IncrementSemiSpaceCopiedObjectSize(int object_size) {
    DCHECK(object_size > 0);
    semi_space_copied_object_size_ += object_size;
  }

  inline void IncrementNodesDiedInNewSpace() { nodes_died_in_new_space_++; }

  inline void IncrementNodesCopiedInNewSpace() { nodes_copied_in_new_space_++; }

  inline void IncrementNodesPromoted() { nodes_promoted_++; }

  inline void IncrementYoungSurvivorsCounter(int survived) {
    DCHECK(survived >= 0);
    survived_since_last_expansion_ += survived;
  }

  inline bool NextGCIsLikelyToBeFull() {
    if (FLAG_gc_global) return true;

    if (FLAG_stress_compaction && (gc_count_ & 1) != 0) return true;

    intptr_t adjusted_allocation_limit =
        old_generation_allocation_limit_ - new_space_.Capacity();

    if (PromotedTotalSize() >= adjusted_allocation_limit) return true;

    return false;
  }

  void UpdateNewSpaceReferencesInExternalStringTable(
      ExternalStringTableUpdaterCallback updater_func);

  void UpdateReferencesInExternalStringTable(
      ExternalStringTableUpdaterCallback updater_func);

  void ProcessWeakReferences(WeakObjectRetainer* retainer);

  void VisitExternalResources(v8::ExternalResourceVisitor* visitor);

  // An object should be promoted if the object has survived a
  // scavenge operation.
  inline bool ShouldBePromoted(Address old_address, int object_size);

  void ClearJSFunctionResultCaches();

  void ClearNormalizedMapCaches();

  GCTracer* tracer() { return &tracer_; }

  // Returns the size of objects residing in non new spaces.
  intptr_t PromotedSpaceSizeOfObjects();

  double total_regexp_code_generated() { return total_regexp_code_generated_; }
  void IncreaseTotalRegexpCodeGenerated(int size) {
    total_regexp_code_generated_ += size;
  }

  void IncrementCodeGeneratedBytes(bool is_crankshafted, int size) {
    if (is_crankshafted) {
      crankshaft_codegen_bytes_generated_ += size;
    } else {
      full_codegen_bytes_generated_ += size;
    }
  }

  // Update GC statistics that are tracked on the Heap.
  void UpdateCumulativeGCStatistics(double duration, double spent_in_mutator,
                                    double marking_time);

  // Returns maximum GC pause.
  double get_max_gc_pause() { return max_gc_pause_; }

  // Returns maximum size of objects alive after GC.
  intptr_t get_max_alive_after_gc() { return max_alive_after_gc_; }

  // Returns minimal interval between two subsequent collections.
  double get_min_in_mutator() { return min_in_mutator_; }

  MarkCompactCollector* mark_compact_collector() {
    return &mark_compact_collector_;
  }

  StoreBuffer* store_buffer() { return &store_buffer_; }

  Marking* marking() { return &marking_; }

  IncrementalMarking* incremental_marking() { return &incremental_marking_; }

  ExternalStringTable* external_string_table() {
    return &external_string_table_;
  }

  // Returns the current sweep generation.
  int sweep_generation() { return sweep_generation_; }

  inline Isolate* isolate();

  void CallGCPrologueCallbacks(GCType gc_type, GCCallbackFlags flags);
  void CallGCEpilogueCallbacks(GCType gc_type, GCCallbackFlags flags);

  inline bool OldGenerationAllocationLimitReached();

  inline void DoScavengeObject(Map* map, HeapObject** slot, HeapObject* obj) {
    scavenging_visitors_table_.GetVisitor(map)(map, slot, obj);
  }

  void QueueMemoryChunkForFree(MemoryChunk* chunk);
  void FreeQueuedChunks();

  int gc_count() const { return gc_count_; }

  // Completely clear the Instanceof cache (to stop it keeping objects alive
  // around a GC).
  inline void CompletelyClearInstanceofCache();

  // The roots that have an index less than this are always in old space.
  static const int kOldSpaceRoots = 0x20;

  uint32_t HashSeed() {
    uint32_t seed = static_cast<uint32_t>(hash_seed()->value());
    DCHECK(FLAG_randomize_hashes || seed == 0);
    return seed;
  }

  void SetArgumentsAdaptorDeoptPCOffset(int pc_offset) {
    DCHECK(arguments_adaptor_deopt_pc_offset() == Smi::FromInt(0));
    set_arguments_adaptor_deopt_pc_offset(Smi::FromInt(pc_offset));
  }

  void SetConstructStubDeoptPCOffset(int pc_offset) {
    DCHECK(construct_stub_deopt_pc_offset() == Smi::FromInt(0));
    set_construct_stub_deopt_pc_offset(Smi::FromInt(pc_offset));
  }

  void SetGetterStubDeoptPCOffset(int pc_offset) {
    DCHECK(getter_stub_deopt_pc_offset() == Smi::FromInt(0));
    set_getter_stub_deopt_pc_offset(Smi::FromInt(pc_offset));
  }

  void SetSetterStubDeoptPCOffset(int pc_offset) {
    DCHECK(setter_stub_deopt_pc_offset() == Smi::FromInt(0));
    set_setter_stub_deopt_pc_offset(Smi::FromInt(pc_offset));
  }

  // For post mortem debugging.
  void RememberUnmappedPage(Address page, bool compacted);

  // Global inline caching age: it is incremented on some GCs after context
  // disposal. We use it to flush inline caches.
  int global_ic_age() { return global_ic_age_; }

  void AgeInlineCaches() {
    global_ic_age_ = (global_ic_age_ + 1) & SharedFunctionInfo::ICAgeBits::kMax;
  }

  bool flush_monomorphic_ics() { return flush_monomorphic_ics_; }

  int64_t amount_of_external_allocated_memory() {
    return amount_of_external_allocated_memory_;
  }

  void DeoptMarkedAllocationSites();

  bool MaximumSizeScavenge() { return maximum_size_scavenges_ > 0; }

  bool DeoptMaybeTenuredAllocationSites() {
    return new_space_.IsAtMaximumCapacity() && maximum_size_scavenges_ == 0;
  }

  // ObjectStats are kept in two arrays, counts and sizes. Related stats are
  // stored in a contiguous linear buffer. Stats groups are stored one after
  // another.
  enum {
    FIRST_CODE_KIND_SUB_TYPE = LAST_TYPE + 1,
    FIRST_FIXED_ARRAY_SUB_TYPE =
        FIRST_CODE_KIND_SUB_TYPE + Code::NUMBER_OF_KINDS,
    FIRST_CODE_AGE_SUB_TYPE =
        FIRST_FIXED_ARRAY_SUB_TYPE + LAST_FIXED_ARRAY_SUB_TYPE + 1,
    OBJECT_STATS_COUNT = FIRST_CODE_AGE_SUB_TYPE + Code::kCodeAgeCount + 1
  };

  void RecordObjectStats(InstanceType type, size_t size) {
    DCHECK(type <= LAST_TYPE);
    object_counts_[type]++;
    object_sizes_[type] += size;
  }

  void RecordCodeSubTypeStats(int code_sub_type, int code_age, size_t size) {
    int code_sub_type_index = FIRST_CODE_KIND_SUB_TYPE + code_sub_type;
    int code_age_index =
        FIRST_CODE_AGE_SUB_TYPE + code_age - Code::kFirstCodeAge;
    DCHECK(code_sub_type_index >= FIRST_CODE_KIND_SUB_TYPE &&
           code_sub_type_index < FIRST_CODE_AGE_SUB_TYPE);
    DCHECK(code_age_index >= FIRST_CODE_AGE_SUB_TYPE &&
           code_age_index < OBJECT_STATS_COUNT);
    object_counts_[code_sub_type_index]++;
    object_sizes_[code_sub_type_index] += size;
    object_counts_[code_age_index]++;
    object_sizes_[code_age_index] += size;
  }

  void RecordFixedArraySubTypeStats(int array_sub_type, size_t size) {
    DCHECK(array_sub_type <= LAST_FIXED_ARRAY_SUB_TYPE);
    object_counts_[FIRST_FIXED_ARRAY_SUB_TYPE + array_sub_type]++;
    object_sizes_[FIRST_FIXED_ARRAY_SUB_TYPE + array_sub_type] += size;
  }

  void CheckpointObjectStats();

  // We don't use a LockGuard here since we want to lock the heap
  // only when FLAG_concurrent_recompilation is true.
  class RelocationLock {
   public:
    explicit RelocationLock(Heap* heap) : heap_(heap) {
      heap_->relocation_mutex_.Lock();
    }


    ~RelocationLock() { heap_->relocation_mutex_.Unlock(); }

   private:
    Heap* heap_;
  };

  void AddWeakObjectToCodeDependency(Handle<Object> obj,
                                     Handle<DependentCode> dep);

  DependentCode* LookupWeakObjectToCodeDependency(Handle<Object> obj);

  void InitializeWeakObjectToCodeTable() {
    set_weak_object_to_code_table(undefined_value());
  }

  void EnsureWeakObjectToCodeTable();

  static void FatalProcessOutOfMemory(const char* location,
                                      bool take_snapshot = false);

  // This event is triggered after successful allocation of a new object made
  // by runtime. Allocations of target space for object evacuation do not
  // trigger the event. In order to track ALL allocations one must turn off
  // FLAG_inline_new and FLAG_use_allocation_folding.
  inline void OnAllocationEvent(HeapObject* object, int size_in_bytes);

  // This event is triggered after object is moved to a new place.
  inline void OnMoveEvent(HeapObject* target, HeapObject* source,
                          int size_in_bytes);

  bool deserialization_complete() const { return deserialization_complete_; }

 protected:
  // Methods made available to tests.

  // Allocates a JS Map in the heap.
  MUST_USE_RESULT AllocationResult
      AllocateMap(InstanceType instance_type, int instance_size,
                  ElementsKind elements_kind = TERMINAL_FAST_ELEMENTS_KIND);

  // Allocates and initializes a new JavaScript object based on a
  // constructor.
  // If allocation_site is non-null, then a memento is emitted after the object
  // that points to the site.
  MUST_USE_RESULT AllocationResult
      AllocateJSObject(JSFunction* constructor,
                       PretenureFlag pretenure = NOT_TENURED,
                       AllocationSite* allocation_site = NULL);

  // Allocates and initializes a new JavaScript object based on a map.
  // Passing an allocation site means that a memento will be created that
  // points to the site.
  MUST_USE_RESULT AllocationResult
      AllocateJSObjectFromMap(Map* map, PretenureFlag pretenure = NOT_TENURED,
                              bool alloc_props = true,
                              AllocationSite* allocation_site = NULL);

  // Allocated a HeapNumber from value.
  MUST_USE_RESULT AllocationResult
      AllocateHeapNumber(double value, MutableMode mode = IMMUTABLE,
                         PretenureFlag pretenure = NOT_TENURED);

  // Allocate a byte array of the specified length
  MUST_USE_RESULT AllocationResult
      AllocateByteArray(int length, PretenureFlag pretenure = NOT_TENURED);

  // Copy the code and scope info part of the code object, but insert
  // the provided data as the relocation information.
  MUST_USE_RESULT AllocationResult
      CopyCode(Code* code, Vector<byte> reloc_info);

  MUST_USE_RESULT AllocationResult CopyCode(Code* code);

  // Allocates a fixed array initialized with undefined values
  MUST_USE_RESULT AllocationResult
      AllocateFixedArray(int length, PretenureFlag pretenure = NOT_TENURED);

 private:
  Heap();

  // The amount of external memory registered through the API kept alive
  // by global handles
  int64_t amount_of_external_allocated_memory_;

  // Caches the amount of external memory registered at the last global gc.
  int64_t amount_of_external_allocated_memory_at_last_global_gc_;

  // This can be calculated directly from a pointer to the heap; however, it is
  // more expedient to get at the isolate directly from within Heap methods.
  Isolate* isolate_;

  Object* roots_[kRootListLength];

  size_t code_range_size_;
  int reserved_semispace_size_;
  int max_semi_space_size_;
  int initial_semispace_size_;
  int target_semispace_size_;
  intptr_t max_old_generation_size_;
  intptr_t max_executable_size_;
  intptr_t maximum_committed_;

  // For keeping track of how much data has survived
  // scavenge since last new space expansion.
  int survived_since_last_expansion_;

  // For keeping track on when to flush RegExp code.
  int sweep_generation_;

  int always_allocate_scope_depth_;

  // For keeping track of context disposals.
  int contexts_disposed_;

  int global_ic_age_;

  bool flush_monomorphic_ics_;

  int scan_on_scavenge_pages_;

  NewSpace new_space_;
  OldSpace* old_pointer_space_;
  OldSpace* old_data_space_;
  OldSpace* code_space_;
  MapSpace* map_space_;
  CellSpace* cell_space_;
  PropertyCellSpace* property_cell_space_;
  LargeObjectSpace* lo_space_;
  HeapState gc_state_;
  int gc_post_processing_depth_;
  Address new_space_top_after_last_gc_;

  // Returns the amount of external memory registered since last global gc.
  int64_t PromotedExternalMemorySize();

  // How many "runtime allocations" happened.
  uint32_t allocations_count_;

  // Running hash over allocations performed.
  uint32_t raw_allocations_hash_;

  // Countdown counter, dumps allocation hash when 0.
  uint32_t dump_allocations_hash_countdown_;

  // How many mark-sweep collections happened.
  unsigned int ms_count_;

  // How many gc happened.
  unsigned int gc_count_;

  // For post mortem debugging.
  static const int kRememberedUnmappedPages = 128;
  int remembered_unmapped_pages_index_;
  Address remembered_unmapped_pages_[kRememberedUnmappedPages];

  // Total length of the strings we failed to flatten since the last GC.
  int unflattened_strings_length_;

#define ROOT_ACCESSOR(type, name, camel_name)                                 \
  inline void set_##name(type* value) {                                       \
    /* The deserializer makes use of the fact that these common roots are */  \
    /* never in new space and never on a page that is being compacted.    */  \
    DCHECK(k##camel_name##RootIndex >= kOldSpaceRoots || !InNewSpace(value)); \
    roots_[k##camel_name##RootIndex] = value;                                 \
  }
  ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

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
  intptr_t old_generation_allocation_limit_;

  // Indicates that an allocation has failed in the old generation since the
  // last GC.
  bool old_gen_exhausted_;

  // Indicates that inline bump-pointer allocation has been globally disabled
  // for all spaces. This is used to disable allocations in generated code.
  bool inline_allocation_disabled_;

  // Weak list heads, threaded through the objects.
  // List heads are initilized lazily and contain the undefined_value at start.
  Object* native_contexts_list_;
  Object* array_buffers_list_;
  Object* allocation_sites_list_;

  // WeakHashTable that maps objects embedded in optimized code to dependent
  // code list. It is initilized lazily and contains the undefined_value at
  // start.
  Object* weak_object_to_code_table_;

  // List of encountered weak collections (JSWeakMap and JSWeakSet) during
  // marking. It is initialized during marking, destroyed after marking and
  // contains Smi(0) while marking is not active.
  Object* encountered_weak_collections_;

  Object* encountered_weak_cells_;

  StoreBufferRebuilder store_buffer_rebuilder_;

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

  static const StringTypeTable string_type_table[];
  static const ConstantStringTable constant_string_table[];
  static const StructTable struct_table[];

  // The special hidden string which is an empty string, but does not match
  // any string when looked up in properties.
  String* hidden_string_;

  // GC callback function, called before and after mark-compact GC.
  // Allocations in the callback function are disallowed.
  struct GCPrologueCallbackPair {
    GCPrologueCallbackPair(v8::Isolate::GCPrologueCallback callback,
                           GCType gc_type, bool pass_isolate)
        : callback(callback), gc_type(gc_type), pass_isolate_(pass_isolate) {}
    bool operator==(const GCPrologueCallbackPair& pair) const {
      return pair.callback == callback;
    }
    v8::Isolate::GCPrologueCallback callback;
    GCType gc_type;
    // TODO(dcarney): remove variable
    bool pass_isolate_;
  };
  List<GCPrologueCallbackPair> gc_prologue_callbacks_;

  struct GCEpilogueCallbackPair {
    GCEpilogueCallbackPair(v8::Isolate::GCPrologueCallback callback,
                           GCType gc_type, bool pass_isolate)
        : callback(callback), gc_type(gc_type), pass_isolate_(pass_isolate) {}
    bool operator==(const GCEpilogueCallbackPair& pair) const {
      return pair.callback == callback;
    }
    v8::Isolate::GCPrologueCallback callback;
    GCType gc_type;
    // TODO(dcarney): remove variable
    bool pass_isolate_;
  };
  List<GCEpilogueCallbackPair> gc_epilogue_callbacks_;

  // Support for computing object sizes during GC.
  HeapObjectCallback gc_safe_size_of_old_object_;
  static int GcSafeSizeOfOldObject(HeapObject* object);

  // Update the GC state. Called from the mark-compact collector.
  void MarkMapPointersAsEncoded(bool encoded) {
    DCHECK(!encoded);
    gc_safe_size_of_old_object_ = &GcSafeSizeOfOldObject;
  }

  // Code that should be run before and after each GC.  Includes some
  // reporting/verification activities when compiled with DEBUG set.
  void GarbageCollectionPrologue();
  void GarbageCollectionEpilogue();

  // Pretenuring decisions are made based on feedback collected during new
  // space evacuation. Note that between feedback collection and calling this
  // method object in old space must not move.
  // Right now we only process pretenuring feedback in high promotion mode.
  void ProcessPretenuringFeedback();

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
      GarbageCollector collector, const char* gc_reason,
      const char* collector_reason,
      const GCCallbackFlags gc_callback_flags = kNoGCCallbackFlags);

  // Performs garbage collection
  // Returns whether there is a chance another major GC could
  // collect more garbage.
  bool PerformGarbageCollection(
      GarbageCollector collector,
      const GCCallbackFlags gc_callback_flags = kNoGCCallbackFlags);

  inline void UpdateOldSpaceLimits();

  // Selects the proper allocation space depending on the given object
  // size, pretenuring decision, and preferred old-space.
  static AllocationSpace SelectSpace(int object_size,
                                     AllocationSpace preferred_old_space,
                                     PretenureFlag pretenure) {
    DCHECK(preferred_old_space == OLD_POINTER_SPACE ||
           preferred_old_space == OLD_DATA_SPACE);
    if (object_size > Page::kMaxRegularHeapObjectSize) return LO_SPACE;
    return (pretenure == TENURED) ? preferred_old_space : NEW_SPACE;
  }

  // Allocate an uninitialized object.  The memory is non-executable if the
  // hardware and OS allow.  This is the single choke-point for allocations
  // performed by the runtime and should not be bypassed (to extend this to
  // inlined allocations, use the Heap::DisableInlineAllocation() support).
  MUST_USE_RESULT inline AllocationResult AllocateRaw(
      int size_in_bytes, AllocationSpace space, AllocationSpace retry_space);

  // Allocates a heap object based on the map.
  MUST_USE_RESULT AllocationResult
      Allocate(Map* map, AllocationSpace space,
               AllocationSite* allocation_site = NULL);

  // Allocates a partial map for bootstrapping.
  MUST_USE_RESULT AllocationResult
      AllocatePartialMap(InstanceType instance_type, int instance_size);

  // Initializes a JSObject based on its map.
  void InitializeJSObjectFromMap(JSObject* obj, FixedArray* properties,
                                 Map* map);
  void InitializeAllocationMemento(AllocationMemento* memento,
                                   AllocationSite* allocation_site);

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

  bool CreateInitialMaps();
  void CreateInitialObjects();

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

  // Make a copy of src and return it. Returns
  // Failure::RetryAfterGC(requested_bytes, space) if the allocation failed.
  MUST_USE_RESULT inline AllocationResult CopyFixedArray(FixedArray* src);

  // Make a copy of src, set the map, and return the copy. Returns
  // Failure::RetryAfterGC(requested_bytes, space) if the allocation failed.
  MUST_USE_RESULT AllocationResult
      CopyFixedArrayWithMap(FixedArray* src, Map* map);

  // Make a copy of src and return it. Returns
  // Failure::RetryAfterGC(requested_bytes, space) if the allocation failed.
  MUST_USE_RESULT inline AllocationResult CopyFixedDoubleArray(
      FixedDoubleArray* src);

  // Make a copy of src and return it. Returns
  // Failure::RetryAfterGC(requested_bytes, space) if the allocation failed.
  MUST_USE_RESULT inline AllocationResult CopyConstantPoolArray(
      ConstantPoolArray* src);


  // Computes a single character string where the character has code.
  // A cache is used for one-byte (Latin1) codes.
  MUST_USE_RESULT AllocationResult
      LookupSingleCharacterStringFromCode(uint16_t code);

  // Allocate a symbol in old space.
  MUST_USE_RESULT AllocationResult AllocateSymbol();

  // Make a copy of src, set the map, and return the copy.
  MUST_USE_RESULT AllocationResult
      CopyConstantPoolArrayWithMap(ConstantPoolArray* src, Map* map);

  MUST_USE_RESULT AllocationResult AllocateConstantPoolArray(
      const ConstantPoolArray::NumberOfEntries& small);

  MUST_USE_RESULT AllocationResult AllocateExtendedConstantPoolArray(
      const ConstantPoolArray::NumberOfEntries& small,
      const ConstantPoolArray::NumberOfEntries& extended);

  // Allocates an external array of the specified length and type.
  MUST_USE_RESULT AllocationResult
      AllocateExternalArray(int length, ExternalArrayType array_type,
                            void* external_pointer, PretenureFlag pretenure);

  // Allocates a fixed typed array of the specified length and type.
  MUST_USE_RESULT AllocationResult
      AllocateFixedTypedArray(int length, ExternalArrayType array_type,
                              PretenureFlag pretenure);

  // Make a copy of src and return it.
  MUST_USE_RESULT AllocationResult CopyAndTenureFixedCOWArray(FixedArray* src);

  // Make a copy of src, set the map, and return the copy.
  MUST_USE_RESULT AllocationResult
      CopyFixedDoubleArrayWithMap(FixedDoubleArray* src, Map* map);

  // Allocates a fixed double array with uninitialized values. Returns
  MUST_USE_RESULT AllocationResult AllocateUninitializedFixedDoubleArray(
      int length, PretenureFlag pretenure = NOT_TENURED);

  // These five Create*EntryStub functions are here and forced to not be inlined
  // because of a gcc-4.4 bug that assigns wrong vtable entries.
  NO_INLINE(void CreateJSEntryStub());
  NO_INLINE(void CreateJSConstructEntryStub());

  void CreateFixedStubs();

  // Allocate empty fixed array.
  MUST_USE_RESULT AllocationResult AllocateEmptyFixedArray();

  // Allocate empty external array of given type.
  MUST_USE_RESULT AllocationResult
      AllocateEmptyExternalArray(ExternalArrayType array_type);

  // Allocate empty fixed typed array of given type.
  MUST_USE_RESULT AllocationResult
      AllocateEmptyFixedTypedArray(ExternalArrayType array_type);

  // Allocate empty constant pool array.
  MUST_USE_RESULT AllocationResult AllocateEmptyConstantPoolArray();

  // Allocate a tenured simple cell.
  MUST_USE_RESULT AllocationResult AllocateCell(Object* value);

  // Allocate a tenured JS global property cell initialized with the hole.
  MUST_USE_RESULT AllocationResult AllocatePropertyCell();

  MUST_USE_RESULT AllocationResult AllocateWeakCell(HeapObject* value);

  // Allocates a new utility object in the old generation.
  MUST_USE_RESULT AllocationResult AllocateStruct(InstanceType type);

  // Allocates a new foreign object.
  MUST_USE_RESULT AllocationResult
      AllocateForeign(Address address, PretenureFlag pretenure = NOT_TENURED);

  MUST_USE_RESULT AllocationResult
      AllocateCode(int object_size, bool immovable);

  MUST_USE_RESULT AllocationResult InternalizeStringWithKey(HashTableKey* key);

  MUST_USE_RESULT AllocationResult InternalizeString(String* str);

  // Performs a minor collection in new generation.
  void Scavenge();

  // Commits from space if it is uncommitted.
  void EnsureFromSpaceIsCommitted();

  // Uncommit unused semi space.
  bool UncommitFromSpace() { return new_space_.UncommitFromSpace(); }

  // Fill in bogus values in from space
  void ZapFromSpace();

  static String* UpdateNewSpaceReferenceInExternalStringTableEntry(
      Heap* heap, Object** pointer);

  Address DoScavenge(ObjectVisitor* scavenge_visitor, Address new_space_front);
  static void ScavengeStoreBufferCallback(Heap* heap, MemoryChunk* page,
                                          StoreBufferEvent event);

  // Performs a major collection in the whole heap.
  void MarkCompact();

  // Code to be run before and after mark-compact.
  void MarkCompactPrologue();

  void ProcessNativeContexts(WeakObjectRetainer* retainer);
  void ProcessArrayBuffers(WeakObjectRetainer* retainer);
  void ProcessAllocationSites(WeakObjectRetainer* retainer);

  // Deopts all code that contains allocation instruction which are tenured or
  // not tenured. Moreover it clears the pretenuring allocation site statistics.
  void ResetAllAllocationSitesDependentCode(PretenureFlag flag);

  // Evaluates local pretenuring for the old space and calls
  // ResetAllTenuredAllocationSitesDependentCode if too many objects died in
  // the old space.
  void EvaluateOldSpaceLocalPretenuring(uint64_t size_of_objects_before_gc);

  // Called on heap tear-down.
  void TearDownArrayBuffers();

  // Record statistics before and after garbage collection.
  void ReportStatisticsBeforeGC();
  void ReportStatisticsAfterGC();

  // Slow part of scavenge object.
  static void ScavengeObjectSlow(HeapObject** p, HeapObject* object);

  // Total RegExp code ever generated
  double total_regexp_code_generated_;

  GCTracer tracer_;

  // Creates and installs the full-sized number string cache.
  int FullSizeNumberStringCacheLength();
  // Flush the number to string cache.
  void FlushNumberStringCache();

  // Sets used allocation sites entries to undefined.
  void FlushAllocationSitesScratchpad();

  // Initializes the allocation sites scratchpad with undefined values.
  void InitializeAllocationSitesScratchpad();

  // Adds an allocation site to the scratchpad if there is space left.
  void AddAllocationSiteToScratchpad(AllocationSite* site,
                                     ScratchpadSlotMode mode);

  void UpdateSurvivalStatistics(int start_new_space_size);

  static const int kYoungSurvivalRateHighThreshold = 90;
  static const int kYoungSurvivalRateAllowedDeviation = 15;

  static const int kOldSurvivalRateLowThreshold = 10;

  int high_survival_rate_period_length_;
  intptr_t promoted_objects_size_;
  double promotion_rate_;
  intptr_t semi_space_copied_object_size_;
  double semi_space_copied_rate_;
  int nodes_died_in_new_space_;
  int nodes_copied_in_new_space_;
  int nodes_promoted_;

  // This is the pretenuring trigger for allocation sites that are in maybe
  // tenure state. When we switched to the maximum new space size we deoptimize
  // the code that belongs to the allocation site and derive the lifetime
  // of the allocation site.
  unsigned int maximum_size_scavenges_;

  // TODO(hpayer): Allocation site pretenuring may make this method obsolete.
  // Re-visit incremental marking heuristics.
  bool IsHighSurvivalRate() { return high_survival_rate_period_length_ > 0; }

  void SelectScavengingVisitorsTable();

  void IdleMarkCompact(const char* message);

  void TryFinalizeIdleIncrementalMarking(
      size_t idle_time_in_ms, size_t size_of_objects,
      size_t mark_compact_speed_in_bytes_per_ms);

  bool WorthActivatingIncrementalMarking();

  void ClearObjectStats(bool clear_last_time_stats = false);

  void set_weak_object_to_code_table(Object* value) {
    DCHECK(!InNewSpace(value));
    weak_object_to_code_table_ = value;
  }

  Object** weak_object_to_code_table_address() {
    return &weak_object_to_code_table_;
  }

  inline void UpdateAllocationsHash(HeapObject* object);
  inline void UpdateAllocationsHash(uint32_t value);
  inline void PrintAlloctionsHash();

  static const int kInitialStringTableSize = 2048;
  static const int kInitialEvalCacheSize = 64;
  static const int kInitialNumberStringCacheSize = 256;

  // Object counts and used memory by InstanceType
  size_t object_counts_[OBJECT_STATS_COUNT];
  size_t object_counts_last_time_[OBJECT_STATS_COUNT];
  size_t object_sizes_[OBJECT_STATS_COUNT];
  size_t object_sizes_last_time_[OBJECT_STATS_COUNT];

  // Maximum GC pause.
  double max_gc_pause_;

  // Total time spent in GC.
  double total_gc_time_ms_;

  // Maximum size of objects alive after GC.
  intptr_t max_alive_after_gc_;

  // Minimal interval between two subsequent collections.
  double min_in_mutator_;

  // Cumulative GC time spent in marking
  double marking_time_;

  // Cumulative GC time spent in sweeping
  double sweeping_time_;

  MarkCompactCollector mark_compact_collector_;

  StoreBuffer store_buffer_;

  Marking marking_;

  IncrementalMarking incremental_marking_;

  GCIdleTimeHandler gc_idle_time_handler_;
  unsigned int gc_count_at_last_idle_gc_;

  // These two counters are monotomically increasing and never reset.
  size_t full_codegen_bytes_generated_;
  size_t crankshaft_codegen_bytes_generated_;

  // If the --deopt_every_n_garbage_collections flag is set to a positive value,
  // this variable holds the number of garbage collections since the last
  // deoptimization triggered by garbage collection.
  int gcs_since_last_deopt_;

#ifdef VERIFY_HEAP
  int no_weak_object_verification_scope_depth_;
#endif

  static const int kAllocationSiteScratchpadSize = 256;
  int allocation_sites_scratchpad_length_;

  static const int kMaxMarkCompactsInIdleRound = 7;
  static const int kIdleScavengeThreshold = 5;

  // Shared state read by the scavenge collector and set by ScavengeObject.
  PromotionQueue promotion_queue_;

  // Flag is set when the heap has been configured.  The heap can be repeatedly
  // configured through the API until it is set up.
  bool configured_;

  ExternalStringTable external_string_table_;

  VisitorDispatchTable<ScavengingCallback> scavenging_visitors_table_;

  MemoryChunk* chunks_queued_for_free_;

  base::Mutex relocation_mutex_;

  int gc_callbacks_depth_;

  bool deserialization_complete_;

  friend class AlwaysAllocateScope;
  friend class Deserializer;
  friend class Factory;
  friend class GCCallbacksScope;
  friend class GCTracer;
  friend class HeapIterator;
  friend class Isolate;
  friend class MarkCompactCollector;
  friend class MarkCompactMarkingVisitor;
  friend class MapCompact;
#ifdef VERIFY_HEAP
  friend class NoWeakObjectVerificationScope;
#endif
  friend class Page;

  DISALLOW_COPY_AND_ASSIGN(Heap);
};


class HeapStats {
 public:
  static const int kStartMarker = 0xDECADE00;
  static const int kEndMarker = 0xDECADE01;

  int* start_marker;                       //  0
  int* new_space_size;                     //  1
  int* new_space_capacity;                 //  2
  intptr_t* old_pointer_space_size;        //  3
  intptr_t* old_pointer_space_capacity;    //  4
  intptr_t* old_data_space_size;           //  5
  intptr_t* old_data_space_capacity;       //  6
  intptr_t* code_space_size;               //  7
  intptr_t* code_space_capacity;           //  8
  intptr_t* map_space_size;                //  9
  intptr_t* map_space_capacity;            // 10
  intptr_t* cell_space_size;               // 11
  intptr_t* cell_space_capacity;           // 12
  intptr_t* lo_space_size;                 // 13
  int* global_handle_count;                // 14
  int* weak_global_handle_count;           // 15
  int* pending_global_handle_count;        // 16
  int* near_death_global_handle_count;     // 17
  int* free_global_handle_count;           // 18
  intptr_t* memory_allocator_size;         // 19
  intptr_t* memory_allocator_capacity;     // 20
  int* objects_per_type;                   // 21
  int* size_per_type;                      // 22
  int* os_error;                           // 23
  int* end_marker;                         // 24
  intptr_t* property_cell_space_size;      // 25
  intptr_t* property_cell_space_capacity;  // 26
};


class AlwaysAllocateScope {
 public:
  explicit inline AlwaysAllocateScope(Isolate* isolate);
  inline ~AlwaysAllocateScope();

 private:
  // Implicitly disable artificial allocation failures.
  Heap* heap_;
  DisallowAllocationFailure daf_;
};


#ifdef VERIFY_HEAP
class NoWeakObjectVerificationScope {
 public:
  inline NoWeakObjectVerificationScope();
  inline ~NoWeakObjectVerificationScope();
};
#endif


class GCCallbacksScope {
 public:
  explicit inline GCCallbacksScope(Heap* heap);
  inline ~GCCallbacksScope();

  inline bool CheckReenter();

 private:
  Heap* heap_;
};


// Visitor class to verify interior pointers in spaces that do not contain
// or care about intergenerational references. All heap object pointers have to
// point into the heap to a location that has a map pointer at its first word.
// Caveat: Heap::Contains is an approximation because it can return true for
// objects in a heap space but above the allocation pointer.
class VerifyPointersVisitor : public ObjectVisitor {
 public:
  inline void VisitPointers(Object** start, Object** end);
};


// Verify that all objects are Smis.
class VerifySmisVisitor : public ObjectVisitor {
 public:
  inline void VisitPointers(Object** start, Object** end);
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


// Space iterator for iterating over all old spaces of the heap: Old pointer
// space, old data space and code space.  Returns each space in turn, and null
// when it is done.
class OldSpaces BASE_EMBEDDED {
 public:
  explicit OldSpaces(Heap* heap) : heap_(heap), counter_(OLD_POINTER_SPACE) {}
  OldSpace* next();

 private:
  Heap* heap_;
  int counter_;
};


// Space iterator for iterating over all the paged spaces of the heap: Map
// space, old pointer space, old data space, code space and cell space.  Returns
// each space in turn, and null when it is done.
class PagedSpaces BASE_EMBEDDED {
 public:
  explicit PagedSpaces(Heap* heap) : heap_(heap), counter_(OLD_POINTER_SPACE) {}
  PagedSpace* next();

 private:
  Heap* heap_;
  int counter_;
};


// Space iterator for iterating over all spaces of the heap.
// For each space an object iterator is provided. The deallocation of the
// returned object iterators is handled by the space iterator.
class SpaceIterator : public Malloced {
 public:
  explicit SpaceIterator(Heap* heap);
  SpaceIterator(Heap* heap, HeapObjectCallback size_func);
  virtual ~SpaceIterator();

  bool has_next();
  ObjectIterator* next();

 private:
  ObjectIterator* CreateIterator();

  Heap* heap_;
  int current_space_;         // from enum AllocationSpace.
  ObjectIterator* iterator_;  // object iterator for the current space.
  HeapObjectCallback size_func_;
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
class HeapObjectsFilter;

class HeapIterator BASE_EMBEDDED {
 public:
  enum HeapObjectsFiltering { kNoFiltering, kFilterUnreachable };

  explicit HeapIterator(Heap* heap);
  HeapIterator(Heap* heap, HeapObjectsFiltering filtering);
  ~HeapIterator();

  HeapObject* next();
  void reset();

 private:
  struct MakeHeapIterableHelper {
    explicit MakeHeapIterableHelper(Heap* heap) { heap->MakeHeapIterable(); }
  };

  // Perform the initialization.
  void Init();
  // Perform all necessary shutdown (destruction) work.
  void Shutdown();
  HeapObject* NextObject();

  MakeHeapIterableHelper make_heap_iterable_helper_;
  DisallowHeapAllocation no_heap_allocation_;
  Heap* heap_;
  HeapObjectsFiltering filtering_;
  HeapObjectsFilter* filter_;
  // Space iterator for iterating all the spaces.
  SpaceIterator* space_iterator_;
  // Object iterator for the space currently being iterated.
  ObjectIterator* object_iterator_;
};


// Cache for mapping (map, property name) into field offset.
// Cleared at startup and prior to mark sweep collection.
class KeyedLookupCache {
 public:
  // Lookup field offset for (map, name). If absent, -1 is returned.
  int Lookup(Handle<Map> map, Handle<Name> name);

  // Update an element in the cache.
  void Update(Handle<Map> map, Handle<Name> name, int field_offset);

  // Clear the cache.
  void Clear();

  static const int kLength = 256;
  static const int kCapacityMask = kLength - 1;
  static const int kMapHashShift = 5;
  static const int kHashMask = -4;  // Zero the last two bits.
  static const int kEntriesPerBucket = 4;
  static const int kEntryLength = 2;
  static const int kMapIndex = 0;
  static const int kKeyIndex = 1;
  static const int kNotFound = -1;

  // kEntriesPerBucket should be a power of 2.
  STATIC_ASSERT((kEntriesPerBucket & (kEntriesPerBucket - 1)) == 0);
  STATIC_ASSERT(kEntriesPerBucket == -kHashMask);

 private:
  KeyedLookupCache() {
    for (int i = 0; i < kLength; ++i) {
      keys_[i].map = NULL;
      keys_[i].name = NULL;
      field_offsets_[i] = kNotFound;
    }
  }

  static inline int Hash(Handle<Map> map, Handle<Name> name);

  // Get the address of the keys and field_offsets arrays.  Used in
  // generated code to perform cache lookups.
  Address keys_address() { return reinterpret_cast<Address>(&keys_); }

  Address field_offsets_address() {
    return reinterpret_cast<Address>(&field_offsets_);
  }

  struct Key {
    Map* map;
    Name* name;
  };

  Key keys_[kLength];
  int field_offsets_[kLength];

  friend class ExternalReference;
  friend class Isolate;
  DISALLOW_COPY_AND_ASSIGN(KeyedLookupCache);
};


// Cache for mapping (map, property name) into descriptor index.
// The cache contains both positive and negative results.
// Descriptor index equals kNotFound means the property is absent.
// Cleared at startup and prior to any gc.
class DescriptorLookupCache {
 public:
  // Lookup descriptor index for (map, name).
  // If absent, kAbsent is returned.
  int Lookup(Map* source, Name* name) {
    if (!name->IsUniqueName()) return kAbsent;
    int index = Hash(source, name);
    Key& key = keys_[index];
    if ((key.source == source) && (key.name == name)) return results_[index];
    return kAbsent;
  }

  // Update an element in the cache.
  void Update(Map* source, Name* name, int result) {
    DCHECK(result != kAbsent);
    if (name->IsUniqueName()) {
      int index = Hash(source, name);
      Key& key = keys_[index];
      key.source = source;
      key.name = name;
      results_[index] = result;
    }
  }

  // Clear the cache.
  void Clear();

  static const int kAbsent = -2;

 private:
  DescriptorLookupCache() {
    for (int i = 0; i < kLength; ++i) {
      keys_[i].source = NULL;
      keys_[i].name = NULL;
      results_[i] = kAbsent;
    }
  }

  static int Hash(Object* source, Name* name) {
    // Uses only lower 32 bits if pointers are larger.
    uint32_t source_hash =
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(source)) >>
        kPointerSizeLog2;
    uint32_t name_hash =
        static_cast<uint32_t>(reinterpret_cast<uintptr_t>(name)) >>
        kPointerSizeLog2;
    return (source_hash ^ name_hash) % kLength;
  }

  static const int kLength = 64;
  struct Key {
    Map* source;
    Name* name;
  };

  Key keys_[kLength];
  int results_[kLength];

  friend class Isolate;
  DISALLOW_COPY_AND_ASSIGN(DescriptorLookupCache);
};


class RegExpResultsCache {
 public:
  enum ResultsCacheType { REGEXP_MULTIPLE_INDICES, STRING_SPLIT_SUBSTRINGS };

  // Attempt to retrieve a cached result.  On failure, 0 is returned as a Smi.
  // On success, the returned result is guaranteed to be a COW-array.
  static Object* Lookup(Heap* heap, String* key_string, Object* key_pattern,
                        ResultsCacheType type);
  // Attempt to add value_array to the cache specified by type.  On success,
  // value_array is turned into a COW-array.
  static void Enter(Isolate* isolate, Handle<String> key_string,
                    Handle<Object> key_pattern, Handle<FixedArray> value_array,
                    ResultsCacheType type);
  static void Clear(FixedArray* cache);
  static const int kRegExpResultsCacheSize = 0x100;

 private:
  static const int kArrayEntriesPerCacheEntry = 4;
  static const int kStringOffset = 0;
  static const int kPatternOffset = 1;
  static const int kArrayOffset = 2;
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


// Intrusive object marking uses least significant bit of
// heap object's map word to mark objects.
// Normally all map words have least significant bit set
// because they contain tagged map pointer.
// If the bit is not set object is marked.
// All objects should be unmarked before resuming
// JavaScript execution.
class IntrusiveMarking {
 public:
  static bool IsMarked(HeapObject* object) {
    return (object->map_word().ToRawValue() & kNotMarkedBit) == 0;
  }

  static void ClearMark(HeapObject* object) {
    uintptr_t map_word = object->map_word().ToRawValue();
    object->set_map_word(MapWord::FromRawValue(map_word | kNotMarkedBit));
    DCHECK(!IsMarked(object));
  }

  static void SetMark(HeapObject* object) {
    uintptr_t map_word = object->map_word().ToRawValue();
    object->set_map_word(MapWord::FromRawValue(map_word & ~kNotMarkedBit));
    DCHECK(IsMarked(object));
  }

  static Map* MapOfMarkedObject(HeapObject* object) {
    uintptr_t map_word = object->map_word().ToRawValue();
    return MapWord::FromRawValue(map_word | kNotMarkedBit).ToMap();
  }

  static int SizeOfMarkedObject(HeapObject* object) {
    return object->SizeFromMap(MapOfMarkedObject(object));
  }

 private:
  static const uintptr_t kNotMarkedBit = 0x1;
  STATIC_ASSERT((kHeapObjectTag & kNotMarkedBit) != 0);  // NOLINT
};


#ifdef DEBUG
// Helper class for tracing paths to a search target Object from all roots.
// The TracePathFrom() method can be used to trace paths from a specific
// object to the search target object.
class PathTracer : public ObjectVisitor {
 public:
  enum WhatToFind {
    FIND_ALL,   // Will find all matches.
    FIND_FIRST  // Will stop the search after first match.
  };

  // Tags 0, 1, and 3 are used. Use 2 for marking visited HeapObject.
  static const int kMarkTag = 2;

  // For the WhatToFind arg, if FIND_FIRST is specified, tracing will stop
  // after the first match.  If FIND_ALL is specified, then tracing will be
  // done for all matches.
  PathTracer(Object* search_target, WhatToFind what_to_find,
             VisitMode visit_mode)
      : search_target_(search_target),
        found_target_(false),
        found_target_in_trace_(false),
        what_to_find_(what_to_find),
        visit_mode_(visit_mode),
        object_stack_(20),
        no_allocation() {}

  virtual void VisitPointers(Object** start, Object** end);

  void Reset();
  void TracePathFrom(Object** root);

  bool found() const { return found_target_; }

  static Object* const kAnyGlobalObject;

 protected:
  class MarkVisitor;
  class UnmarkVisitor;

  void MarkRecursively(Object** p, MarkVisitor* mark_visitor);
  void UnmarkRecursively(Object** p, UnmarkVisitor* unmark_visitor);
  virtual void ProcessResults();

  Object* search_target_;
  bool found_target_;
  bool found_target_in_trace_;
  WhatToFind what_to_find_;
  VisitMode visit_mode_;
  List<Object*> object_stack_;

  DisallowHeapAllocation no_allocation;  // i.e. no gc allowed.

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PathTracer);
};
#endif  // DEBUG
}
}  // namespace v8::internal

#endif  // V8_HEAP_HEAP_H_
