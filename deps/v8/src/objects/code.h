// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_CODE_H_
#define V8_OBJECTS_CODE_H_

#include "src/base/bit-field.h"
#include "src/codegen/handler-table.h"
#include "src/deoptimizer/translation-array.h"
#include "src/objects/code-kind.h"
#include "src/objects/contexts.h"
#include "src/objects/fixed-array.h"
#include "src/objects/heap-object.h"
#include "src/objects/objects.h"
#include "src/objects/struct.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class ByteArray;
class BytecodeArray;
class CodeDataContainer;
class CodeDesc;

namespace interpreter {
class Register;
}  // namespace interpreter

// CodeDataContainer is a container for all mutable fields associated with its
// referencing {Code} object. Since {Code} objects reside on write-protected
// pages within the heap, its header fields need to be immutable. There always
// is a 1-to-1 relation between {Code} and {CodeDataContainer}, the referencing
// field {Code::code_data_container} itself is immutable.
class CodeDataContainer : public HeapObject {
 public:
  NEVER_READ_ONLY_SPACE
  DECL_ACCESSORS(next_code_link, Object)
  DECL_INT_ACCESSORS(kind_specific_flags)

  // Clear uninitialized padding space. This ensures that the snapshot content
  // is deterministic.
  inline void clear_padding();

  DECL_CAST(CodeDataContainer)

  // Dispatched behavior.
  DECL_PRINTER(CodeDataContainer)
  DECL_VERIFIER(CodeDataContainer)

// Layout description.
#define CODE_DATA_FIELDS(V)                                 \
  /* Weak pointer fields. */                                \
  V(kPointerFieldsStrongEndOffset, 0)                       \
  V(kNextCodeLinkOffset, kTaggedSize)                       \
  V(kPointerFieldsWeakEndOffset, 0)                         \
  /* Raw data fields. */                                    \
  V(kKindSpecificFlagsOffset, kInt32Size)                   \
  V(kUnalignedSize, OBJECT_POINTER_PADDING(kUnalignedSize)) \
  /* Total size. */                                         \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize, CODE_DATA_FIELDS)
#undef CODE_DATA_FIELDS

  class BodyDescriptor;

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
  V8_EXPORT_PRIVATE Address OffHeapInstructionStart() const;

  inline Address raw_instruction_end() const;
  inline Address InstructionEnd() const;
  V8_EXPORT_PRIVATE Address OffHeapInstructionEnd() const;

  inline int raw_instruction_size() const;
  inline void set_raw_instruction_size(int value);
  inline int InstructionSize() const;
  V8_EXPORT_PRIVATE int OffHeapInstructionSize() const;

  inline Address raw_metadata_start() const;
  inline Address MetadataStart() const;
  V8_EXPORT_PRIVATE Address OffHeapMetadataStart() const;
  inline Address raw_metadata_end() const;
  inline Address MetadataEnd() const;
  V8_EXPORT_PRIVATE Address OffHeapMetadataEnd() const;
  inline int raw_metadata_size() const;
  inline void set_raw_metadata_size(int value);
  inline int MetadataSize() const;
  int OffHeapMetadataSize() const;

  // The metadata section is aligned to this value.
  static constexpr int kMetadataAlignment = kIntSize;

  // [safepoint_table_offset]: The offset where the safepoint table starts.
  inline int safepoint_table_offset() const { return 0; }
  Address SafepointTableAddress() const;
  int safepoint_table_size() const;
  bool has_safepoint_table() const;

  // [handler_table_offset]: The offset where the exception handler table
  // starts.
  inline int handler_table_offset() const;
  inline void set_handler_table_offset(int offset);
  Address HandlerTableAddress() const;
  int handler_table_size() const;
  bool has_handler_table() const;

  // [constant_pool offset]: Offset of the constant pool.
  inline int constant_pool_offset() const;
  inline void set_constant_pool_offset(int offset);
  inline Address constant_pool() const;
  int constant_pool_size() const;
  bool has_constant_pool() const;

  // [code_comments_offset]: Offset of the code comment section.
  inline int code_comments_offset() const;
  inline void set_code_comments_offset(int offset);
  inline Address code_comments() const;
  V8_EXPORT_PRIVATE int code_comments_size() const;
  V8_EXPORT_PRIVATE bool has_code_comments() const;

  // [unwinding_info_offset]: Offset of the unwinding info section.
  inline int32_t unwinding_info_offset() const;
  inline void set_unwinding_info_offset(int32_t offset);
  inline Address unwinding_info_start() const;
  inline Address unwinding_info_end() const;
  inline int unwinding_info_size() const;
  inline bool has_unwinding_info() const;

