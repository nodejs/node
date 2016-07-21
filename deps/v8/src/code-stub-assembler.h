// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODE_STUB_ASSEMBLER_H_
#define V8_CODE_STUB_ASSEMBLER_H_

#include "src/compiler/code-assembler.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

class CallInterfaceDescriptor;

// Provides JavaScript-specific "macro-assembler" functionality on top of the
// CodeAssembler. By factoring the JavaScript-isms out of the CodeAssembler,
// it's possible to add JavaScript-specific useful CodeAssembler "macros"
// without modifying files in the compiler directory (and requiring a review
// from a compiler directory OWNER).
class CodeStubAssembler : public compiler::CodeAssembler {
 public:
  // Create with CallStub linkage.
  // |result_size| specifies the number of results returned by the stub.
  // TODO(rmcilroy): move result_size to the CallInterfaceDescriptor.
  CodeStubAssembler(Isolate* isolate, Zone* zone,
                    const CallInterfaceDescriptor& descriptor,
                    Code::Flags flags, const char* name,
                    size_t result_size = 1);

  // Create with JSCall linkage.
  CodeStubAssembler(Isolate* isolate, Zone* zone, int parameter_count,
                    Code::Flags flags, const char* name);

  enum ParameterMode { INTEGER_PARAMETERS, SMI_PARAMETERS };

  compiler::Node* BooleanMapConstant();
  compiler::Node* EmptyStringConstant();
  compiler::Node* HeapNumberMapConstant();
  compiler::Node* NoContextConstant();
  compiler::Node* NullConstant();
  compiler::Node* UndefinedConstant();
  compiler::Node* StaleRegisterConstant();

  // Float64 operations.
  compiler::Node* Float64Ceil(compiler::Node* x);
  compiler::Node* Float64Floor(compiler::Node* x);
  compiler::Node* Float64Round(compiler::Node* x);
  compiler::Node* Float64Trunc(compiler::Node* x);

  // Tag a Word as a Smi value.
  compiler::Node* SmiTag(compiler::Node* value);
  // Untag a Smi value as a Word.
  compiler::Node* SmiUntag(compiler::Node* value);

  // Smi conversions.
  compiler::Node* SmiToFloat64(compiler::Node* value);
  compiler::Node* SmiFromWord32(compiler::Node* value);
  compiler::Node* SmiToWord(compiler::Node* value) { return SmiUntag(value); }
  compiler::Node* SmiToWord32(compiler::Node* value);

  // Smi operations.
  compiler::Node* SmiAdd(compiler::Node* a, compiler::Node* b);
  compiler::Node* SmiAddWithOverflow(compiler::Node* a, compiler::Node* b);
  compiler::Node* SmiSub(compiler::Node* a, compiler::Node* b);
  compiler::Node* SmiSubWithOverflow(compiler::Node* a, compiler::Node* b);
  compiler::Node* SmiEqual(compiler::Node* a, compiler::Node* b);
  compiler::Node* SmiAboveOrEqual(compiler::Node* a, compiler::Node* b);
  compiler::Node* SmiLessThan(compiler::Node* a, compiler::Node* b);
  compiler::Node* SmiLessThanOrEqual(compiler::Node* a, compiler::Node* b);
  compiler::Node* SmiMin(compiler::Node* a, compiler::Node* b);

  // Allocate an object of the given size.
  compiler::Node* Allocate(compiler::Node* size, AllocationFlags flags = kNone);
  compiler::Node* Allocate(int size, AllocationFlags flags = kNone);
  compiler::Node* InnerAllocate(compiler::Node* previous, int offset);
  compiler::Node* InnerAllocate(compiler::Node* previous,
                                compiler::Node* offset);

  // Check a value for smi-ness
  compiler::Node* WordIsSmi(compiler::Node* a);
  // Check that the value is a positive smi.
  compiler::Node* WordIsPositiveSmi(compiler::Node* a);

  void BranchIfSmiLessThan(compiler::Node* a, compiler::Node* b, Label* if_true,
                           Label* if_false) {
    BranchIf(SmiLessThan(a, b), if_true, if_false);
  }

