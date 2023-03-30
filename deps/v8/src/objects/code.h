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
class Code;
class CodeDesc;
class ObjectIterator;
class SafepointScope;

class LocalFactory;
template <typename Impl>
class FactoryBase;

namespace interpreter {
class Register;
}  // namespace interpreter

#include "torque-generated/src/objects/code-tq.inc"

// Code is a container for data fields related to its associated
// {InstructionStream} object. Since {InstructionStream} objects reside on
// write-protected pages within the heap, its header fields need to be
// immutable.  Every InstructionStream object has an associated Code object,
// but not every Code object has an InstructionStream (e.g. for builtins).
//
// Embedded builtins consist of on-heap Code objects, with an out-of-line body
// section. Accessors (e.g. InstructionStart), redirect to the off-heap area.
// Metadata table offsets remain relative to MetadataStart(), i.e. they point
// into the off-heap metadata section. The off-heap layout is described in
// detail in the EmbeddedData class, but at a high level one can assume a
// dedicated, out-of-line, instruction and metadata section for each embedded
// builtin:
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
//
// TODO(jgruber): Code currently contains many aliases for InstructionStream
// functions. These will eventually move to the Code object. Once done, put all
// these declarations in a decent order and move over comments from the current
// declarations in InstructionStream.
class Code : public HeapObject {
 public:
  // When V8_EXTERNAL_CODE_SPACE is enabled, InstructionStream objects are
  // allocated in a separate pointer compression cage instead of the cage where
  // all the other objects are allocated.
  inline PtrComprCageBase code_cage_base() const;

  // Back-reference to the InstructionStream object.
  //
  // Note the cage-less accessor versions may not be called if the current Code
  // object is InReadOnlySpace. That may only be the case for Code objects
  // representing builtins, or in other words, Code objects for which
  // has_instruction_stream() is never true.
  DECL_GETTER(instruction_stream, InstructionStream)
  DECL_RELAXED_GETTER(instruction_stream, InstructionStream)
  DECL_ACCESSORS(raw_instruction_stream, Object)
  DECL_RELAXED_GETTER(raw_instruction_stream, Object)

  // Whether this Code object has an associated InstructionStream (embedded
  // builtins don't).
  //
  // Note there's a short amount of time during CodeBuilder::BuildInternal in
  // which the Code object has been allocated and initialized, but the
  // InstructionStream doesn't exist yet - in this situation,
  // has_instruction_stream is `false` but will change to `true` once
  // InstructionStream has also been initialized.
  inline bool has_instruction_stream() const;
  inline bool has_instruction_stream(RelaxedLoadTag) const;

  // Cached value of instruction_stream().InstructionStart().
  DECL_GETTER(code_entry_point, Address)

  // Aliases for code_entry_point for API compatibility with InstructionStream.
  inline Address InstructionStart() const;