#ifdef ENABLE_DISASSEMBLER
  const char* GetName(Isolate* isolate) const;
  V8_EXPORT_PRIVATE void Disassemble(const char* name, std::ostream& os,
                                     Isolate* isolate,
                                     Address current_pc = kNullAddress);
#endif

  // [relocation_info]: Code relocation information
  DECL_ACCESSORS(relocation_info, ByteArray)

  // This function should be called only from GC.
  void ClearEmbeddedObjects(Heap* heap);

  // [deoptimization_data]: Array containing data for deopt.
  DECL_ACCESSORS(deoptimization_data, FixedArray)

  // [source_position_table]: ByteArray for the source positions table.
  DECL_ACCESSORS(source_position_table, Object)

  // If source positions have not been collected or an exception has been thrown
  // this will return empty_byte_array.
  inline ByteArray SourcePositionTable() const;

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
  inline bool is_baseline_prologue_builtin() const;
  inline bool is_baseline_leave_frame_builtin() const;

  // Tells whether the code checks the optimization marker in the function's
  // feedback vector.
  inline bool checks_optimization_marker() const;

  // Tells whether the outgoing parameters of this code are tagged pointers.
  inline bool has_tagged_outgoing_params() const;

  // [is_turbofanned]: Tells whether the code object was generated by the
  // TurboFan optimizing compiler.
  inline bool is_turbofanned() const;

  // [can_have_weak_objects]: If CodeKindIsOptimizedJSFunction(kind), tells
  // whether the embedded objects in code should be treated weakly.
  inline bool can_have_weak_objects() const;
  inline void set_can_have_weak_objects(bool value);

  // [builtin_index]: For builtins, tells which builtin index the code object
  // has. The builtin index is a non-negative integer for builtins, and
  // Builtins::kNoBuiltinId (-1) otherwise.
  inline int builtin_index() const;
  inline void set_builtin_index(int id);
  inline bool is_builtin() const;

  inline unsigned inlined_bytecode_size() const;
  inline void set_inlined_bytecode_size(unsigned size);

  inline bool has_safepoint_info() const;

  // [stack_slots]: If {has_safepoint_info()}, the number of stack slots
  // reserved in the code prologue.
  inline int stack_slots() const;

  // [marked_for_deoptimization]: If CodeKindCanDeoptimize(kind), tells whether
  // the code is going to be deoptimized.
  inline bool marked_for_deoptimization() const;
  inline void set_marked_for_deoptimization(bool flag);

  // [deoptimization_count]: If CodeKindCanDeoptimize(kind). In turboprop we
  // retain the deoptimized code on soft deopts for a certain number of soft
  // deopts. This field keeps track of the number of deoptimizations we have
  // seen so far.
  inline int deoptimization_count() const;
  inline void increment_deoptimization_count();

  // [embedded_objects_cleared]: If CodeKindIsOptimizedJSFunction(kind), tells
  // whether the embedded objects in the code marked for deoptimization were
  // cleared. Note that embedded_objects_cleared() implies
  // marked_for_deoptimization().
  inline bool embedded_objects_cleared() const;
  inline void set_embedded_objects_cleared(bool flag);

  // [deopt_already_counted]: If CodeKindCanDeoptimize(kind), tells whether
  // the code was already deoptimized.
  inline bool deopt_already_counted() const;
  inline void set_deopt_already_counted(bool flag);

  // [is_promise_rejection]: For kind BUILTIN tells whether the
  // exception thrown by the code will lead to promise rejection or
  // uncaught if both this and is_exception_caught is set.
  // Use GetBuiltinCatchPrediction to access this.
  inline void set_is_promise_rejection(bool flag);

  // [is_exception_caught]: For kind BUILTIN tells whether the
  // exception thrown by the code will be caught internally or
  // uncaught if both this and is_promise_rejection is set.
  // Use GetBuiltinCatchPrediction to access this.
  inline void set_is_exception_caught(bool flag);

  // [is_off_heap_trampoline]: For kind BUILTIN tells whether
  // this is a trampoline to an off-heap builtin.
  inline bool is_off_heap_trampoline() const;

  // Get the safepoint entry for the given pc.
  SafepointEntry GetSafepointEntry(Address pc);

  // The entire code object including its header is copied verbatim to the
  // snapshot so that it can be written in one, fast, memcpy during
  // deserialization. The deserializer will overwrite some pointers, rather
  // like a runtime linker, but the random allocation addresses used in the
  // mksnapshot process would still be present in the unlinked snapshot data,
  // which would make snapshot production non-reproducible. This method wipes
  // out the to-be-overwritten header data for reproducible snapshots.
  inline void WipeOutHeader();

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
  inline int SizeIncludingMetadata() const;

  // Returns the address of the first relocation info (read backwards!).
  inline byte* relocation_start() const;

  // Returns the address right after the relocation info (read backwards!).
  inline byte* relocation_end() const;

  // Code entry point.
  inline Address entry() const;

  // Returns true if pc is inside this object's instructions.
  inline bool contains(Address pc);

  // Relocate the code by delta bytes. Called to signal that this code
  // object has been moved by delta bytes.
  void Relocate(intptr_t delta);

  // Migrate code from desc without flushing the instruction cache.
  void CopyFromNoFlush(Heap* heap, const CodeDesc& desc);

  // Copy the RelocInfo portion of |desc| to |dest|. The ByteArray must be
  // exactly the same size as the RelocInfo in |desc|.
  static inline void CopyRelocInfoToByteArray(ByteArray dest,
                                              const CodeDesc& desc);

  inline uintptr_t GetBaselinePCForBytecodeOffset(int bytecode_offset,
                                                  bool precise = true);
  inline int GetBytecodeOffsetForBaselinePC(Address baseline_pc);

  // Flushes the instruction cache for the executable instructions of this code
  // object. Make sure to call this while the code is still writable.
  void FlushICache() const;

  // Returns the object size for a given body (used for allocation).
  static int SizeFor(int body_size) {
    return RoundUp(kHeaderSize + body_size, kCodeAlignment);
  }

  DECL_CAST(Code)

  // Dispatched behavior.
  inline int CodeSize() const;

  DECL_PRINTER(Code)
  DECL_VERIFIER(Code)

  bool CanDeoptAt(Address pc);

  void SetMarkedForDeoptimization(const char* reason);

  inline HandlerTable::CatchPrediction GetBuiltinCatchPrediction();

  bool IsIsolateIndependent(Isolate* isolate);
  bool IsNativeContextIndependent(Isolate* isolate);

  inline bool CanContainWeakObjects();

  inline bool IsWeakObject(HeapObject object);

  static inline bool IsWeakObjectInOptimizedCode(HeapObject object);

  // Returns false if this is an embedded builtin Code object that's in
  // read_only_space and hence doesn't have execute permissions.
  inline bool IsExecutable();

  // Returns true if the function is inlined in the code.
  bool Inlines(SharedFunctionInfo sfi);

  class OptimizedCodeIterator;

  // Layout description.
