// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_CODE_H_
#define V8_OBJECTS_CODE_H_

#include "src/base/bit-field.h"
#include "src/builtins/builtins.h"
#include "src/codegen/handler-table.h"
#include "src/codegen/maglev-safepoint-table.h"
#include "src/deoptimizer/translation-array.h"
#include "src/objects/code-kind.h"
#include "src/objects/contexts.h"
#include "src/objects/fixed-array.h"
#include "src/objects/heap-object.h"
#include "src/objects/objects.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/struct.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class ByteArray;
class BytecodeArray;
class CodeDataContainer;
class CodeDesc;

class LocalFactory;
template <typename Impl>
class FactoryBase;

namespace interpreter {
class Register;
}  // namespace interpreter

#include "torque-generated/src/objects/code-tq.inc"

// CodeDataContainer is a container for all mutable fields associated with its
// referencing {Code} object. Since {Code} objects reside on write-protected
// pages within the heap, its header fields need to be immutable. There always
// is a 1-to-1 relation between {Code} and {CodeDataContainer}, the referencing
// field {Code::code_data_container} itself is immutable.
class CodeDataContainer : public HeapObject {
 public:
  NEVER_READ_ONLY_SPACE
  DECL_ACCESSORS(next_code_link, Object)
  DECL_RELAXED_INT32_ACCESSORS(kind_specific_flags)

  // Clear uninitialized padding space. This ensures that the snapshot content
  // is deterministic.
  inline void clear_padding();

  //
  // A collection of getters and predicates that are used by respective methods
  // on Code object. They are defined here mostly because they operate on the
  // writable state of the respective Code object.
  //

  DECL_PRIMITIVE_ACCESSORS(can_have_weak_objects, bool)
  DECL_PRIMITIVE_ACCESSORS(marked_for_deoptimization, bool)
  DECL_PRIMITIVE_ACCESSORS(is_promise_rejection, bool)

  inline HandlerTable::CatchPrediction GetBuiltinCatchPrediction() const;

  // Back-reference to the Code object.
  // Available only when V8_EXTERNAL_CODE_SPACE is defined.
  DECL_GETTER(code, Code)
  DECL_RELAXED_GETTER(code, Code)

  // When V8_EXTERNAL_CODE_SPACE is enabled, Code objects are allocated in
  // a separate pointer compression cage instead of the cage where all the
  // other objects are allocated.
  // This field contains code cage base value which is used for decompressing
  // the reference to respective Code. Basically, |code_cage_base| and |code|
  // fields together form a full pointer. The reason why they are split is that
  // the code field must also support atomic access and the word alignment of
  // the full value is not guaranteed.
  inline PtrComprCageBase code_cage_base() const;
  inline void set_code_cage_base(Address code_cage_base);
  inline PtrComprCageBase code_cage_base(RelaxedLoadTag) const;
  inline void set_code_cage_base(Address code_cage_base, RelaxedStoreTag);

  // Cached value of code().InstructionStart().
  // Available only when V8_EXTERNAL_CODE_SPACE is defined.
  DECL_GETTER(code_entry_point, Address)

  inline void SetCodeAndEntryPoint(
      Isolate* isolate_for_sandbox, Code code,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline void SetEntryPointForOffHeapBuiltin(Isolate* isolate_for_sandbox,
                                             Address entry);
  // Updates the value of the code entry point. The code must be equal to
  // the code() value.
  inline void UpdateCodeEntryPoint(Isolate* isolate_for_sandbox, Code code);

  // Initializes internal flags field which stores cached values of some
  // properties of the respective Code object.
  // Available only when V8_EXTERNAL_CODE_SPACE is enabled.
  inline void initialize_flags(CodeKind kind, Builtin builtin_id,
                               bool is_turbofanned,
                               bool is_off_heap_trampoline);

  // Alias for code_entry_point to make it API compatible with Code.
  inline Address InstructionStart() const;

  // Alias for code_entry_point to make it API compatible with Code.
  inline Address raw_instruction_start() const;

  // Alias for code_entry_point to make it API compatible with Code.
  inline Address entry() const;

#ifdef V8_EXTERNAL_CODE_SPACE
  //
  // A collection of getters and predicates that forward queries to associated
  // Code object.
  //

  inline CodeKind kind() const;
  inline Builtin builtin_id() const;
  inline bool is_builtin() const;

  inline bool is_optimized_code() const;
  inline bool is_wasm_code() const;

  // Testers for interpreter builtins.
  inline bool is_interpreter_trampoline_builtin() const;

  // Testers for baseline builtins.
  inline bool is_baseline_trampoline_builtin() const;
  inline bool is_baseline_leave_frame_builtin() const;

  // Tells whether the code checks the tiering state in the function's
  // feedback vector.
  inline bool checks_tiering_state() const;

  // Tells whether the outgoing parameters of this code are tagged pointers.
  inline bool has_tagged_outgoing_params() const;

  // [is_maglevved]: Tells whether the code object was generated by the
  // Maglev optimizing compiler.
  inline bool is_maglevved() const;

  // [is_turbofanned]: Tells whether the code object was generated by the
  // TurboFan optimizing compiler.
  inline bool is_turbofanned() const;

  // [is_off_heap_trampoline]: For kind BUILTIN tells whether
  // this is a trampoline to an off-heap builtin.
  inline bool is_off_heap_trampoline() const;

  // [uses_safepoint_table]: Whether this Code object uses safepoint tables
  // (note the table may still be empty, see has_safepoint_table).
  inline bool uses_safepoint_table() const;

  // [stack_slots]: If {uses_safepoint_table()}, the number of stack slots
  // reserved in the code prologue; otherwise 0.
  inline int stack_slots() const;

  DECL_GETTER(deoptimization_data, FixedArray)
  DECL_GETTER(bytecode_or_interpreter_data, HeapObject)
  DECL_GETTER(source_position_table, ByteArray)
  DECL_GETTER(bytecode_offset_table, ByteArray)

  // Returns true if pc is inside this object's instructions.
  inline bool contains(Isolate* isolate, Address pc);

  inline Address SafepointTableAddress() const;
  inline int safepoint_table_size() const;
  inline bool has_safepoint_table() const;

  inline Address HandlerTableAddress() const;
  inline int handler_table_size() const;
  inline bool has_handler_table() const;

  inline Address constant_pool() const;
  inline int constant_pool_size() const;
  inline bool has_constant_pool() const;

  inline Address code_comments() const;
  inline int code_comments_size() const;
  inline bool has_code_comments() const;

  inline Address unwinding_info_start() const;
  inline Address unwinding_info_end() const;
  inline int unwinding_info_size() const;
  inline bool has_unwinding_info() const;

  inline byte* relocation_start() const;
  inline byte* relocation_end() const;
  inline int relocation_size() const;

  // When builtins un-embedding is enabled for the Isolate
  // (see Isolate::is_short_builtin_calls_enabled()) then both embedded and
  // un-embedded builtins might be exeuted and thus two kinds of |pc|s might
  // appear on the stack.
  // Unlike the paremeterless versions of the functions above the below variants
  // ensure that the instruction start correspond to the given |pc| value.
  // Thus for off-heap trampoline Code objects the result might be the
  // instruction start/end of the embedded code stream or of un-embedded one.
  // For normal Code objects these functions just return the
  // raw_instruction_start/end() values.
  // TODO(11527): remove these versions once the full solution is ready.
  inline Address InstructionStart(Isolate* isolate, Address pc) const;
  V8_EXPORT_PRIVATE Address OffHeapInstructionStart(Isolate* isolate,
                                                    Address pc) const;
  inline Address InstructionEnd(Isolate* isolate, Address pc) const;
  V8_EXPORT_PRIVATE Address OffHeapInstructionEnd(Isolate* isolate,
                                                  Address pc) const;

  V8_EXPORT_PRIVATE bool OffHeapBuiltinContains(Isolate* isolate,
                                                Address pc) const;

  inline Address InstructionEnd() const;
  inline int InstructionSize() const;

  // Get the safepoint entry for the given pc.
  SafepointEntry GetSafepointEntry(Isolate* isolate, Address pc);

  // Get the maglev safepoint entry for the given pc.
  MaglevSafepointEntry GetMaglevSafepointEntry(Isolate* isolate, Address pc);

  inline int GetOffsetFromInstructionStart(Isolate* isolate, Address pc) const;

  void SetMarkedForDeoptimization(const char* reason);

#ifdef ENABLE_DISASSEMBLER
  V8_EXPORT_PRIVATE void Disassemble(const char* name, std::ostream& os,
                                     Isolate* isolate,
                                     Address current_pc = kNullAddress);
#endif  // ENABLE_DISASSEMBLER

#endif  // V8_EXTERNAL_CODE_SPACE

  DECL_CAST(CodeDataContainer)

  // Dispatched behavior.
  DECL_PRINTER(CodeDataContainer)
  DECL_VERIFIER(CodeDataContainer)

// Layout description.
#define CODE_DATA_FIELDS(V)                                         \
  /* Strong pointer fields. */                                      \
  V(kPointerFieldsStrongEndOffset, 0)                               \
  /* Weak pointer fields. */                                        \
  V(kNextCodeLinkOffset, kTaggedSize)                               \
  V(kPointerFieldsWeakEndOffset, 0)                                 \
  /* Strong Code pointer fields. */                                 \
  V(kCodeOffset, V8_EXTERNAL_CODE_SPACE_BOOL ? kTaggedSize : 0)     \
  V(kCodePointerFieldsStrongEndOffset, 0)                           \
  /* Raw data fields. */                                            \
  V(kCodeCageBaseUpper32BitsOffset,                                 \
    V8_EXTERNAL_CODE_SPACE_BOOL ? kTaggedSize : 0)                  \
  V(kCodeEntryPointOffset,                                          \
    V8_EXTERNAL_CODE_SPACE_BOOL ? kSystemPointerSize : 0)           \
  V(kFlagsOffset, V8_EXTERNAL_CODE_SPACE_BOOL ? kUInt16Size : 0)    \
  V(kBuiltinIdOffset, V8_EXTERNAL_CODE_SPACE_BOOL ? kInt16Size : 0) \
  V(kKindSpecificFlagsOffset, kInt32Size)                           \
  V(kUnalignedSize, OBJECT_POINTER_PADDING(kUnalignedSize))         \
  /* Total size. */                                                 \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize, CODE_DATA_FIELDS)
#undef CODE_DATA_FIELDS

#ifdef V8_EXTERNAL_CODE_SPACE
  using ExternalCodeField =
      TaggedField<Object, kCodeOffset, ExternalCodeCompressionScheme>;
#endif