  inline void SetInstructionStreamAndEntryPoint(
      Isolate* isolate_for_sandbox, InstructionStream code,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  inline void SetEntryPointForOffHeapBuiltin(Isolate* isolate_for_sandbox,
                                             Address entry);
  inline void SetCodeEntryPointForSerialization(Isolate* isolate,
                                                Address entry);
  // Updates the value of the code entry point. The code must be equal to
  // the code() value.
  inline void UpdateCodeEntryPoint(Isolate* isolate_for_sandbox,
                                   InstructionStream code);

  DECL_RELAXED_UINT16_ACCESSORS(kind_specific_flags)

  // Initializes internal flags field which stores cached values of some
  // properties of the respective InstructionStream object.
  inline void initialize_flags(CodeKind kind, Builtin builtin_id,
                               bool is_turbofanned, int stack_slots);

  // Clear uninitialized padding space. This ensures that the snapshot content
  // is deterministic.
  inline void clear_padding();
  // Clear the padding in the InstructionStream
  inline void ClearInstructionStreamPadding();

  // Flushes the instruction cache for the executable instructions of this code
  // object. Make sure to call this while the code is still writable.
  void FlushICache() const;

  DECL_PRIMITIVE_ACCESSORS(can_have_weak_objects, bool)
  DECL_PRIMITIVE_ACCESSORS(marked_for_deoptimization, bool)

  // [is_promise_rejection]: For kind BUILTIN tells whether the
  // exception thrown by the code will lead to promise rejection or
  // uncaught if both this and is_exception_caught is set.
  // Use GetBuiltinCatchPrediction to access this.
  DECL_PRIMITIVE_ACCESSORS(is_promise_rejection, bool)

  inline HandlerTable::CatchPrediction GetBuiltinCatchPrediction() const;

  DECL_PRIMITIVE_ACCESSORS(instruction_size, int)
  DECL_PRIMITIVE_ACCESSORS(metadata_size, int)
  // [handler_table_offset]: The offset where the exception handler table
  // starts.
  DECL_PRIMITIVE_ACCESSORS(handler_table_offset, int)
  // [unwinding_info_offset]: Offset of the unwinding info section.
  DECL_PRIMITIVE_ACCESSORS(unwinding_info_offset, int32_t)
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
  // [relocation_info]: InstructionStream relocation information
  DECL_ACCESSORS(relocation_info, ByteArray)
  DECL_PRIMITIVE_ACCESSORS(inlined_bytecode_size, unsigned)
  DECL_PRIMITIVE_ACCESSORS(osr_offset, BytecodeOffset)
  // [code_comments_offset]: Offset of the code comment section.
  DECL_PRIMITIVE_ACCESSORS(code_comments_offset, int)
  // [constant_pool offset]: Offset of the constant pool.
  DECL_PRIMITIVE_ACCESSORS(constant_pool_offset, int)

  // Unchecked accessors to be used during GC.
  inline ByteArray unchecked_relocation_info() const;
  inline FixedArray unchecked_deoptimization_data() const;

  inline CodeKind kind() const;
  inline Builtin builtin_id() const;
  inline bool is_builtin() const;

  inline bool is_optimized_code() const;
  inline bool is_wasm_code() const;

  inline bool is_interpreter_trampoline_builtin() const;
  inline bool is_baseline_trampoline_builtin() const;
  inline bool is_baseline_leave_frame_builtin() const;

  // Tells whether the code checks the tiering state in the function's feedback
  // vector.
  inline bool checks_tiering_state() const;

  // Tells whether the outgoing parameters of this code are tagged pointers.
  inline bool has_tagged_outgoing_params() const;

  // [is_maglevved]: Tells whether the code object was generated by the
  // Maglev optimizing compiler.
  inline bool is_maglevved() const;

  // [is_turbofanned]: Tells whether the code object was generated by the
  // TurboFan optimizing compiler.
  inline bool is_turbofanned() const;

  // [uses_safepoint_table]: Whether this InstructionStream object uses
  // safepoint tables (note the table may still be empty, see
  // has_safepoint_table).
  inline bool uses_safepoint_table() const;

  // [stack_slots]: If {uses_safepoint_table()}, the number of stack slots
  // reserved in the code prologue; otherwise 0.
  inline int stack_slots() const;

  inline ByteArray SourcePositionTable(Isolate* isolate,
                                       SharedFunctionInfo sfi) const;

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

  // [safepoint_table_offset]: The offset where the safepoint table starts.
  inline int safepoint_table_offset() const { return 0; }

  inline Address body_start() const;
  inline Address body_end() const;
  inline int body_size() const;

  inline Address metadata_start() const;
  inline Address metadata_end() const;

  inline Address handler_table_address() const;

  inline Address safepoint_table_address() const;

  inline int CodeSize() const;
  inline int SizeIncludingMetadata() const;

  // When builtins un-embedding is enabled for the Isolate
  // (see Isolate::is_short_builtin_calls_enabled()) then both embedded and
  // un-embedded builtins might be exeuted and thus two kinds of |pc|s might
  // appear on the stack.
  // Unlike the paremeterless versions of the functions above the below variants
  // ensure that the instruction start correspond to the given |pc| value.
  // Thus for off-heap trampoline InstructionStream objects the result might be
  // the instruction start/end of the embedded code stream or of un-embedded
  // one. For normal InstructionStream objects these functions just return the
  // instruction_start/end() values.
  // TODO(11527): remove these versions once the full solution is ready.
  inline Address InstructionStart(Isolate* isolate, Address pc) const;
  V8_EXPORT_PRIVATE Address OffHeapInstructionStart() const;
  V8_EXPORT_PRIVATE Address OffHeapInstructionStart(Isolate* isolate,
                                                    Address pc) const;
  inline Address InstructionEnd(Isolate* isolate, Address pc) const;
  V8_EXPORT_PRIVATE Address OffHeapInstructionEnd(Isolate* isolate,
                                                  Address pc) const;

  V8_EXPORT_PRIVATE bool OffHeapBuiltinContains(Isolate* isolate,
                                                Address pc) const;

  inline Address InstructionEnd() const;
  inline int InstructionSize() const;

  SafepointEntry GetSafepointEntry(Isolate* isolate, Address pc);
  MaglevSafepointEntry GetMaglevSafepointEntry(Isolate* isolate, Address pc);

  inline int GetOffsetFromInstructionStart(Isolate* isolate, Address pc) const;

  void SetMarkedForDeoptimization(Isolate* isolate, const char* reason);

  inline bool CanContainWeakObjects();

  inline bool IsWeakObject(HeapObject object);

  static inline bool IsWeakObjectInOptimizedCode(HeapObject object);

  static inline bool IsWeakObjectInDeoptimizationLiteralArray(Object object);

  // This function should be called only from GC.
  void ClearEmbeddedObjects(Heap* heap);

  // [embedded_objects_cleared]: If CodeKindIsOptimizedJSFunction(kind), tells
  // whether the embedded objects in the code marked for deoptimization were
  // cleared. Note that embedded_objects_cleared() implies
  // marked_for_deoptimization().
  inline bool embedded_objects_cleared() const;
  inline void set_embedded_objects_cleared(bool flag);

  // Migrate code from desc without flushing the instruction cache.
  void CopyFromNoFlush(ByteArray reloc_info, Heap* heap, const CodeDesc& desc);
  void RelocateFromDesc(ByteArray reloc_info, Heap* heap, const CodeDesc& desc);

  // Copy the RelocInfo portion of |desc| to |dest|. The ByteArray must be
  // exactly the same size as the RelocInfo in |desc|.
  static inline void CopyRelocInfoToByteArray(ByteArray dest,
                                              const CodeDesc& desc);

  bool IsIsolateIndependent(Isolate* isolate);

  inline uintptr_t GetBaselineStartPCForBytecodeOffset(int bytecode_offset,
                                                       BytecodeArray bytecodes);

  inline uintptr_t GetBaselineEndPCForBytecodeOffset(int bytecode_offset,
                                                     BytecodeArray bytecodes);

  // Returns true if the function is inlined in the code.
  bool Inlines(SharedFunctionInfo sfi);

  // Returns the PC of the next bytecode in execution order.
  // If the bytecode at the given offset is JumpLoop, the PC of the jump target
  // is returned. Other jumps are not allowed.
  // For other bytecodes this is equivalent to
  // GetBaselineEndPCForBytecodeOffset.
  inline uintptr_t GetBaselinePCForNextExecutedBytecode(
      int bytecode_offset, BytecodeArray bytecodes);

  inline int GetBytecodeOffsetForBaselinePC(Address baseline_pc,
                                            BytecodeArray bytecodes);

  inline void IterateDeoptimizationLiterals(RootVisitor* v);

  static inline Code FromTargetAddress(Address address);

#ifdef ENABLE_DISASSEMBLER
  V8_EXPORT_PRIVATE void Disassemble(const char* name, std::ostream& os,
                                     Isolate* isolate,
                                     Address current_pc = kNullAddress);
#endif  // ENABLE_DISASSEMBLER

  DECL_CAST(Code)

  // Dispatched behavior.
  DECL_PRINTER(Code)
  DECL_VERIFIER(Code)

// Layout description.
#define CODE_DATA_FIELDS(V)                                                   \
  /* Strong pointer fields. */                                                \
  V(kRelocationInfoOffset, kTaggedSize)                                       \
  V(kDeoptimizationDataOrInterpreterDataOffset, kTaggedSize)                  \
  V(kPositionTableOffset, kTaggedSize)                                        \
  V(kPointerFieldsStrongEndOffset, 0)                                         \
  /* Strong InstructionStream pointer fields. */                              \
  V(kInstructionStreamOffset, kTaggedSize)                                    \
  V(kCodePointerFieldsStrongEndOffset, 0)                                     \
  /* Raw data fields. */                                                      \
  /* Data or code not directly visited by GC directly starts here. */         \
  V(kDataStart, 0)                                                            \
  V(kCodeEntryPointOffset, kSystemPointerSize)                                \
  /* The serializer needs to copy bytes starting from here verbatim. */       \
  /* Objects embedded into code is visited via reloc info. */                 \
  V(kFlagsOffset, kInt32Size)                                                 \
  V(kBuiltinIdOffset, kInt16Size)                                             \
  V(kKindSpecificFlagsOffset, kInt16Size)                                     \
  V(kInstructionSizeOffset, kIntSize)                                         \
  V(kMetadataSizeOffset, kIntSize)                                            \
  V(kInlinedBytecodeSizeOffset, kIntSize)                                     \
  V(kOsrOffsetOffset, kInt32Size)                                             \
  /* Offsets describing inline metadata tables, relative to MetadataStart. */ \
  V(kHandlerTableOffsetOffset, kIntSize)                                      \
  V(kUnwindingInfoOffsetOffset, kInt32Size)                                   \
  V(kConstantPoolOffsetOffset, V8_EMBEDDED_CONSTANT_POOL_BOOL ? kIntSize : 0) \
  V(kCodeCommentsOffsetOffset, kIntSize)                                      \
  V(kUnalignedSize, OBJECT_POINTER_PADDING(kUnalignedSize))                   \
  /* Total size. */                                                           \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize, CODE_DATA_FIELDS)
#undef CODE_DATA_FIELDS

#ifdef V8_EXTERNAL_CODE_SPACE
  template <typename T>
  using ExternalCodeField =
      TaggedField<T, kInstructionStreamOffset, ExternalCodeCompressionScheme>;
#else
  template <typename T>
  using ExternalCodeField = TaggedField<T, kInstructionStreamOffset>;
#endif  // V8_EXTERNAL_CODE_SPACE