  void BranchIfSmiLessThanOrEqual(compiler::Node* a, compiler::Node* b,
                                  Label* if_true, Label* if_false) {
    BranchIf(SmiLessThanOrEqual(a, b), if_true, if_false);
  }

  void BranchIfFloat64IsNaN(compiler::Node* value, Label* if_true,
                            Label* if_false) {
    BranchIfFloat64Equal(value, value, if_false, if_true);
  }

  // Load an object pointer from a buffer that isn't in the heap.
  compiler::Node* LoadBufferObject(compiler::Node* buffer, int offset,
                                   MachineType rep = MachineType::AnyTagged());
  // Load a field from an object on the heap.
  compiler::Node* LoadObjectField(compiler::Node* object, int offset,
                                  MachineType rep = MachineType::AnyTagged());
  // Load the floating point value of a HeapNumber.
  compiler::Node* LoadHeapNumberValue(compiler::Node* object);
  // Load the Map of an HeapObject.
  compiler::Node* LoadMap(compiler::Node* object);
  // Load the instance type of an HeapObject.
  compiler::Node* LoadInstanceType(compiler::Node* object);
  // Load the elements backing store of a JSObject.
  compiler::Node* LoadElements(compiler::Node* object);
  // Load the length of a fixed array base instance.
  compiler::Node* LoadFixedArrayBaseLength(compiler::Node* array);
  // Load the bit field of a Map.
  compiler::Node* LoadMapBitField(compiler::Node* map);
  // Load bit field 2 of a map.
  compiler::Node* LoadMapBitField2(compiler::Node* map);
  // Load bit field 3 of a map.
  compiler::Node* LoadMapBitField3(compiler::Node* map);
  // Load the instance type of a map.
  compiler::Node* LoadMapInstanceType(compiler::Node* map);
  // Load the instance descriptors of a map.
  compiler::Node* LoadMapDescriptors(compiler::Node* map);
  // Load the prototype of a map.
  compiler::Node* LoadMapPrototype(compiler::Node* map);

  // Load the hash field of a name.
  compiler::Node* LoadNameHash(compiler::Node* name);
  // Load the instance size of a Map.
  compiler::Node* LoadMapInstanceSize(compiler::Node* map);

  compiler::Node* AllocateUninitializedFixedArray(compiler::Node* length);

  // Load an array element from a FixedArray.
  compiler::Node* LoadFixedArrayElement(
      compiler::Node* object, compiler::Node* int32_index,
      int additional_offset = 0,
      ParameterMode parameter_mode = INTEGER_PARAMETERS);

  // Context manipulation
  compiler::Node* LoadNativeContext(compiler::Node* context);

  compiler::Node* LoadJSArrayElementsMap(ElementsKind kind,
                                         compiler::Node* native_context);

  // Store the floating point value of a HeapNumber.
  compiler::Node* StoreHeapNumberValue(compiler::Node* object,
                                       compiler::Node* value);
  // Store a field to an object on the heap.
  compiler::Node* StoreObjectField(
      compiler::Node* object, int offset, compiler::Node* value);
  compiler::Node* StoreObjectFieldNoWriteBarrier(
      compiler::Node* object, int offset, compiler::Node* value,
      MachineRepresentation rep = MachineRepresentation::kTagged);
  // Store the Map of an HeapObject.
  compiler::Node* StoreMapNoWriteBarrier(compiler::Node* object,
                                         compiler::Node* map);
  // Store an array element to a FixedArray.
  compiler::Node* StoreFixedArrayElement(
      compiler::Node* object, compiler::Node* index, compiler::Node* value,
      WriteBarrierMode barrier_mode = UPDATE_WRITE_BARRIER,
      ParameterMode parameter_mode = INTEGER_PARAMETERS);

  compiler::Node* StoreFixedDoubleArrayElement(
      compiler::Node* object, compiler::Node* index, compiler::Node* value,
      ParameterMode parameter_mode = INTEGER_PARAMETERS);