  class BodyDescriptor;

  // Flags layout.
#define FLAGS_BIT_FIELDS(V, _)      \
  V(KindField, CodeKind, 4, _)      \
  V(IsTurbofannedField, bool, 1, _) \
  V(IsOffHeapTrampoline, bool, 1, _)
  /* The other 10 bits are still free. */

  DEFINE_BIT_FIELDS(FLAGS_BIT_FIELDS)
#undef FLAGS_BIT_FIELDS
  static_assert(FLAGS_BIT_FIELDS_Ranges::kBitsCount == 6);
  static_assert(!V8_EXTERNAL_CODE_SPACE_BOOL ||
                (FLAGS_BIT_FIELDS_Ranges::kBitsCount <=
                 FIELD_SIZE(CodeDataContainer::kFlagsOffset) * kBitsPerByte));

 private:
  DECL_ACCESSORS(raw_code, Object)
  DECL_RELAXED_GETTER(raw_code, Object)

  inline void init_code_entry_point(Isolate* isolate, Address initial_value);
  inline void set_code_entry_point(Isolate* isolate, Address value);

  // When V8_EXTERNAL_CODE_SPACE is enabled the flags field contains cached
  // values of some flags of the from the respective Code object.
  DECL_RELAXED_UINT16_ACCESSORS(flags)
  inline void set_is_off_heap_trampoline_for_hash(bool value);

  template <typename IsolateT>
  friend class Deserializer;
  friend Factory;
  friend FactoryBase<Factory>;
  friend FactoryBase<LocalFactory>;
  friend Isolate;

  OBJECT_CONSTRUCTORS(CodeDataContainer, HeapObject);
};

// Code describes objects with on-the-fly generated machine code.
class Code : public HeapObject {
 public:
  NEVER_READ_ONLY_SPACE
  // Opaque data type for encapsulating code flags like kind, inline
  // cache state, and arguments count.
  using Flags = uint32_t;

  // All Code objects have the following layout:
  //
  //  +--------------------------+
  //  |          header          |
  //  | padded to code alignment |
  //  +--------------------------+  <-- raw_body_start()
  //  |       instructions       |   == raw_instruction_start()
  //  |           ...            |
  //  | padded to meta alignment |      see kMetadataAlignment
  //  +--------------------------+  <-- raw_instruction_end()
  //  |         metadata         |   == raw_metadata_start() (MS)
  //  |           ...            |
  //  |                          |  <-- MS + handler_table_offset()
  //  |                          |  <-- MS + constant_pool_offset()
  //  |                          |  <-- MS + code_comments_offset()
  //  |                          |  <-- MS + unwinding_info_offset()
  //  | padded to obj alignment  |
  //  +--------------------------+  <-- raw_metadata_end() == raw_body_end()
  //  | padded to code alignment |
  //  +--------------------------+
  //
  // In other words, the variable-size 'body' consists of 'instructions' and
  // 'metadata'.
  //
  // Note the accessor functions below may be prefixed with 'raw'. In this case,
  // raw accessors (e.g. raw_instruction_start) always refer to the on-heap
  // Code object, while camel-case accessors (e.g. InstructionStart) may refer
  // to an off-heap area in the case of embedded builtins.
  //
  // Embedded builtins are on-heap Code objects, with an out-of-line body
  // section. The on-heap Code object contains an essentially empty body
  // section, while accessors, as mentioned above, redirect to the off-heap
  // area. Metadata table offsets remain relative to MetadataStart(), i.e. they
  // point into the off-heap metadata section. The off-heap layout is described
  // in detail in the EmbeddedData class, but at a high level one can assume a
  // dedicated, out-of-line, instruction and metadata section for each embedded
  // builtin *in addition* to the on-heap Code object:
  //
  //  +--------------------------+  <-- InstructionStart()
  //  |   off-heap instructions  |
  //  |           ...            |
  //  +--------------------------+  <-- InstructionEnd()
  //
  //  +--------------------------+  <-- MetadataStart() (MS)
  //  |    off-heap metadata     |
  //  |           ...            |  <-- MS + handler_table_offset()
  //  |                          |  <-- MS + constant_pool_offset()
  //  |                          |  <-- MS + code_comments_offset()
  //  |                          |  <-- MS + unwinding_info_offset()
  //  +--------------------------+  <-- MetadataEnd()

  // Constants for use in static asserts, stating whether the body is adjacent,
  // i.e. instructions and metadata areas are adjacent.
  static constexpr bool kOnHeapBodyIsContiguous = true;
  static constexpr bool kOffHeapBodyIsContiguous = false;
  static constexpr bool kBodyIsContiguous =
      kOnHeapBodyIsContiguous && kOffHeapBodyIsContiguous;