  class BodyDescriptor;

  // Flags layout.
#define FLAGS_BIT_FIELDS(V, _)      \
  V(KindField, CodeKind, 4, _)      \
  V(IsTurbofannedField, bool, 1, _) \
  V(StackSlotsField, int, 24, _)
  /* The other 3 bits are still free. */
  // TODO(v8:13784): merge this with KindSpecificFlags by dropping the
  // IsPromiseRejection field or taking one bit from the StackSlots field.

  DEFINE_BIT_FIELDS(FLAGS_BIT_FIELDS)
#undef FLAGS_BIT_FIELDS
  static_assert(kCodeKindCount <= KindField::kNumValues);
  static_assert(FLAGS_BIT_FIELDS_Ranges::kBitsCount == 29);
  static_assert(FLAGS_BIT_FIELDS_Ranges::kBitsCount <=
                FIELD_SIZE(kFlagsOffset) * kBitsPerByte);

  // KindSpecificFlags layout.
#define KIND_SPECIFIC_FLAGS_BIT_FIELDS(V, _)  \
  V(MarkedForDeoptimizationField, bool, 1, _) \
  V(EmbeddedObjectsClearedField, bool, 1, _)  \
  V(CanHaveWeakObjectsField, bool, 1, _)      \
  V(IsPromiseRejectionField, bool, 1, _)
  DEFINE_BIT_FIELDS(KIND_SPECIFIC_FLAGS_BIT_FIELDS)
#undef CODE_KIND_SPECIFIC_FLAGS_BIT_FIELDS
  static_assert(KIND_SPECIFIC_FLAGS_BIT_FIELDS_Ranges::kBitsCount == 4);
  static_assert(KIND_SPECIFIC_FLAGS_BIT_FIELDS_Ranges::kBitsCount <=
                FIELD_SIZE(Code::kKindSpecificFlagsOffset) * kBitsPerByte);