#define CODE_FIELDS(V)                                                        \
  V(kRelocationInfoOffset, kTaggedSize)                                       \
  V(kDeoptimizationDataOffset, kTaggedSize)                                   \
  V(kSourcePositionTableOffset, kTaggedSize)                                  \
  V(kCodeDataContainerOffset, kTaggedSize)                                    \
  /* Data or code not directly visited by GC directly starts here. */         \
  /* The serializer needs to copy bytes starting from here verbatim. */       \
  /* Objects embedded into code is visited via reloc info. */                 \
  V(kDataStart, 0)                                                            \
  V(kInstructionSizeOffset, kIntSize)                                         \
  V(kMetadataSizeOffset, kIntSize)                                            \
  V(kFlagsOffset, kInt32Size)                                                 \
  V(kBuiltinIndexOffset, kIntSize)                                            \
  V(kInlinedBytecodeSizeOffset, kIntSize)                                     \
  /* Offsets describing inline metadata tables, relative to MetadataStart. */ \
  V(kHandlerTableOffsetOffset, kIntSize)                                      \
  V(kConstantPoolOffsetOffset,                                                \
    FLAG_enable_embedded_constant_pool ? kIntSize : 0)                        \
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
  static constexpr int kHeaderPaddingSize = COMPRESS_POINTERS_BOOL ? 12 : 24;