  inline Address raw_body_start() const;
  inline Address raw_body_end() const;
  inline int raw_body_size() const;

  inline Address raw_instruction_start() const;
  inline Address InstructionStart() const;

  inline Address raw_instruction_end() const;
  inline Address InstructionEnd() const;

  // When builtins un-embedding is enabled for the Isolate
  // (see Isolate::is_short_builtin_calls_enabled()) then both embedded and
  // un-embedded builtins might be exeuted and thus two kinds of |pc|s might
  // appear on the stack.
  // Unlike the paremeterless versions of the functions above the below variants
  // ensure that the instruction start correspond to the given |pc| value.
  // Thus for off-heap trampoline Code objects the result might be the
  // instruction start/end of the embedded code stream or of un-embedded one.
  // For normal Code objects these functions just return the
  // raw_instruction_start/end() values.
  // TODO(11527): remove these versions once the full solution is ready.
  inline Address InstructionStart(Isolate* isolate, Address pc) const;
  V8_EXPORT_PRIVATE Address OffHeapInstructionStart(Isolate* isolate,
                                                    Address pc) const;
  inline Address InstructionEnd(Isolate* isolate, Address pc) const;
  V8_EXPORT_PRIVATE Address OffHeapInstructionEnd(Isolate* isolate,
                                                  Address pc) const;

  V8_EXPORT_PRIVATE bool OffHeapBuiltinContains(Isolate* isolate,
                                                Address pc) const;

  // Computes offset of the |pc| from the instruction start. The |pc| must
  // belong to this code.
  inline int GetOffsetFromInstructionStart(Isolate* isolate, Address pc) const;

  inline int raw_instruction_size() const;
  inline void set_raw_instruction_size(int value);
  inline int InstructionSize() const;

  inline Address raw_metadata_start() const;
  inline Address raw_metadata_end() const;
  inline int raw_metadata_size() const;
  inline void set_raw_metadata_size(int value);
  inline int MetadataSize() const;

  // The metadata section is aligned to this value.
  static constexpr int kMetadataAlignment = kIntSize;

  // [safepoint_table_offset]: The offset where the safepoint table starts.
  inline int safepoint_table_offset() const { return 0; }
  inline Address raw_safepoint_table_address() const;
  inline Address SafepointTableAddress() const;
  inline int safepoint_table_size() const;
  inline bool has_safepoint_table() const;

  // [handler_table_offset]: The offset where the exception handler table
  // starts.
  inline int handler_table_offset() const;
  inline void set_handler_table_offset(int offset);
  inline Address raw_handler_table_address() const;
  inline Address HandlerTableAddress() const;
  inline int handler_table_size() const;
  inline bool has_handler_table() const;

  // [constant_pool offset]: Offset of the constant pool.
  inline int constant_pool_offset() const;
  inline void set_constant_pool_offset(int offset);
  inline Address raw_constant_pool() const;
  inline Address constant_pool() const;
  inline int constant_pool_size() const;
  inline bool has_constant_pool() const;

  // [code_comments_offset]: Offset of the code comment section.
  inline int code_comments_offset() const;
  inline void set_code_comments_offset(int offset);
  inline Address raw_code_comments() const;
  inline Address code_comments() const;
  inline int code_comments_size() const;
  inline bool has_code_comments() const;

  // [unwinding_info_offset]: Offset of the unwinding info section.
  inline int32_t unwinding_info_offset() const;
  inline void set_unwinding_info_offset(int32_t offset);
  inline Address raw_unwinding_info_start() const;
  inline Address unwinding_info_start() const;
  inline Address unwinding_info_end() const;
  inline int unwinding_info_size() const;
  inline bool has_unwinding_info() const;

#ifdef ENABLE_DISASSEMBLER
  V8_EXPORT_PRIVATE void Disassemble(const char* name, std::ostream& os,
                                     Isolate* isolate,
                                     Address current_pc = kNullAddress);
#endif

  // [relocation_info]: Code relocation information
  DECL_ACCESSORS(relocation_info, ByteArray)

  // This function should be called only from GC.
  void ClearEmbeddedObjects(Heap* heap);

  // [deoptimization_data]: Array containing data for deopt for non-baseline
  // code.
  DECL_ACCESSORS(deoptimization_data, FixedArray)
  // [bytecode_or_interpreter_data]: BytecodeArray or InterpreterData for
  // baseline code.
  DECL_ACCESSORS(bytecode_or_interpreter_data, HeapObject)

  // [source_position_table]: ByteArray for the source positions table for
  // non-baseline code.
  DECL_ACCESSORS(source_position_table, ByteArray)
  // [bytecode_offset_table]: ByteArray for the bytecode offset for baseline
  // code.
  DECL_ACCESSORS(bytecode_offset_table, ByteArray)

  // If source positions have not been collected or an exception has been thrown
  // this will return empty_byte_array.
  inline ByteArray SourcePositionTable(PtrComprCageBase cage_base,
                                       SharedFunctionInfo sfi) const;

  // [code_data_container]: A container indirection for all mutable fields.
  DECL_RELEASE_ACQUIRE_ACCESSORS(code_data_container, CodeDataContainer)

  // [next_code_link]: Link for lists of optimized or deoptimized code.
  // Note that this field is stored in the {CodeDataContainer} to be mutable.
  inline Object next_code_link() const;
  inline void set_next_code_link(Object value);

  // Unchecked accessors to be used during GC.
  inline ByteArray unchecked_relocation_info() const;

  inline int relocation_size() const;

  // [kind]: Access to specific code kind.
  inline CodeKind kind() const;

  inline bool is_optimized_code() const;
  inline bool is_wasm_code() const;

  // Testers for interpreter builtins.
  inline bool is_interpreter_trampoline_builtin() const;

  // Testers for baseline builtins.
  inline bool is_baseline_trampoline_builtin() const;
  inline bool is_baseline_leave_frame_builtin() const;

  // Tells whether the code checks the tiering state in the function's
  // feedback vector.
  inline bool checks_tiering_state() const;

  // Tells whether the outgoing parameters of this code are tagged pointers.
  inline bool has_tagged_outgoing_params() const;

  // [is_turbofanned]: Tells whether the code object was generated by the
  // TurboFan optimizing compiler.
  inline bool is_turbofanned() const;

  // TODO(jgruber): Reconsider these predicates; we should probably merge them
  // and rename to something appropriate.
  inline bool is_maglevved() const;

  // [can_have_weak_objects]: If CodeKindIsOptimizedJSFunction(kind), tells
  // whether the embedded objects in code should be treated weakly.
  inline bool can_have_weak_objects() const;
  inline void set_can_have_weak_objects(bool value);

  // [builtin]: For builtins, tells which builtin index the code object
  // has. The builtin index is a non-negative integer for builtins, and
  // Builtin::kNoBuiltinId (-1) otherwise.
  inline Builtin builtin_id() const;
  inline void set_builtin_id(Builtin builtin);
  inline bool is_builtin() const;

  inline unsigned inlined_bytecode_size() const;
  inline void set_inlined_bytecode_size(unsigned size);

  inline BytecodeOffset osr_offset() const;
  inline void set_osr_offset(BytecodeOffset offset);

  // [uses_safepoint_table]: Whether this Code object uses safepoint tables
  // (note the table may still be empty, see has_safepoint_table).
  inline bool uses_safepoint_table() const;

  // [stack_slots]: If {uses_safepoint_table()}, the number of stack slots
  // reserved in the code prologue; otherwise 0.
  inline int stack_slots() const;