  // The {marked_for_deoptimization} field is accessed from generated code.
  static const int kMarkedForDeoptimizationBit =
      MarkedForDeoptimizationField::kShift;

  class OptimizedCodeIterator;

 private:
  inline void init_code_entry_point(Isolate* isolate, Address initial_value);
  inline void set_code_entry_point(Isolate* isolate, Address value);

  // Contains cached values of some flags of the from the respective
  // InstructionStream object.
  DECL_RELAXED_UINT16_ACCESSORS(flags)

  V8_EXPORT_PRIVATE Address OffHeapInstructionEnd() const;
  V8_EXPORT_PRIVATE int OffHeapInstructionSize() const;
  V8_EXPORT_PRIVATE Address OffHeapMetadataStart() const;
  V8_EXPORT_PRIVATE Address OffHeapMetadataEnd() const;
  V8_EXPORT_PRIVATE int OffHeapMetadataSize() const;
  V8_EXPORT_PRIVATE Address OffHeapSafepointTableAddress() const;
  V8_EXPORT_PRIVATE int OffHeapSafepointTableSize() const;
  V8_EXPORT_PRIVATE Address OffHeapHandlerTableAddress() const;
  V8_EXPORT_PRIVATE int OffHeapHandlerTableSize() const;
  V8_EXPORT_PRIVATE Address OffHeapConstantPoolAddress() const;
  V8_EXPORT_PRIVATE int OffHeapConstantPoolSize() const;
  V8_EXPORT_PRIVATE Address OffHeapCodeCommentsAddress() const;
  V8_EXPORT_PRIVATE int OffHeapCodeCommentsSize() const;
  V8_EXPORT_PRIVATE Address OffHeapUnwindingInfoAddress() const;
  V8_EXPORT_PRIVATE int OffHeapUnwindingInfoSize() const;
  V8_EXPORT_PRIVATE int OffHeapStackSlots() const;

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