  // Allocate a HeapNumber without initializing its value.
  compiler::Node* AllocateHeapNumber();
  // Allocate a HeapNumber with a specific value.
  compiler::Node* AllocateHeapNumberWithValue(compiler::Node* value);
  // Allocate a SeqOneByteString with the given length.
  compiler::Node* AllocateSeqOneByteString(int length);
  // Allocate a SeqTwoByteString with the given length.
  compiler::Node* AllocateSeqTwoByteString(int length);
  // Allocated an JSArray
  compiler::Node* AllocateJSArray(ElementsKind kind, compiler::Node* array_map,
                                  compiler::Node* capacity,
                                  compiler::Node* length,
                                  compiler::Node* allocation_site = nullptr,
                                  ParameterMode mode = INTEGER_PARAMETERS);

  // Allocation site manipulation
  void InitializeAllocationMemento(compiler::Node* base_allocation,
                                   int base_allocation_size,
                                   compiler::Node* allocation_site);

  compiler::Node* TruncateTaggedToFloat64(compiler::Node* context,
                                          compiler::Node* value);
  compiler::Node* TruncateTaggedToWord32(compiler::Node* context,
                                         compiler::Node* value);
  // Truncate the floating point value of a HeapNumber to an Int32.
  compiler::Node* TruncateHeapNumberValueToWord32(compiler::Node* object);

  // Conversions.
  compiler::Node* ChangeFloat64ToTagged(compiler::Node* value);
  compiler::Node* ChangeInt32ToTagged(compiler::Node* value);
  compiler::Node* ChangeUint32ToTagged(compiler::Node* value);

  // Type conversions.
  // Throws a TypeError for {method_name} if {value} is not coercible to Object,
  // or returns the {value} converted to a String otherwise.
  compiler::Node* ToThisString(compiler::Node* context, compiler::Node* value,
                               char const* method_name);

  // String helpers.
  // Load a character from a String (might flatten a ConsString).
  compiler::Node* StringCharCodeAt(compiler::Node* string,
                                   compiler::Node* smi_index);
  // Return the single character string with only {code}.
  compiler::Node* StringFromCharCode(compiler::Node* code);

  // Returns a node that is true if the given bit is set in |word32|.
  template <typename T>
  compiler::Node* BitFieldDecode(compiler::Node* word32) {
    return BitFieldDecode(word32, T::kShift, T::kMask);
  }

  compiler::Node* BitFieldDecode(compiler::Node* word32, uint32_t shift,
                                 uint32_t mask);

  // Various building blocks for stubs doing property lookups.
  void TryToName(compiler::Node* key, Label* if_keyisindex, Variable* var_index,
                 Label* if_keyisunique, Label* call_runtime);

  void TryLookupProperty(compiler::Node* object, compiler::Node* map,
                         compiler::Node* instance_type, compiler::Node* name,
                         Label* if_found, Label* if_not_found,
                         Label* call_runtime);

  void TryLookupElement(compiler::Node* object, compiler::Node* map,
                        compiler::Node* instance_type, compiler::Node* index,
                        Label* if_found, Label* if_not_found,
                        Label* call_runtime);

  // Instanceof helpers.
  // ES6 section 7.3.19 OrdinaryHasInstance (C, O)
  compiler::Node* OrdinaryHasInstance(compiler::Node* context,
                                      compiler::Node* callable,
                                      compiler::Node* object);

 private:
  compiler::Node* ElementOffsetFromIndex(compiler::Node* index,
                                         ElementsKind kind, ParameterMode mode,
                                         int base_size = 0);

  compiler::Node* AllocateRawAligned(compiler::Node* size_in_bytes,
                                     AllocationFlags flags,
                                     compiler::Node* top_address,
                                     compiler::Node* limit_address);
  compiler::Node* AllocateRawUnaligned(compiler::Node* size_in_bytes,
                                       AllocationFlags flags,
                                       compiler::Node* top_adddress,
                                       compiler::Node* limit_address);

  static const int kElementLoopUnrollThreshold = 8;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CODE_STUB_ASSEMBLER_H_