  // [marked_for_deoptimization]: If CodeKindCanDeoptimize(kind), tells whether
  // the code is going to be deoptimized.
  inline bool marked_for_deoptimization() const;
  inline void set_marked_for_deoptimization(bool flag);

  // [embedded_objects_cleared]: If CodeKindIsOptimizedJSFunction(kind), tells
  // whether the embedded objects in the code marked for deoptimization were
  // cleared. Note that embedded_objects_cleared() implies
  // marked_for_deoptimization().
  inline bool embedded_objects_cleared() const;
  inline void set_embedded_objects_cleared(bool flag);

  // [is_promise_rejection]: For kind BUILTIN tells whether the
  // exception thrown by the code will lead to promise rejection or
  // uncaught if both this and is_exception_caught is set.
  // Use GetBuiltinCatchPrediction to access this.
  inline void set_is_promise_rejection(bool flag);

  // [is_off_heap_trampoline]: For kind BUILTIN tells whether
  // this is a trampoline to an off-heap builtin.
  inline bool is_off_heap_trampoline() const;

  // Get the safepoint entry for the given pc.
  SafepointEntry GetSafepointEntry(Isolate* isolate, Address pc);

  // Get the maglev safepoint entry for the given pc.
  MaglevSafepointEntry GetMaglevSafepointEntry(Isolate* isolate, Address pc);

  // The entire code object including its header is copied verbatim to the
  // snapshot so that it can be written in one, fast, memcpy during
  // deserialization. The deserializer will overwrite some pointers, rather
  // like a runtime linker, but the random allocation addresses used in the
  // mksnapshot process would still be present in the unlinked snapshot data,
  // which would make snapshot production non-reproducible. This method wipes
  // out the to-be-overwritten header data for reproducible snapshots.
  inline void WipeOutHeader();

  // When V8_EXTERNAL_CODE_SPACE is enabled, Code objects are allocated in
  // a separate pointer compression cage instead of the cage where all the
  // other objects are allocated.
  // This field contains cage base value which is used for decompressing
  // the references to non-Code objects (map, deoptimization_data, etc.).
  inline PtrComprCageBase main_cage_base() const;
  inline PtrComprCageBase main_cage_base(RelaxedLoadTag) const;
  inline void set_main_cage_base(Address cage_base, RelaxedStoreTag);

  // Clear uninitialized padding space. This ensures that the snapshot content
  // is deterministic. Depending on the V8 build mode there could be no padding.
  inline void clear_padding();
  // Initialize the flags field. Similar to clear_padding above this ensure that
  // the snapshot content is deterministic.
  inline void initialize_flags(CodeKind kind, bool is_turbofanned,
                               int stack_slots, bool is_off_heap_trampoline);

  // Convert a target address into a code object.
  static inline Code GetCodeFromTargetAddress(Address address);

  // Convert an entry address into an object.
  static inline Code GetObjectFromEntryAddress(Address location_of_address);

  // Returns the size of code and its metadata. This includes the size of code
  // relocation information, deoptimization data.
  DECL_GETTER(SizeIncludingMetadata, int)

  // Returns the address of the first relocation info (read backwards!).
  inline byte* relocation_start() const;

  // Returns the address right after the relocation info (read backwards!).
  inline byte* relocation_end() const;

  // Code entry point.
  inline Address entry() const;

  // Returns true if pc is inside this object's instructions.
  inline bool contains(Isolate* isolate, Address pc);

  // Relocate the code by delta bytes. Called to signal that this code
  // object has been moved by delta bytes.
  void Relocate(intptr_t delta);

  // Migrate code from desc without flushing the instruction cache.
  void CopyFromNoFlush(ByteArray reloc_info, Heap* heap, const CodeDesc& desc);
  void RelocateFromDesc(ByteArray reloc_info, Heap* heap, const CodeDesc& desc);

  // Copy the RelocInfo portion of |desc| to |dest|. The ByteArray must be
  // exactly the same size as the RelocInfo in |desc|.
  static inline void CopyRelocInfoToByteArray(ByteArray dest,
                                              const CodeDesc& desc);

  inline uintptr_t GetBaselineStartPCForBytecodeOffset(int bytecode_offset,
                                                       BytecodeArray bytecodes);

  inline uintptr_t GetBaselineEndPCForBytecodeOffset(int bytecode_offset,
                                                     BytecodeArray bytecodes);

  // Returns the PC of the next bytecode in execution order.
  // If the bytecode at the given offset is JumpLoop, the PC of the jump target
  // is returned. Other jumps are not allowed.
  // For other bytecodes this is equivalent to
  // GetBaselineEndPCForBytecodeOffset.
  inline uintptr_t GetBaselinePCForNextExecutedBytecode(
      int bytecode_offset, BytecodeArray bytecodes);

  inline int GetBytecodeOffsetForBaselinePC(Address baseline_pc,
                                            BytecodeArray bytecodes);

  // Flushes the instruction cache for the executable instructions of this code
  // object. Make sure to call this while the code is still writable.
  void FlushICache() const;

  // Returns the object size for a given body (used for allocation).
  static int SizeFor(int body_size) {
    return RoundUp(kHeaderSize + body_size, kCodeAlignment);
  }

  inline int CodeSize() const;

  // Hides HeapObject::Size(...) and redirects queries to CodeSize().
  DECL_GETTER(Size, int)

  DECL_CAST(Code)

  // Dispatched behavior.
  DECL_PRINTER(Code)
  DECL_VERIFIER(Code)

  bool CanDeoptAt(Isolate* isolate, Address pc);

  void SetMarkedForDeoptimization(const char* reason);

  inline HandlerTable::CatchPrediction GetBuiltinCatchPrediction() const;

  bool IsIsolateIndependent(Isolate* isolate);

  inline bool CanContainWeakObjects();

  inline bool IsWeakObject(HeapObject object);

  static inline bool IsWeakObjectInOptimizedCode(HeapObject object);

  static inline bool IsWeakObjectInDeoptimizationLiteralArray(Object object);

  // Returns false if this is an embedded builtin Code object that's in
  // read_only_space and hence doesn't have execute permissions.
  inline bool IsExecutable();

  // Returns true if the function is inlined in the code.
  bool Inlines(SharedFunctionInfo sfi);

  class OptimizedCodeIterator;

  // Layout description.
#define CODE_FIELDS(V)                                                        \
  V(kRelocationInfoOffset, kTaggedSize)                                       \
  V(kDeoptimizationDataOrInterpreterDataOffset, kTaggedSize)                  \
  V(kPositionTableOffset, kTaggedSize)                                        \
  V(kCodeDataContainerOffset, kTaggedSize)                                    \
  /* Data or code not directly visited by GC directly starts here. */         \
  /* The serializer needs to copy bytes starting from here verbatim. */       \
  /* Objects embedded into code is visited via reloc info. */                 \
  V(kDataStart, 0)                                                            \
  V(kMainCageBaseUpper32BitsOffset,                                           \
    V8_EXTERNAL_CODE_SPACE_BOOL ? kTaggedSize : 0)                            \
  V(kInstructionSizeOffset, kIntSize)                                         \
  V(kMetadataSizeOffset, kIntSize)                                            \
  V(kFlagsOffset, kInt32Size)                                                 \
  V(kBuiltinIndexOffset, kIntSize)                                            \
  V(kInlinedBytecodeSizeOffset, kIntSize)                                     \
  V(kOsrOffsetOffset, kInt32Size)                                             \
  /* Offsets describing inline metadata tables, relative to MetadataStart. */ \
  V(kHandlerTableOffsetOffset, kIntSize)                                      \
  V(kConstantPoolOffsetOffset, V8_EMBEDDED_CONSTANT_POOL_BOOL ? kIntSize : 0) \
  V(kCodeCommentsOffsetOffset, kIntSize)                                      \
  V(kUnwindingInfoOffsetOffset, kInt32Size)                                   \
  V(kUnalignedHeaderSize, 0)                                                  \
  /* Add padding to align the instruction start following right after */      \
  /* the Code object header. */                                               \
  V(kOptionalPaddingOffset, CODE_POINTER_PADDING(kOptionalPaddingOffset))     \
  V(kHeaderSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize, CODE_FIELDS)
#undef CODE_FIELDS