  template <typename IsolateT>
  friend class Deserializer;
  friend class ReadOnlyDeserializer;  // For init_code_entry_point.
  friend class GcSafeCode;  // For OffHeapFoo functions.
  friend Factory;
  friend FactoryBase<Factory>;
  friend FactoryBase<LocalFactory>;
  friend Isolate;

  OBJECT_CONSTRUCTORS(Code, HeapObject);
};

// A Code object when used in situations where gc might be in progress. The
// underlying pointer is guaranteed to be a Code object.
//
// Semantics around Code and InstructionStream objects are quite delicate when
// GC is in progress and objects are currently being moved, because the
// tightly-coupled object pair {Code,InstructionStream} are conceptually
// treated as a single object in our codebase, and we frequently convert
// between the two. However, during GC, extra care must be taken when accessing
// the `Code::instruction_stream` and `InstructionStream::code` slots because
// they may contain forwarding pointers.
//
// This class a) clarifies at use sites that we're dealing with a Code object
// in a situation that requires special semantics, and b) safely implements
// related functions.
//
// Note that both the underlying Code object and the associated
// InstructionStream may be forwarding pointers, thus type checks and normal
// (checked) casts do not work on GcSafeCode.
class GcSafeCode : public HeapObject {
 public:
  DECL_CAST(GcSafeCode)

  // Use with care, this casts away knowledge that we're dealing with a
  // special-semantics object.
  inline Code UnsafeCastToCode() const;