#elif V8_TARGET_ARCH_MIPS64
  static constexpr int kHeaderPaddingSize = 24;
#elif V8_TARGET_ARCH_X64
  static constexpr int kHeaderPaddingSize = COMPRESS_POINTERS_BOOL ? 12 : 24;
#elif V8_TARGET_ARCH_ARM
  static constexpr int kHeaderPaddingSize = 12;
#elif V8_TARGET_ARCH_IA32
  static constexpr int kHeaderPaddingSize = 12;
#elif V8_TARGET_ARCH_MIPS
  static constexpr int kHeaderPaddingSize = 12;
#elif V8_TARGET_ARCH_PPC64
  static constexpr int kHeaderPaddingSize =
      FLAG_enable_embedded_constant_pool ? (COMPRESS_POINTERS_BOOL ? 8 : 20)
                                         : (COMPRESS_POINTERS_BOOL ? 12 : 24);
#elif V8_TARGET_ARCH_S390X
  static constexpr int kHeaderPaddingSize = COMPRESS_POINTERS_BOOL ? 12 : 24;
#elif V8_TARGET_ARCH_RISCV64
  static constexpr int kHeaderPaddingSize = 24;
#else
#error Unknown architecture.
#endif
  STATIC_ASSERT(FIELD_SIZE(kOptionalPaddingOffset) == kHeaderPaddingSize);

  class BodyDescriptor;

  // Flags layout.  base::BitField<type, shift, size>.
#define CODE_FLAGS_BIT_FIELDS(V, _)    \
  V(KindField, CodeKind, 4, _)         \
  V(IsTurbofannedField, bool, 1, _)    \
  V(StackSlotsField, int, 24, _)       \
  V(IsOffHeapTrampoline, bool, 1, _)
  DEFINE_BIT_FIELDS(CODE_FLAGS_BIT_FIELDS)
#undef CODE_FLAGS_BIT_FIELDS
  STATIC_ASSERT(kCodeKindCount <= KindField::kNumValues);
  STATIC_ASSERT(CODE_FLAGS_BIT_FIELDS_Ranges::kBitsCount == 30);
  STATIC_ASSERT(CODE_FLAGS_BIT_FIELDS_Ranges::kBitsCount <=
                FIELD_SIZE(kFlagsOffset) * kBitsPerByte);

  // KindSpecificFlags layout.
#define CODE_KIND_SPECIFIC_FLAGS_BIT_FIELDS(V, _) \
  V(MarkedForDeoptimizationField, bool, 1, _)     \
  V(EmbeddedObjectsClearedField, bool, 1, _)      \
  V(DeoptAlreadyCountedField, bool, 1, _)         \
  V(CanHaveWeakObjectsField, bool, 1, _)          \
  V(IsPromiseRejectionField, bool, 1, _)          \
  V(IsExceptionCaughtField, bool, 1, _)           \
  V(DeoptCountField, int, 4, _)
  DEFINE_BIT_FIELDS(CODE_KIND_SPECIFIC_FLAGS_BIT_FIELDS)