  // This documents the amount of free space we have in each Code object header
  // due to padding for code alignment.
#if V8_TARGET_ARCH_ARM64
  static constexpr int kHeaderPaddingSize =
      V8_EXTERNAL_CODE_SPACE_BOOL ? 4 : (COMPRESS_POINTERS_BOOL ? 8 : 20);
#elif V8_TARGET_ARCH_MIPS64
  static constexpr int kHeaderPaddingSize = 20;
#elif V8_TARGET_ARCH_LOONG64
  static constexpr int kHeaderPaddingSize = 20;
#elif V8_TARGET_ARCH_X64
  static constexpr int kHeaderPaddingSize =
      V8_EXTERNAL_CODE_SPACE_BOOL ? 4 : (COMPRESS_POINTERS_BOOL ? 8 : 52);
#elif V8_TARGET_ARCH_ARM
  static constexpr int kHeaderPaddingSize = 8;
#elif V8_TARGET_ARCH_IA32
  static constexpr int kHeaderPaddingSize = 8;
#elif V8_TARGET_ARCH_MIPS
  static constexpr int kHeaderPaddingSize = 8;
#elif V8_TARGET_ARCH_PPC64
  static constexpr int kHeaderPaddingSize =
      V8_EMBEDDED_CONSTANT_POOL_BOOL ? (COMPRESS_POINTERS_BOOL ? 4 : 48)
                                     : (COMPRESS_POINTERS_BOOL ? 8 : 52);
#elif V8_TARGET_ARCH_S390X
  static constexpr int kHeaderPaddingSize = COMPRESS_POINTERS_BOOL ? 8 : 20;
#elif V8_TARGET_ARCH_RISCV64
  static constexpr int kHeaderPaddingSize = (COMPRESS_POINTERS_BOOL ? 8 : 20);
#elif V8_TARGET_ARCH_RISCV32
  static constexpr int kHeaderPaddingSize = 8;
#else
#error Unknown architecture.
#endif
  static_assert(FIELD_SIZE(kOptionalPaddingOffset) == kHeaderPaddingSize);

  class BodyDescriptor;

  // Flags layout.  base::BitField<type, shift, size>.
#define CODE_FLAGS_BIT_FIELDS(V, _)    \
  V(KindField, CodeKind, 4, _)         \
  V(IsTurbofannedField, bool, 1, _)    \
  V(StackSlotsField, int, 24, _)       \
  V(IsOffHeapTrampoline, bool, 1, _)
  DEFINE_BIT_FIELDS(CODE_FLAGS_BIT_FIELDS)
#undef CODE_FLAGS_BIT_FIELDS
  static_assert(kCodeKindCount <= KindField::kNumValues);
  static_assert(CODE_FLAGS_BIT_FIELDS_Ranges::kBitsCount == 30);
  static_assert(CODE_FLAGS_BIT_FIELDS_Ranges::kBitsCount <=
                FIELD_SIZE(kFlagsOffset) * kBitsPerByte);

  // KindSpecificFlags layout.
#define CODE_KIND_SPECIFIC_FLAGS_BIT_FIELDS(V, _) \
  V(MarkedForDeoptimizationField, bool, 1, _)     \
  V(EmbeddedObjectsClearedField, bool, 1, _)      \
  V(CanHaveWeakObjectsField, bool, 1, _)          \
  V(IsPromiseRejectionField, bool, 1, _)
  DEFINE_BIT_FIELDS(CODE_KIND_SPECIFIC_FLAGS_BIT_FIELDS)
#undef CODE_KIND_SPECIFIC_FLAGS_BIT_FIELDS
  static_assert(CODE_KIND_SPECIFIC_FLAGS_BIT_FIELDS_Ranges::kBitsCount == 4);
  static_assert(CODE_KIND_SPECIFIC_FLAGS_BIT_FIELDS_Ranges::kBitsCount <=
                FIELD_SIZE(CodeDataContainer::kKindSpecificFlagsOffset) *
                    kBitsPerByte);

  // The {marked_for_deoptimization} field is accessed from generated code.
  static const int kMarkedForDeoptimizationBit =
      MarkedForDeoptimizationField::kShift;

  static const int kArgumentsBits = 16;
  // Reserve one argument count value as the "don't adapt arguments" sentinel.
  static const int kMaxArguments = (1 << kArgumentsBits) - 2;

 private:
  friend class RelocIterator;
  friend class EvacuateVisitorBase;

  inline CodeDataContainer GCSafeCodeDataContainer(AcquireLoadTag) const;

  bool is_promise_rejection() const;

  enum BytecodeToPCPosition {
    kPcAtStartOfBytecode,
    // End of bytecode equals the start of the next bytecode.
    // We need it when we deoptimize to the next bytecode (lazy deopt or deopt
    // of non-topmost frame).
    kPcAtEndOfBytecode
  };
  inline uintptr_t GetBaselinePCForBytecodeOffset(int bytecode_offset,
                                                  BytecodeToPCPosition position,
                                                  BytecodeArray bytecodes);

  OBJECT_CONSTRUCTORS(Code, HeapObject);
};

// TODO(v8:11880): move these functions to CodeDataContainer once they are no
// longer used from Code.
V8_EXPORT_PRIVATE Address OffHeapInstructionStart(HeapObject code,
                                                  Builtin builtin);
V8_EXPORT_PRIVATE Address OffHeapInstructionEnd(HeapObject code,
                                                Builtin builtin);
V8_EXPORT_PRIVATE int OffHeapInstructionSize(HeapObject code, Builtin builtin);

V8_EXPORT_PRIVATE Address OffHeapMetadataStart(HeapObject code,
                                               Builtin builtin);
V8_EXPORT_PRIVATE Address OffHeapMetadataEnd(HeapObject code, Builtin builtin);
V8_EXPORT_PRIVATE int OffHeapMetadataSize(HeapObject code, Builtin builtin);

V8_EXPORT_PRIVATE Address OffHeapSafepointTableAddress(HeapObject code,
                                                       Builtin builtin);
V8_EXPORT_PRIVATE int OffHeapSafepointTableSize(HeapObject code,
                                                Builtin builtin);
V8_EXPORT_PRIVATE Address OffHeapHandlerTableAddress(HeapObject code,
                                                     Builtin builtin);
V8_EXPORT_PRIVATE int OffHeapHandlerTableSize(HeapObject code, Builtin builtin);
V8_EXPORT_PRIVATE Address OffHeapConstantPoolAddress(HeapObject code,
                                                     Builtin builtin);
V8_EXPORT_PRIVATE int OffHeapConstantPoolSize(HeapObject code, Builtin builtin);
V8_EXPORT_PRIVATE Address OffHeapCodeCommentsAddress(HeapObject code,
                                                     Builtin builtin);
V8_EXPORT_PRIVATE int OffHeapCodeCommentsSize(HeapObject code, Builtin builtin);
V8_EXPORT_PRIVATE Address OffHeapUnwindingInfoAddress(HeapObject code,
                                                      Builtin builtin);