  // Safe accessors (these just forward to Code methods).
  inline Address InstructionStart() const;
  inline Address InstructionEnd() const;
  inline bool is_builtin() const;
  inline Builtin builtin_id() const;
  inline CodeKind kind() const;
  inline bool is_interpreter_trampoline_builtin() const;
  inline bool is_baseline_trampoline_builtin() const;
  inline bool is_baseline_leave_frame_builtin() const;
  inline bool has_instruction_stream() const;
  inline bool is_maglevved() const;
  inline bool is_turbofanned() const;
  inline bool has_tagged_outgoing_params() const;
  inline bool marked_for_deoptimization() const;
  inline Object raw_instruction_stream() const;
  inline Address constant_pool() const;
  inline int stack_slots() const;

  inline int GetOffsetFromInstructionStart(Isolate* isolate, Address pc) const;
  inline Address InstructionStart(Isolate* isolate, Address pc) const;
  inline Address InstructionEnd(Isolate* isolate, Address pc) const;
  inline bool CanDeoptAt(Isolate* isolate, Address pc) const;
  inline Object raw_instruction_stream(PtrComprCageBase code_cage_base) const;

  // Accessors that had to be modified to be used in GC settings.
  inline Address SafepointTableAddress() const;

 private:
  OBJECT_CONSTRUCTORS(GcSafeCode, HeapObject);
};

// InstructionStream contains the instruction stream for V8-generated code
// objects.
//
// When V8_EXTERNAL_CODE_SPACE is enabled, InstructionStream objects are
// allocated in a separate pointer compression cage instead of the cage where
// all the other objects are allocated.
class InstructionStream : public HeapObject {
 public:
  NEVER_READ_ONLY_SPACE

  // All InstructionStream objects have the following layout:
  //
  //  +--------------------------+
  //  |          header          |
  //  | padded to code alignment |
  //  +--------------------------+  <-- body_start()
  //  |       instructions       |   == instruction_start()
  //  |           ...            |
  //  | padded to meta alignment |      see kMetadataAlignment
  //  +--------------------------+  <-- instruction_end()
  //  |         metadata         |   == metadata_start() (MS)
  //  |           ...            |
  //  |                          |  <-- MS + handler_table_offset()
  //  |                          |  <-- MS + constant_pool_offset()
  //  |                          |  <-- MS + code_comments_offset()
  //  |                          |  <-- MS + unwinding_info_offset()
  //  | padded to obj alignment  |
  //  +--------------------------+  <-- metadata_end() == body_end()
  //  | padded to code alignment |
  //  +--------------------------+
  //
  // In other words, the variable-size 'body' consists of 'instructions' and
  // 'metadata'.

  // Constants for use in static asserts, stating whether the body is adjacent,
  // i.e. instructions and metadata areas are adjacent.
  static constexpr bool kOnHeapBodyIsContiguous = true;
  static constexpr bool kOffHeapBodyIsContiguous = false;
  static constexpr bool kBodyIsContiguous =
      kOnHeapBodyIsContiguous && kOffHeapBodyIsContiguous;

  inline Address instruction_start() const;

  // The metadata section is aligned to this value.
  static constexpr int kMetadataAlignment = kIntSize;

  // [code]: The associated Code object.
  DECL_RELEASE_ACQUIRE_ACCESSORS(code, Code)
  DECL_RELEASE_ACQUIRE_ACCESSORS(raw_code, HeapObject)

  // A convenience wrapper around raw_code that will do an unchecked cast for
  // you.
  inline Code unchecked_code() const;

  // The entire code object including its header is copied verbatim to the
  // snapshot so that it can be written in one, fast, memcpy during
  // deserialization. The deserializer will overwrite some pointers, rather
  // like a runtime linker, but the random allocation addresses used in the
  // mksnapshot process would still be present in the unlinked snapshot data,
  // which would make snapshot production non-reproducible. This method wipes
  // out the to-be-overwritten header data for reproducible snapshots.
  inline void WipeOutHeader();