#undef CODE_KIND_SPECIFIC_FLAGS_BIT_FIELDS
  STATIC_ASSERT(CODE_KIND_SPECIFIC_FLAGS_BIT_FIELDS_Ranges::kBitsCount == 10);
  STATIC_ASSERT(CODE_KIND_SPECIFIC_FLAGS_BIT_FIELDS_Ranges::kBitsCount <=
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

  bool is_promise_rejection() const;
  bool is_exception_caught() const;

  OBJECT_CONSTRUCTORS(Code, HeapObject);
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

class AbstractCode : public HeapObject {
 public:
  NEVER_READ_ONLY_SPACE

  int SourcePosition(int offset);
  int SourceStatementPosition(int offset);

  // Returns the address of the first instruction.
  inline Address raw_instruction_start();

  // Returns the address of the first instruction. For off-heap code objects
  // this differs from instruction_start (which would point to the off-heap
  // trampoline instead).
  inline Address InstructionStart();

  // Returns the address right after the last instruction.
  inline Address raw_instruction_end();

  // Returns the address right after the last instruction. For off-heap code
  // objects this differs from instruction_end (which would point to the
  // off-heap trampoline instead).
  inline Address InstructionEnd();

  // Returns the size of the code instructions.
  inline int raw_instruction_size();

  // Returns the size of the native instructions, including embedded
  // data such as the safepoints table. For off-heap code objects
  // this may differ from instruction_size in that this will return the size of
  // the off-heap instruction stream rather than the on-heap trampoline located
  // at instruction_start.
  inline int InstructionSize();

  // Return the source position table.
  inline ByteArray source_position_table();

  void DropStackFrameCache();

  // Returns the size of instructions and the metadata.
  inline int SizeIncludingMetadata();

  // Returns true if pc is inside this object's instructions.
  inline bool contains(Address pc);

  // Returns the kind of the code.
  inline CodeKind kind();

  DECL_CAST(AbstractCode)
  inline Code GetCode();
  inline BytecodeArray GetBytecodeArray();

  // Max loop nesting marker used to postpose OSR. We don't take loop
  // nesting that is deeper than 5 levels into account.
  static const int kMaxLoopNestingMarker = 6;

  OBJECT_CONSTRUCTORS(AbstractCode, HeapObject);
};

// Dependent code is a singly linked list of weak fixed arrays. Each array
// contains weak pointers to code objects for one dependent group. The suffix of
// the array can be filled with the undefined value if the number of codes is
// less than the length of the array.
//
// +------+-----------------+--------+--------+-----+--------+-----------+-----+
// | next | count & group 1 | code 1 | code 2 | ... | code n | undefined | ... |
// +------+-----------------+--------+--------+-----+--------+-----------+-----+
//    |
//    V
// +------+-----------------+--------+--------+-----+--------+-----------+-----+
// | next | count & group 2 | code 1 | code 2 | ... | code m | undefined | ... |
// +------+-----------------+--------+--------+-----+--------+-----------+-----+
//    |
//    V
// empty_weak_fixed_array()
//
// The list of weak fixed arrays is ordered by dependency groups.

class DependentCode : public WeakFixedArray {
 public:
  DECL_CAST(DependentCode)

  enum DependencyGroup {
    // Group of code that embed a transition to this map, and depend on being
    // deoptimized when the transition is replaced by a new version.
    kTransitionGroup,
    // Group of code that omit run-time prototype checks for prototypes
    // described by this map. The group is deoptimized whenever an object
    // described by this map changes shape (and transitions to a new map),
    // possibly invalidating the assumptions embedded in the code.
    kPrototypeCheckGroup,
    // Group of code that depends on global property values in property cells
    // not being changed.
    kPropertyCellChangedGroup,
    // Group of code that omit run-time checks for field(s) introduced by
    // this map, i.e. for the field type.
    kFieldTypeGroup,
    kFieldConstGroup,
    kFieldRepresentationGroup,
    // Group of code that omit run-time type checks for initial maps of
    // constructors.
    kInitialMapChangedGroup,
    // Group of code that depends on tenuring information in AllocationSites
    // not being changed.
    kAllocationSiteTenuringChangedGroup,
    // Group of code that depends on element transition information in
    // AllocationSites not being changed.
    kAllocationSiteTransitionChangedGroup
  };

  // Register a dependency of {code} on {object}, of the kind given by {group}.
  V8_EXPORT_PRIVATE static void InstallDependency(Isolate* isolate,
                                                  const MaybeObjectHandle& code,
                                                  Handle<HeapObject> object,
                                                  DependencyGroup group);

  void DeoptimizeDependentCodeGroup(DependencyGroup group);

  bool MarkCodeForDeoptimization(DependencyGroup group);

  // The following low-level accessors are exposed only for tests.
  inline DependencyGroup group();
  inline MaybeObject object_at(int i);
  inline int count();
  inline DependentCode next_link();

 private:
  static const char* DependencyGroupName(DependencyGroup group);

  // Get/Set {object}'s {DependentCode}.
  static DependentCode GetDependentCode(Handle<HeapObject> object);
  static void SetDependentCode(Handle<HeapObject> object,
                               Handle<DependentCode> dep);

  static Handle<DependentCode> New(Isolate* isolate, DependencyGroup group,
                                   const MaybeObjectHandle& object,
                                   Handle<DependentCode> next);
  static Handle<DependentCode> EnsureSpace(Isolate* isolate,
                                           Handle<DependentCode> entries);
  static Handle<DependentCode> InsertWeakCode(Isolate* isolate,
                                              Handle<DependentCode> entries,
                                              DependencyGroup group,
                                              const MaybeObjectHandle& code);

  // Compact by removing cleared weak cells and return true if there was
  // any cleared weak cell.
  bool Compact();

  static int Grow(int number_of_entries) {
    if (number_of_entries < 5) return number_of_entries + 1;
    return number_of_entries * 5 / 4;
  }

  static const int kGroupCount = kAllocationSiteTransitionChangedGroup + 1;
  static const int kNextLinkIndex = 0;
  static const int kFlagsIndex = 1;
  static const int kCodesStartIndex = 2;

  inline void set_next_link(DependentCode next);
  inline void set_count(int value);
  inline void set_object_at(int i, MaybeObject object);
  inline void clear_at(int i);
  inline void copy(int from, int to);

  inline int flags();
  inline void set_flags(int flags);
  using GroupField = base::BitField<int, 0, 5>;
  using CountField = base::BitField<int, 5, 27>;
  STATIC_ASSERT(kGroupCount <= GroupField::kMax + 1);

  OBJECT_CONSTRUCTORS(DependentCode, WeakFixedArray);
};

// BytecodeArray represents a sequence of interpreter bytecodes.
class BytecodeArray : public FixedArrayBase {
 public:
  enum Age {
    kNoAgeBytecodeAge = 0,
    kQuadragenarianBytecodeAge,
    kQuinquagenarianBytecodeAge,
    kSexagenarianBytecodeAge,
    kSeptuagenarianBytecodeAge,
    kOctogenarianBytecodeAge,
    kAfterLastBytecodeAge,
    kFirstBytecodeAge = kNoAgeBytecodeAge,
    kLastBytecodeAge = kAfterLastBytecodeAge - 1,
    kBytecodeAgeCount = kAfterLastBytecodeAge - kFirstBytecodeAge - 1,
    kIsOldBytecodeAge = kSexagenarianBytecodeAge
  };

  static constexpr int SizeFor(int length) {
    return OBJECT_POINTER_ALIGN(kHeaderSize + length);
  }

  // Setter and getter
  inline byte get(int index) const;
  inline void set(int index, byte value);

  // Returns data start address.
  inline Address GetFirstBytecodeAddress();

  // Accessors for frame size.
  inline int32_t frame_size() const;
  inline void set_frame_size(int32_t frame_size);

  // Accessor for register count (derived from frame_size).
  inline int register_count() const;

  // Accessors for parameter count (including implicit 'this' receiver).
  inline int32_t parameter_count() const;
  inline void set_parameter_count(int32_t number_of_parameters);

  // Register used to pass the incoming new.target or generator object from the
  // fucntion call.
  inline interpreter::Register incoming_new_target_or_generator_register()
      const;
  inline void set_incoming_new_target_or_generator_register(
      interpreter::Register incoming_new_target_or_generator_register);

  // Accessors for OSR loop nesting level.
  inline int osr_loop_nesting_level() const;
  inline void set_osr_loop_nesting_level(int depth);

  // Accessors for bytecode's code age.
  inline Age bytecode_age() const;
  inline void set_bytecode_age(Age age);

  // Accessors for the constant pool.
  DECL_ACCESSORS(constant_pool, FixedArray)

  // Accessors for handler table containing offsets of exception handlers.
  DECL_ACCESSORS(handler_table, ByteArray)

  // Accessors for source position table. Can contain:
  // * undefined (initial value)
  // * empty_byte_array (for bytecode generated for functions that will never
  // have source positions, e.g. native functions).
  // * ByteArray (when source positions have been collected for the bytecode)
  // * exception (when an error occurred while explicitly collecting source
  // positions for pre-existing bytecode).
  DECL_RELEASE_ACQUIRE_ACCESSORS(source_position_table, Object)

  inline bool HasSourcePositionTable() const;
  inline bool DidSourcePositionGenerationFail() const;

  // If source positions have not been collected or an exception has been thrown
  // this will return empty_byte_array.
  inline ByteArray SourcePositionTable() const;

  // Indicates that an attempt was made to collect source positions, but that it
  // failed most likely due to stack exhaustion. When in this state
  // |SourcePositionTable| will return an empty byte array rather than crashing
  // as it would if no attempt was ever made to collect source positions.
  inline void SetSourcePositionsFailedToCollect();

  DECL_CAST(BytecodeArray)

  // Dispatched behavior.
  inline int BytecodeArraySize();

  inline int raw_instruction_size();

  // Returns the size of bytecode and its metadata. This includes the size of
  // bytecode, constant pool, source position table, and handler table.
  inline int SizeIncludingMetadata();

  DECL_PRINTER(BytecodeArray)
  DECL_VERIFIER(BytecodeArray)

  V8_EXPORT_PRIVATE void Disassemble(std::ostream& os);

  void CopyBytecodesTo(BytecodeArray to);

  // Bytecode aging
  V8_EXPORT_PRIVATE bool IsOld() const;
  V8_EXPORT_PRIVATE void MakeOlder();

  // Clear uninitialized padding space. This ensures that the snapshot content
  // is deterministic.
  inline void clear_padding();

  // Layout description.
  DEFINE_FIELD_OFFSET_CONSTANTS(FixedArrayBase::kHeaderSize,
                                TORQUE_GENERATED_BYTECODE_ARRAY_FIELDS)

  // InterpreterEntryTrampoline expects these fields to be next to each other
  // and writes a 16-bit value to reset them.
  STATIC_ASSERT(BytecodeArray::kBytecodeAgeOffset ==
                kOsrNestingLevelOffset + kCharSize);

  // Maximal memory consumption for a single BytecodeArray.
  static const int kMaxSize = 512 * MB;
  // Maximal length of a single BytecodeArray.
  static const int kMaxLength = kMaxSize - kHeaderSize;

  class BodyDescriptor;

  OBJECT_CONSTRUCTORS(BytecodeArray, FixedArrayBase);
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
  static const int kEagerSoftAndBailoutDeoptCountIndex = 9;
  static const int kLazyDeoptCountIndex = 10;
  static const int kFirstDeoptEntryIndex = 11;

  // Offsets of deopt entry elements relative to the start of the entry.
  static const int kBytecodeOffsetRawOffset = 0;
  static const int kTranslationIndexOffset = 1;
  static const int kPcOffset = 2;
  static const int kDeoptEntrySize = 3;

// Simple element accessors.
#define DECL_ELEMENT_ACCESSORS(name, type) \
  inline type name() const;                \
  inline void Set##name(type value);

  DECL_ELEMENT_ACCESSORS(TranslationByteArray, TranslationArray)
  DECL_ELEMENT_ACCESSORS(InlinedFunctionCount, Smi)
  DECL_ELEMENT_ACCESSORS(LiteralArray, FixedArray)
  DECL_ELEMENT_ACCESSORS(OsrBytecodeOffset, Smi)
  DECL_ELEMENT_ACCESSORS(OsrPcOffset, Smi)
  DECL_ELEMENT_ACCESSORS(OptimizationId, Smi)
  DECL_ELEMENT_ACCESSORS(SharedFunctionInfo, Object)
  DECL_ELEMENT_ACCESSORS(InliningPositions, PodArray<InliningPosition>)
  DECL_ELEMENT_ACCESSORS(DeoptExitStart, Smi)
  DECL_ELEMENT_ACCESSORS(EagerSoftAndBailoutDeoptCount, Smi)
  DECL_ELEMENT_ACCESSORS(LazyDeoptCount, Smi)

#undef DECL_ELEMENT_ACCESSORS

// Accessors for elements of the ith deoptimization entry.
#define DECL_ENTRY_ACCESSORS(name, type) \
  inline type name(int i) const;         \
  inline void Set##name(int i, type value);

  DECL_ENTRY_ACCESSORS(BytecodeOffsetRaw, Smi)
  DECL_ENTRY_ACCESSORS(TranslationIndex, Smi)
  DECL_ENTRY_ACCESSORS(Pc, Smi)

#undef DECL_ENTRY_ACCESSORS

  inline BytecodeOffset GetBytecodeOffset(int i);

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
  void DeoptimizationDataPrint(std::ostream& os);  // NOLINT
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