V8_EXPORT_PRIVATE int OffHeapUnwindingInfoSize(HeapObject code,
                                               Builtin builtin);
V8_EXPORT_PRIVATE int OffHeapStackSlots(HeapObject code, Builtin builtin);

// Represents result of the code by inner address (or pc) lookup.
// When V8_EXTERNAL_CODE_SPACE is disabled there might be two variants:
//  - the pc does not correspond to any known code and IsFound() will return
//    false,
//  - the pc corresponds to existing Code object or embedded builtin (in which
//    case the code() will return the respective Code object or the trampoline
//    Code object that corresponds to the builtin).
//
// When V8_EXTERNAL_CODE_SPACE is enabled there might be three variants:
//  - the pc does not correspond to any known code (in which case IsFound()
//    will return false),
//  - the pc corresponds to existing Code object (in which case the code() will
//    return the respective Code object),
//  - the pc corresponds to an embedded builtin (in which case the
//    code_data_container() will return CodeDataContainer object corresponding
//    to the builtin).
class CodeLookupResult {
 public:
  // Not found.
  CodeLookupResult() = default;

  // Code object was found.
  explicit CodeLookupResult(Code code) : code_(code) {}

#ifdef V8_EXTERNAL_CODE_SPACE
  // Embedded builtin was found.
  explicit CodeLookupResult(CodeDataContainer code_data_container)
      : code_data_container_(code_data_container) {}
#endif

  // Returns true if the lookup was successful.
  bool IsFound() const { return IsCode() || IsCodeDataContainer(); }

  // Returns true if the lookup found a Code object.
  bool IsCode() const { return !code_.is_null(); }

  // Returns true if V8_EXTERNAL_CODE_SPACE is enabled and the lookup found
  // an embedded builtin.
  bool IsCodeDataContainer() const {
#ifdef V8_EXTERNAL_CODE_SPACE
    return !code_data_container_.is_null();
#else
    return false;
#endif
  }

  // Returns the Code object containing the address in question.
  Code code() const {
    DCHECK(IsCode());
    return code_;
  }

  // Returns the CodeDataContainer object corresponding to an embedded builtin
  // containing the address in question.
  // Can be used only when V8_EXTERNAL_CODE_SPACE is enabled.
  CodeDataContainer code_data_container() const {
#ifdef V8_EXTERNAL_CODE_SPACE
    DCHECK(IsCodeDataContainer());
    return code_data_container_;
#else
    UNREACHABLE();
#endif
  }

  // Returns the CodeT object corresponding to the result in question.
  // The method doesn't try to convert Code result to CodeT, one should use
  // ToCodeT() instead if the conversion logic is required.
  CodeT codet() const {
#ifdef V8_EXTERNAL_CODE_SPACE
    return code_data_container();
#else
    return code();
#endif
  }

  // Helper methods, in case of successful lookup return the result of
  // respective accessor of the Code/CodeDataContainer object found.
  // It's safe use them from GC.
  inline CodeKind kind() const;
  inline Builtin builtin_id() const;
  inline bool has_tagged_outgoing_params() const;
  inline bool has_handler_table() const;
  inline bool is_baseline_trampoline_builtin() const;
  inline bool is_interpreter_trampoline_builtin() const;
  inline bool is_baseline_leave_frame_builtin() const;
  inline bool is_maglevved() const;
  inline bool is_turbofanned() const;
  inline bool is_optimized_code() const;
  inline int stack_slots() const;
  inline HandlerTable::CatchPrediction GetBuiltinCatchPrediction() const;

  inline int GetOffsetFromInstructionStart(Isolate* isolate, Address pc) const;

  inline SafepointEntry GetSafepointEntry(Isolate* isolate, Address pc) const;
  inline MaglevSafepointEntry GetMaglevSafepointEntry(Isolate* isolate,
                                                      Address pc) const;

  // Helper method, coverts the successful lookup result to AbstractCode object.
  inline AbstractCode ToAbstractCode() const;

  // Helper method, coverts the successful lookup result to Code object.
  // It's not safe to be used from GC because conversion to Code might perform
  // a map check.
  inline Code ToCode() const;

  // Helper method, coverts the successful lookup result to CodeT object.
  // It's not safe to be used from GC because conversion to CodeT might perform
  // a map check.
  inline CodeT ToCodeT() const;

  bool operator==(const CodeLookupResult& other) const {
    return code_ == other.code_
#ifdef V8_EXTERNAL_CODE_SPACE
           && code_data_container_ == other.code_data_container_
#endif
        ;  // NOLINT(whitespace/semicolon)
  }
  bool operator!=(const CodeLookupResult& other) const {
    return !operator==(other);
  }

 private:
  Code code_;
#ifdef V8_EXTERNAL_CODE_SPACE
  CodeDataContainer code_data_container_;
#endif
};

class Code::OptimizedCodeIterator {
 public:
  explicit OptimizedCodeIterator(Isolate* isolate);
  OptimizedCodeIterator(const OptimizedCodeIterator&) = delete;
  OptimizedCodeIterator& operator=(const OptimizedCodeIterator&) = delete;
  Code Next();

 private:
  NativeContext next_context_;
  Code current_code_;
  Isolate* isolate_;

  DISALLOW_GARBAGE_COLLECTION(no_gc)
};

// Helper functions for converting Code objects to CodeDataContainer and back
// when V8_EXTERNAL_CODE_SPACE is enabled.
inline CodeT ToCodeT(Code code);
inline Handle<CodeT> ToCodeT(Handle<Code> code, Isolate* isolate);
inline Code FromCodeT(CodeT code);
inline Code FromCodeT(CodeT code, RelaxedLoadTag);
inline Code FromCodeT(CodeT code, AcquireLoadTag);
inline Code FromCodeT(CodeT code, PtrComprCageBase);
inline Code FromCodeT(CodeT code, PtrComprCageBase, RelaxedLoadTag);
inline Code FromCodeT(CodeT code, PtrComprCageBase, AcquireLoadTag);
inline Handle<Code> FromCodeT(Handle<CodeT> code, Isolate* isolate);
inline AbstractCode ToAbstractCode(CodeT code);
inline Handle<AbstractCode> ToAbstractCode(Handle<CodeT> code,
                                           Isolate* isolate);
inline CodeDataContainer CodeDataContainerFromCodeT(CodeT code);

// AbsractCode is an helper wrapper around {Code | BytecodeArray} or
// {Code | CodeDataContainer | BytecodeArray} depending on whether the
// V8_REMOVE_BUILTINS_CODE_OBJECTS is disabled or not.
// Note that when V8_EXTERNAL_CODE_SPACE is enabled then the same abstract code
// can be represented either by Code object or by respective CodeDataContainer
// object.
class AbstractCode : public HeapObject {
 public:
  NEVER_READ_ONLY_SPACE

  int SourcePosition(PtrComprCageBase cage_base, int offset);
  int SourceStatementPosition(PtrComprCageBase cage_base, int offset);

  // Returns the address of the first instruction. For off-heap code objects
  // this differs from instruction_start (which would point to the off-heap
  // trampoline instead).
  inline Address InstructionStart(PtrComprCageBase cage_base);

  // Returns the address right after the last instruction. For off-heap code
  // objects this differs from instruction_end (which would point to the
  // off-heap trampoline instead).
  inline Address InstructionEnd(PtrComprCageBase cage_base);

  // Returns the size of the native instructions, including embedded
  // data such as the safepoints table. For off-heap code objects
  // this may differ from instruction_size in that this will return the size of
  // the off-heap instruction stream rather than the on-heap trampoline located
  // at instruction_start.
  inline int InstructionSize(PtrComprCageBase cage_base);