  // When V8_EXTERNAL_CODE_SPACE is enabled, InstructionStream objects are
  // allocated in a separate pointer compression cage instead of the cage where
  // all the other objects are allocated. This field contains cage base value
  // which is used for decompressing the references to non-InstructionStream
  // objects (map, deoptimization_data, etc.).
  inline PtrComprCageBase main_cage_base() const;
  inline PtrComprCageBase main_cage_base(RelaxedLoadTag) const;
  inline void set_main_cage_base(Address cage_base, RelaxedStoreTag);

  static inline InstructionStream FromTargetAddress(Address address);
  static inline InstructionStream FromEntryAddress(Address location_of_address);

  // InstructionStream entry point.
  inline Address entry() const;

  // Returns true if pc is inside this object's instructions.
  inline bool contains(Isolate* isolate, Address pc);

  // Relocate the code by delta bytes. Called to signal that this code
  // object has been moved by delta bytes.
  void Relocate(intptr_t delta);

  // Returns the object size for a given body (used for allocation).
  static int SizeFor(int body_size) {
    return RoundUp(kHeaderSize + body_size, kCodeAlignment);
  }

  inline int CodeSize() const;

  // Hides HeapObject::Size(...) and redirects queries to CodeSize().
  DECL_GETTER(Size, int)

  DECL_CAST(InstructionStream)

  // Dispatched behavior.
  DECL_PRINTER(InstructionStream)
  DECL_VERIFIER(InstructionStream)

  inline HandlerTable::CatchPrediction GetBuiltinCatchPrediction() const;

  // Layout description.
#define ISTREAM_FIELDS(V)                                             \
  V(kCodeOffset, kTaggedSize)                                         \
  /* Data or code not directly visited by GC directly starts here. */ \
  V(kDataStart, 0)                                                    \
  V(kMainCageBaseUpper32BitsOffset,                                   \
    V8_EXTERNAL_CODE_SPACE_BOOL ? kTaggedSize : 0)                    \
  V(kHeaderSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize, ISTREAM_FIELDS)
#undef ISTREAM_FIELDS

  static_assert(kCodeAlignment > kHeaderSize);
  // We do two things to ensure kCodeAlignment of the entry address:
  // 1) add kCodeAlignmentMinusCodeHeader padding once in the beginning of every
  //    MemoryChunk
  // 2) Round up all IStream allocations to a multiple of kCodeAlignment
  // Together, the IStream object itself will always start at offset
  // kCodeAlignmentMinusCodeHeader, which aligns the entry to kCodeAlignment.
  static constexpr int kCodeAlignmentMinusCodeHeader =
      kCodeAlignment - kHeaderSize;

  class BodyDescriptor;

  static const int kArgumentsBits = 16;
  // Reserve one argument count value as the "don't adapt arguments" sentinel.
  static const int kMaxArguments = (1 << kArgumentsBits) - 2;

 private:
  friend class RelocIterator;
  friend class EvacuateVisitorBase;

  inline Code GCSafeCode(AcquireLoadTag) const;

  bool is_promise_rejection() const;

  OBJECT_CONSTRUCTORS(InstructionStream, HeapObject);
};

class Code::OptimizedCodeIterator {
 public:
  explicit OptimizedCodeIterator(Isolate* isolate);
  OptimizedCodeIterator(const OptimizedCodeIterator&) = delete;
  OptimizedCodeIterator& operator=(const OptimizedCodeIterator&) = delete;
  Code Next();

 private:
  Isolate* isolate_;
  std::unique_ptr<SafepointScope> safepoint_scope_;
  std::unique_ptr<ObjectIterator> object_iterator_;
  enum { kIteratingCodeSpace, kIteratingCodeLOSpace, kDone } state_;

  DISALLOW_GARBAGE_COLLECTION(no_gc)
};

// Helper functions for converting InstructionStream objects to
// Code and back.
inline Code ToCode(InstructionStream code);
inline InstructionStream FromCode(Code code);
inline InstructionStream FromCode(Code code, Isolate* isolate, RelaxedLoadTag);
inline InstructionStream FromCode(Code code, PtrComprCageBase, RelaxedLoadTag);

// AbstractCode is a helper wrapper around {Code|BytecodeArray}.
class AbstractCode : public HeapObject {
 public:
  int SourcePosition(PtrComprCageBase cage_base, int offset);
  int SourceStatementPosition(PtrComprCageBase cage_base, int offset);

  inline Address InstructionStart(PtrComprCageBase cage_base);
  inline Address InstructionEnd(PtrComprCageBase cage_base);
  inline int InstructionSize(PtrComprCageBase cage_base);

  // Return the source position table for interpreter code.
  inline ByteArray SourcePositionTable(Isolate* isolate,
                                       SharedFunctionInfo sfi);

  void DropStackFrameCache(PtrComprCageBase cage_base);

  // Returns the size of instructions and the metadata.
  inline int SizeIncludingMetadata(PtrComprCageBase cage_base);

  // Returns true if pc is inside this object's instructions.
  inline bool contains(Isolate* isolate, Address pc);

  // Returns the kind of the code.
  inline CodeKind kind(PtrComprCageBase cage_base);

  inline Builtin builtin_id(PtrComprCageBase cage_base);

  inline bool has_instruction_stream(PtrComprCageBase cage_base);

  inline HandlerTable::CatchPrediction GetBuiltinCatchPrediction(
      PtrComprCageBase cage_base);

  DECL_CAST(AbstractCode)

  inline bool IsCode(PtrComprCageBase cage_base) const;
  inline bool IsBytecodeArray(PtrComprCageBase cage_base) const;

  inline Code GetCode();
  inline BytecodeArray GetBytecodeArray();

 private:
  inline ByteArray SourcePositionTableInternal(PtrComprCageBase cage_base);

  OBJECT_CONSTRUCTORS(AbstractCode, HeapObject);
};

// Dependent code is conceptually the list of {InstructionStream,
// DependencyGroup} tuples associated with an object, where the dependency group
// is a reason that could lead to a deopt of the corresponding code.
//
// Implementation details: DependentCode is a weak array list containing
// entries, where each entry consists of a (weak) InstructionStream object and
// the DependencyGroups bitset as a Smi.
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
  static bool MarkCodeForDeoptimization(Isolate* isolate, ObjectT object,
                                        DependencyGroups groups);

  V8_EXPORT_PRIVATE static DependentCode empty_dependent_code(
      const ReadOnlyRoots& roots);
  static constexpr RootIndex kEmptyDependentCode =
      RootIndex::kEmptyWeakArrayList;

  // Constants exposed for tests.
  static constexpr int kSlotsPerEntry =
      2;  // {code: weak InstructionStream, groups: Smi}.
  static constexpr int kCodeSlotOffset = 0;
  static constexpr int kGroupsSlotOffset = 1;

 private:
  // Get/Set {object}'s {DependentCode}.
  static DependentCode GetDependentCode(HeapObject object);
  static void SetDependentCode(Handle<HeapObject> object,
                               Handle<DependentCode> dep);

  static Handle<DependentCode> InsertWeakCode(Isolate* isolate,
                                              Handle<DependentCode> entries,
                                              DependencyGroups groups,
                                              Handle<Code> code);

  bool MarkCodeForDeoptimization(Isolate* isolate,
                                 DependencyGroups deopt_groups);

  void DeoptimizeDependencyGroups(Isolate* isolate, DependencyGroups groups);

  // The callback is called for all non-cleared entries, and should return true
  // iff the current entry should be cleared.
  using IterateAndCompactFn = std::function<bool(Code, DependencyGroups)>;
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

  // Raw accessors to access these fields during code cache deserialization.
  DECL_GETTER(raw_constant_pool, Object)
  DECL_GETTER(raw_handler_table, Object)
  DECL_GETTER(raw_source_position_table, Object)

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
  // on InstructionStream::IsWeakObjectInOptimizedCode.
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