  // Return the source position table for interpreter code.
  inline ByteArray SourcePositionTable(PtrComprCageBase cage_base,
                                       SharedFunctionInfo sfi);

  void DropStackFrameCache(PtrComprCageBase cage_base);

  // Returns the size of instructions and the metadata.
  inline int SizeIncludingMetadata(PtrComprCageBase cage_base);

  // Returns true if pc is inside this object's instructions.
  inline bool contains(Isolate* isolate, Address pc);

  // Returns the kind of the code.
  inline CodeKind kind(PtrComprCageBase cage_base);

  inline Builtin builtin_id(PtrComprCageBase cage_base);

  inline bool is_off_heap_trampoline(PtrComprCageBase cage_base);

  inline HandlerTable::CatchPrediction GetBuiltinCatchPrediction(
      PtrComprCageBase cage_base);

  DECL_CAST(AbstractCode)

  // The following predicates don't have the parameterless versions on
  // purpose - in order to avoid the expensive cage base computation that
  // should work for both regular V8 heap objects and external code space
  // objects.
  inline bool IsCode(PtrComprCageBase cage_base) const;
  inline bool IsCodeT(PtrComprCageBase cage_base) const;
  inline bool IsBytecodeArray(PtrComprCageBase cage_base) const;

  inline Code ToCode(PtrComprCageBase cage_base);
  inline CodeT ToCodeT(PtrComprCageBase cage_base);

  inline Code GetCode();
  inline CodeT GetCodeT();
  inline BytecodeArray GetBytecodeArray();

  // AbstractCode might be represented by both Code and non-Code objects and
  // thus regular comparison of tagged values might not be correct when
  // V8_EXTERNAL_CODE_SPACE is enabled. SafeEquals() must be used instead.
  constexpr bool operator==(AbstractCode other) const {
    return SafeEquals(other);
  }
  constexpr bool operator!=(AbstractCode other) const {
    return !SafeEquals(other);
  }

 private:
  inline ByteArray SourcePositionTableInternal(PtrComprCageBase cage_base);

  OBJECT_CONSTRUCTORS(AbstractCode, HeapObject);
};

// Dependent code is conceptually the list of {Code, DependencyGroup} tuples
// associated with an object, where the dependency group is a reason that could
// lead to a deopt of the corresponding code.
//
// Implementation details: DependentCode is a weak array list containing
// entries, where each entry consists of a (weak) Code object and the
// DependencyGroups bitset as a Smi.
//
// Note the underlying weak array list currently never shrinks physically (the
// contents may shrink).
// TODO(jgruber): Consider adding physical shrinking.
class DependentCode : public WeakArrayList {
 public:
  DECL_CAST(DependentCode)

  enum DependencyGroup {
    // Group of code objects that embed a transition to this map, and depend on
    // being deoptimized when the transition is replaced by a new version.
    kTransitionGroup = 1 << 0,
    // Group of code objects that omit run-time prototype checks for prototypes
    // described by this map. The group is deoptimized whenever the following
    // conditions hold, possibly invalidating the assumptions embedded in the
    // code:
    // a) A fast-mode object described by this map changes shape (and
    // transitions to a new map), or
    // b) A dictionary-mode prototype described by this map changes shape, the
    // const-ness of one of its properties changes, or its [[Prototype]]
    // changes (only the latter causes a transition).
    kPrototypeCheckGroup = 1 << 1,
    // Group of code objects that depends on global property values in property
    // cells not being changed.
    kPropertyCellChangedGroup = 1 << 2,
    // Group of code objects that omit run-time checks for field(s) introduced
    // by this map, i.e. for the field type.
    kFieldTypeGroup = 1 << 3,
    kFieldConstGroup = 1 << 4,
    kFieldRepresentationGroup = 1 << 5,
    // Group of code objects that omit run-time type checks for initial maps of
    // constructors.
    kInitialMapChangedGroup = 1 << 6,
    // Group of code objects that depends on tenuring information in
    // AllocationSites not being changed.
    kAllocationSiteTenuringChangedGroup = 1 << 7,
    // Group of code objects that depends on element transition information in
    // AllocationSites not being changed.
    kAllocationSiteTransitionChangedGroup = 1 << 8,
    // IMPORTANT: The last bit must fit into a Smi, i.e. into 31 bits.
  };
  using DependencyGroups = base::Flags<DependencyGroup, uint32_t>;

  static const char* DependencyGroupName(DependencyGroup group);

  // Register a dependency of {code} on {object}, of the kinds given by
  // {groups}.
  V8_EXPORT_PRIVATE static void InstallDependency(Isolate* isolate,
                                                  Handle<Code> code,
                                                  Handle<HeapObject> object,
                                                  DependencyGroups groups);

  template <typename ObjectT>
  static void DeoptimizeDependencyGroups(Isolate* isolate, ObjectT object,
                                         DependencyGroups groups);

  template <typename ObjectT>
  static bool MarkCodeForDeoptimization(ObjectT object,
                                        DependencyGroups groups);

  V8_EXPORT_PRIVATE static DependentCode empty_dependent_code(
      const ReadOnlyRoots& roots);
  static constexpr RootIndex kEmptyDependentCode =
      RootIndex::kEmptyWeakArrayList;

  // Constants exposed for tests.
  static constexpr int kSlotsPerEntry = 2;  // {code: weak Code, groups: Smi}.
  static constexpr int kCodeSlotOffset = 0;
  static constexpr int kGroupsSlotOffset = 1;

 private:
  // Get/Set {object}'s {DependentCode}.
  static DependentCode GetDependentCode(HeapObject object);
  static void SetDependentCode(Handle<HeapObject> object,
                               Handle<DependentCode> dep);

  static Handle<DependentCode> New(Isolate* isolate, DependencyGroups groups,
                                   Handle<Code> code);
  static Handle<DependentCode> InsertWeakCode(Isolate* isolate,
                                              Handle<DependentCode> entries,
                                              DependencyGroups groups,
                                              Handle<Code> code);

  bool MarkCodeForDeoptimization(DependencyGroups deopt_groups);

  void DeoptimizeDependencyGroups(Isolate* isolate, DependencyGroups groups);

  // The callback is called for all non-cleared entries, and should return true
  // iff the current entry should be cleared.
  using IterateAndCompactFn = std::function<bool(CodeT, DependencyGroups)>;
  void IterateAndCompact(const IterateAndCompactFn& fn);

  // Fills the given entry with the last non-cleared entry in this list, and
  // returns the new length after the last non-cleared entry has been moved.
  int FillEntryFromBack(int index, int length);

  static constexpr int LengthFor(int number_of_entries) {
    return number_of_entries * kSlotsPerEntry;
  }

  OBJECT_CONSTRUCTORS(DependentCode, WeakArrayList);
};

DEFINE_OPERATORS_FOR_FLAGS(DependentCode::DependencyGroups)

// BytecodeArray represents a sequence of interpreter bytecodes.
class BytecodeArray
    : public TorqueGeneratedBytecodeArray<BytecodeArray, FixedArrayBase> {
 public:
  static constexpr int SizeFor(int length) {
    return OBJECT_POINTER_ALIGN(kHeaderSize + length);
  }

  inline byte get(int index) const;
  inline void set(int index, byte value);

  inline Address GetFirstBytecodeAddress();

  inline int32_t frame_size() const;
  inline void set_frame_size(int32_t frame_size);

  // Note: The register count is derived from frame_size.
  inline int register_count() const;

  // Note: the parameter count includes the implicit 'this' receiver.
  inline int32_t parameter_count() const;
  inline void set_parameter_count(int32_t number_of_parameters);

  inline interpreter::Register incoming_new_target_or_generator_register()
      const;
  inline void set_incoming_new_target_or_generator_register(
      interpreter::Register incoming_new_target_or_generator_register);

  static constexpr int kBytecodeAgeSize = kUInt16Size;
  static_assert(kBytecodeAgeOffset + kBytecodeAgeSize - 1 ==
                kBytecodeAgeOffsetEnd);

  inline uint16_t bytecode_age() const;
  inline void set_bytecode_age(uint16_t age);

  inline bool HasSourcePositionTable() const;
  inline bool DidSourcePositionGenerationFail() const;

  // If source positions have not been collected or an exception has been thrown
  // this will return empty_byte_array.
  DECL_GETTER(SourcePositionTable, ByteArray)

  // Indicates that an attempt was made to collect source positions, but that it
  // failed most likely due to stack exhaustion. When in this state
  // |SourcePositionTable| will return an empty byte array rather than crashing
  // as it would if no attempt was ever made to collect source positions.
  inline void SetSourcePositionsFailedToCollect();

  inline int BytecodeArraySize() const;

  // Returns the size of bytecode and its metadata. This includes the size of
  // bytecode, constant pool, source position table, and handler table.
  DECL_GETTER(SizeIncludingMetadata, int)

  DECL_PRINTER(BytecodeArray)
  DECL_VERIFIER(BytecodeArray)

  V8_EXPORT_PRIVATE void PrintJson(std::ostream& os);
  V8_EXPORT_PRIVATE void Disassemble(std::ostream& os);

  V8_EXPORT_PRIVATE static void Disassemble(Handle<BytecodeArray> handle,
                                            std::ostream& os);

  void CopyBytecodesTo(BytecodeArray to);

  // Bytecode aging
  V8_EXPORT_PRIVATE bool IsOld() const;
  V8_EXPORT_PRIVATE void MakeOlder();

  // Clear uninitialized padding space. This ensures that the snapshot content
  // is deterministic.
  inline void clear_padding();

  // Maximal memory consumption for a single BytecodeArray.
  static const int kMaxSize = 512 * MB;
  // Maximal length of a single BytecodeArray.
  static const int kMaxLength = kMaxSize - kHeaderSize;

  class BodyDescriptor;

 private:
  // Hide accessors inherited from generated class. Use parameter_count instead.
  DECL_INT_ACCESSORS(parameter_size)

  TQ_OBJECT_CONSTRUCTORS(BytecodeArray)
};

// This class holds data required during deoptimization. It does not have its
// own instance type.
class DeoptimizationLiteralArray : public WeakFixedArray {
 public:
  // Getters for literals. These include runtime checks that the pointer was not
  // cleared, if the literal was held weakly.
  inline Object get(int index) const;
  inline Object get(PtrComprCageBase cage_base, int index) const;

  // Setter for literals. This will set the object as strong or weak depending
  // on Code::IsWeakObjectInOptimizedCode.
  inline void set(int index, Object value);

  DECL_CAST(DeoptimizationLiteralArray)

  OBJECT_CONSTRUCTORS(DeoptimizationLiteralArray, WeakFixedArray);
};

// DeoptimizationData is a fixed array used to hold the deoptimization data for
// optimized code.  It also contains information about functions that were
// inlined.  If N different functions were inlined then the first N elements of
// the literal array will contain these functions.
//
// It can be empty.
class DeoptimizationData : public FixedArray {
 public:
  // Layout description.  Indices in the array.
  static const int kTranslationByteArrayIndex = 0;
  static const int kInlinedFunctionCountIndex = 1;
  static const int kLiteralArrayIndex = 2;
  static const int kOsrBytecodeOffsetIndex = 3;
  static const int kOsrPcOffsetIndex = 4;
  static const int kOptimizationIdIndex = 5;
  static const int kSharedFunctionInfoIndex = 6;
  static const int kInliningPositionsIndex = 7;
  static const int kDeoptExitStartIndex = 8;
  static const int kEagerDeoptCountIndex = 9;
  static const int kLazyDeoptCountIndex = 10;
  static const int kFirstDeoptEntryIndex = 11;

  // Offsets of deopt entry elements relative to the start of the entry.
  static const int kBytecodeOffsetRawOffset = 0;
  static const int kTranslationIndexOffset = 1;
  static const int kPcOffset = 2;
#ifdef DEBUG
  static const int kNodeIdOffset = 3;
  static const int kDeoptEntrySize = 4;
#else   // DEBUG
  static const int kDeoptEntrySize = 3;
#endif  // DEBUG

// Simple element accessors.
#define DECL_ELEMENT_ACCESSORS(name, type) \
  inline type name() const;                \
  inline void Set##name(type value);

  DECL_ELEMENT_ACCESSORS(TranslationByteArray, TranslationArray)
  DECL_ELEMENT_ACCESSORS(InlinedFunctionCount, Smi)
  DECL_ELEMENT_ACCESSORS(LiteralArray, DeoptimizationLiteralArray)
  DECL_ELEMENT_ACCESSORS(OsrBytecodeOffset, Smi)
  DECL_ELEMENT_ACCESSORS(OsrPcOffset, Smi)
  DECL_ELEMENT_ACCESSORS(OptimizationId, Smi)
  DECL_ELEMENT_ACCESSORS(SharedFunctionInfo, Object)
  DECL_ELEMENT_ACCESSORS(InliningPositions, PodArray<InliningPosition>)
  DECL_ELEMENT_ACCESSORS(DeoptExitStart, Smi)
  DECL_ELEMENT_ACCESSORS(EagerDeoptCount, Smi)
  DECL_ELEMENT_ACCESSORS(LazyDeoptCount, Smi)

#undef DECL_ELEMENT_ACCESSORS

// Accessors for elements of the ith deoptimization entry.
#define DECL_ENTRY_ACCESSORS(name, type) \
  inline type name(int i) const;         \
  inline void Set##name(int i, type value);

  DECL_ENTRY_ACCESSORS(BytecodeOffsetRaw, Smi)
  DECL_ENTRY_ACCESSORS(TranslationIndex, Smi)
  DECL_ENTRY_ACCESSORS(Pc, Smi)
#ifdef DEBUG
  DECL_ENTRY_ACCESSORS(NodeId, Smi)
#endif  // DEBUG

#undef DECL_ENTRY_ACCESSORS

  inline BytecodeOffset GetBytecodeOffset(int i) const;

  inline void SetBytecodeOffset(int i, BytecodeOffset value);

  inline int DeoptCount();

  static const int kNotInlinedIndex = -1;

  // Returns the inlined function at the given position in LiteralArray, or the
  // outer function if index == kNotInlinedIndex.
  class SharedFunctionInfo GetInlinedFunction(int index);

  // Allocates a DeoptimizationData.
  static Handle<DeoptimizationData> New(Isolate* isolate, int deopt_entry_count,
                                        AllocationType allocation);

  // Return an empty DeoptimizationData.
  V8_EXPORT_PRIVATE static Handle<DeoptimizationData> Empty(Isolate* isolate);

  DECL_CAST(DeoptimizationData)

#ifdef ENABLE_DISASSEMBLER
  void DeoptimizationDataPrint(std::ostream& os);
#endif

 private:
  static int IndexForEntry(int i) {
    return kFirstDeoptEntryIndex + (i * kDeoptEntrySize);
  }

  static int LengthFor(int entry_count) { return IndexForEntry(entry_count); }

  OBJECT_CONSTRUCTORS(DeoptimizationData, FixedArray);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_CODE_H_
