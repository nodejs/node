// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_ARM

#include "src/arm/assembler-arm.h"
#include "src/code-stubs.h"
#include "src/codegen.h"
#include "src/disasm.h"
#include "src/ic-inl.h"
#include "src/runtime.h"
#include "src/stub-cache.h"

namespace v8 {
namespace internal {


// ----------------------------------------------------------------------------
// Static IC stub generators.
//

#define __ ACCESS_MASM(masm)


static void GenerateGlobalInstanceTypeCheck(MacroAssembler* masm,
                                            Register type,
                                            Label* global_object) {
  // Register usage:
  //   type: holds the receiver instance type on entry.
  __ cmp(type, Operand(JS_GLOBAL_OBJECT_TYPE));
  __ b(eq, global_object);
  __ cmp(type, Operand(JS_BUILTINS_OBJECT_TYPE));
  __ b(eq, global_object);
  __ cmp(type, Operand(JS_GLOBAL_PROXY_TYPE));
  __ b(eq, global_object);
}


// Helper function used from LoadIC GenerateNormal.
//
// elements: Property dictionary. It is not clobbered if a jump to the miss
//           label is done.
// name:     Property name. It is not clobbered if a jump to the miss label is
//           done
// result:   Register for the result. It is only updated if a jump to the miss
//           label is not done. Can be the same as elements or name clobbering
//           one of these in the case of not jumping to the miss label.
// The two scratch registers need to be different from elements, name and
// result.
// The generated code assumes that the receiver has slow properties,
// is not a global object and does not have interceptors.
static void GenerateDictionaryLoad(MacroAssembler* masm,
                                   Label* miss,
                                   Register elements,
                                   Register name,
                                   Register result,
                                   Register scratch1,
                                   Register scratch2) {
  // Main use of the scratch registers.
  // scratch1: Used as temporary and to hold the capacity of the property
  //           dictionary.
  // scratch2: Used as temporary.
  Label done;

  // Probe the dictionary.
  NameDictionaryLookupStub::GeneratePositiveLookup(masm,
                                                   miss,
                                                   &done,
                                                   elements,
                                                   name,
                                                   scratch1,
                                                   scratch2);

  // If probing finds an entry check that the value is a normal
  // property.
  __ bind(&done);  // scratch2 == elements + 4 * index
  const int kElementsStartOffset = NameDictionary::kHeaderSize +
      NameDictionary::kElementsStartIndex * kPointerSize;
  const int kDetailsOffset = kElementsStartOffset + 2 * kPointerSize;
  __ ldr(scratch1, FieldMemOperand(scratch2, kDetailsOffset));
  __ tst(scratch1, Operand(PropertyDetails::TypeField::kMask << kSmiTagSize));
  __ b(ne, miss);

  // Get the value at the masked, scaled index and return.
  __ ldr(result,
         FieldMemOperand(scratch2, kElementsStartOffset + 1 * kPointerSize));
}


// Helper function used from StoreIC::GenerateNormal.
//
// elements: Property dictionary. It is not clobbered if a jump to the miss
//           label is done.
// name:     Property name. It is not clobbered if a jump to the miss label is
//           done
// value:    The value to store.
// The two scratch registers need to be different from elements, name and
// result.
// The generated code assumes that the receiver has slow properties,
// is not a global object and does not have interceptors.
static void GenerateDictionaryStore(MacroAssembler* masm,
                                    Label* miss,
                                    Register elements,
                                    Register name,
                                    Register value,
                                    Register scratch1,
                                    Register scratch2) {
  // Main use of the scratch registers.
  // scratch1: Used as temporary and to hold the capacity of the property
  //           dictionary.
  // scratch2: Used as temporary.
  Label done;

  // Probe the dictionary.
  NameDictionaryLookupStub::GeneratePositiveLookup(masm,
                                                   miss,
                                                   &done,
                                                   elements,
                                                   name,
                                                   scratch1,
                                                   scratch2);

  // If probing finds an entry in the dictionary check that the value
  // is a normal property that is not read only.
  __ bind(&done);  // scratch2 == elements + 4 * index
  const int kElementsStartOffset = NameDictionary::kHeaderSize +
      NameDictionary::kElementsStartIndex * kPointerSize;
  const int kDetailsOffset = kElementsStartOffset + 2 * kPointerSize;
  const int kTypeAndReadOnlyMask =
      (PropertyDetails::TypeField::kMask |
       PropertyDetails::AttributesField::encode(READ_ONLY)) << kSmiTagSize;
  __ ldr(scratch1, FieldMemOperand(scratch2, kDetailsOffset));
  __ tst(scratch1, Operand(kTypeAndReadOnlyMask));
  __ b(ne, miss);

  // Store the value at the masked, scaled index and return.
  const int kValueOffset = kElementsStartOffset + kPointerSize;
  __ add(scratch2, scratch2, Operand(kValueOffset - kHeapObjectTag));
  __ str(value, MemOperand(scratch2));

  // Update the write barrier. Make sure not to clobber the value.
  __ mov(scratch1, value);
  __ RecordWrite(
      elements, scratch2, scratch1, kLRHasNotBeenSaved, kDontSaveFPRegs);
}


// Checks the receiver for special cases (value type, slow case bits).
// Falls through for regular JS object.
static void GenerateKeyedLoadReceiverCheck(MacroAssembler* masm,
                                           Register receiver,
                                           Register map,
                                           Register scratch,
                                           int interceptor_bit,
                                           Label* slow) {
  // Check that the object isn't a smi.
  __ JumpIfSmi(receiver, slow);
  // Get the map of the receiver.
  __ ldr(map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  // Check bit field.
  __ ldrb(scratch, FieldMemOperand(map, Map::kBitFieldOffset));
  __ tst(scratch,
         Operand((1 << Map::kIsAccessCheckNeeded) | (1 << interceptor_bit)));
  __ b(ne, slow);
  // Check that the object is some kind of JS object EXCEPT JS Value type.
  // In the case that the object is a value-wrapper object,
  // we enter the runtime system to make sure that indexing into string
  // objects work as intended.
  DCHECK(JS_OBJECT_TYPE > JS_VALUE_TYPE);
  __ ldrb(scratch, FieldMemOperand(map, Map::kInstanceTypeOffset));
  __ cmp(scratch, Operand(JS_OBJECT_TYPE));
  __ b(lt, slow);
}


// Loads an indexed element from a fast case array.
// If not_fast_array is NULL, doesn't perform the elements map check.
static void GenerateFastArrayLoad(MacroAssembler* masm,
                                  Register receiver,
                                  Register key,
                                  Register elements,
                                  Register scratch1,
                                  Register scratch2,
                                  Register result,
                                  Label* not_fast_array,
                                  Label* out_of_range) {
  // Register use:
  //
  // receiver - holds the receiver on entry.
  //            Unchanged unless 'result' is the same register.
  //
  // key      - holds the smi key on entry.
  //            Unchanged unless 'result' is the same register.
  //
  // elements - holds the elements of the receiver on exit.
  //
  // result   - holds the result on exit if the load succeeded.
  //            Allowed to be the the same as 'receiver' or 'key'.
  //            Unchanged on bailout so 'receiver' and 'key' can be safely
  //            used by further computation.
  //
  // Scratch registers:
  //
  // scratch1 - used to hold elements map and elements length.
  //            Holds the elements map if not_fast_array branch is taken.
  //
  // scratch2 - used to hold the loaded value.

  __ ldr(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  if (not_fast_array != NULL) {
    // Check that the object is in fast mode and writable.
    __ ldr(scratch1, FieldMemOperand(elements, HeapObject::kMapOffset));
    __ LoadRoot(ip, Heap::kFixedArrayMapRootIndex);
    __ cmp(scratch1, ip);
    __ b(ne, not_fast_array);
  } else {
    __ AssertFastElements(elements);
  }
  // Check that the key (index) is within bounds.
  __ ldr(scratch1, FieldMemOperand(elements, FixedArray::kLengthOffset));
  __ cmp(key, Operand(scratch1));
  __ b(hs, out_of_range);
  // Fast case: Do the load.
  __ add(scratch1, elements, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ ldr(scratch2, MemOperand::PointerAddressFromSmiKey(scratch1, key));
  __ LoadRoot(ip, Heap::kTheHoleValueRootIndex);
  __ cmp(scratch2, ip);
  // In case the loaded value is the_hole we have to consult GetProperty
  // to ensure the prototype chain is searched.
  __ b(eq, out_of_range);
  __ mov(result, scratch2);
}


// Checks whether a key is an array index string or a unique name.
// Falls through if a key is a unique name.
static void GenerateKeyNameCheck(MacroAssembler* masm,
                                 Register key,
                                 Register map,
                                 Register hash,
                                 Label* index_string,
                                 Label* not_unique) {
  // The key is not a smi.
  Label unique;
  // Is it a name?
  __ CompareObjectType(key, map, hash, LAST_UNIQUE_NAME_TYPE);
  __ b(hi, not_unique);
  STATIC_ASSERT(LAST_UNIQUE_NAME_TYPE == FIRST_NONSTRING_TYPE);
  __ b(eq, &unique);

  // Is the string an array index, with cached numeric value?
  __ ldr(hash, FieldMemOperand(key, Name::kHashFieldOffset));
  __ tst(hash, Operand(Name::kContainsCachedArrayIndexMask));
  __ b(eq, index_string);

  // Is the string internalized? We know it's a string, so a single
  // bit test is enough.
  // map: key map
  __ ldrb(hash, FieldMemOperand(map, Map::kInstanceTypeOffset));
  STATIC_ASSERT(kInternalizedTag == 0);
  __ tst(hash, Operand(kIsNotInternalizedMask));
  __ b(ne, not_unique);

  __ bind(&unique);
}


void LoadIC::GenerateMegamorphic(MacroAssembler* masm) {
  // The return address is in lr.
  Register receiver = ReceiverRegister();
  Register name = NameRegister();
  DCHECK(receiver.is(r1));
  DCHECK(name.is(r2));

  // Probe the stub cache.
  Code::Flags flags = Code::RemoveTypeAndHolderFromFlags(
      Code::ComputeHandlerFlags(Code::LOAD_IC));
  masm->isolate()->stub_cache()->GenerateProbe(
      masm, flags, receiver, name, r3, r4, r5, r6);

  // Cache miss: Jump to runtime.
  GenerateMiss(masm);
}


void LoadIC::GenerateNormal(MacroAssembler* masm) {
  Register dictionary = r0;
  DCHECK(!dictionary.is(ReceiverRegister()));
  DCHECK(!dictionary.is(NameRegister()));

  Label slow;

  __ ldr(dictionary,
         FieldMemOperand(ReceiverRegister(), JSObject::kPropertiesOffset));
  GenerateDictionaryLoad(masm, &slow, dictionary, NameRegister(), r0, r3, r4);
  __ Ret();

  // Dictionary load failed, go slow (but don't miss).
  __ bind(&slow);
  GenerateRuntimeGetProperty(masm);
}


// A register that isn't one of the parameters to the load ic.
static const Register LoadIC_TempRegister() { return r3; }


void LoadIC::GenerateMiss(MacroAssembler* masm) {
  // The return address is in lr.
  Isolate* isolate = masm->isolate();

  __ IncrementCounter(isolate->counters()->load_miss(), 1, r3, r4);

  __ mov(LoadIC_TempRegister(), ReceiverRegister());
  __ Push(LoadIC_TempRegister(), NameRegister());

  // Perform tail call to the entry.
  ExternalReference ref =
      ExternalReference(IC_Utility(kLoadIC_Miss), isolate);
  __ TailCallExternalReference(ref, 2, 1);
}


void LoadIC::GenerateRuntimeGetProperty(MacroAssembler* masm) {
  // The return address is in lr.

  __ mov(LoadIC_TempRegister(), ReceiverRegister());
  __ Push(LoadIC_TempRegister(), NameRegister());

  __ TailCallRuntime(Runtime::kGetProperty, 2, 1);
}


static MemOperand GenerateMappedArgumentsLookup(MacroAssembler* masm,
                                                Register object,
                                                Register key,
                                                Register scratch1,
                                                Register scratch2,
                                                Register scratch3,
                                                Label* unmapped_case,
                                                Label* slow_case) {
  Heap* heap = masm->isolate()->heap();

  // Check that the receiver is a JSObject. Because of the map check
  // later, we do not need to check for interceptors or whether it
  // requires access checks.
  __ JumpIfSmi(object, slow_case);
  // Check that the object is some kind of JSObject.
  __ CompareObjectType(object, scratch1, scratch2, FIRST_JS_RECEIVER_TYPE);
  __ b(lt, slow_case);

  // Check that the key is a positive smi.
  __ tst(key, Operand(0x80000001));
  __ b(ne, slow_case);

  // Load the elements into scratch1 and check its map.
  Handle<Map> arguments_map(heap->sloppy_arguments_elements_map());
  __ ldr(scratch1, FieldMemOperand(object, JSObject::kElementsOffset));
  __ CheckMap(scratch1, scratch2, arguments_map, slow_case, DONT_DO_SMI_CHECK);

  // Check if element is in the range of mapped arguments. If not, jump
  // to the unmapped lookup with the parameter map in scratch1.
  __ ldr(scratch2, FieldMemOperand(scratch1, FixedArray::kLengthOffset));
  __ sub(scratch2, scratch2, Operand(Smi::FromInt(2)));
  __ cmp(key, Operand(scratch2));
  __ b(cs, unmapped_case);

  // Load element index and check whether it is the hole.
  const int kOffset =
      FixedArray::kHeaderSize + 2 * kPointerSize - kHeapObjectTag;

  __ mov(scratch3, Operand(kPointerSize >> 1));
  __ mul(scratch3, key, scratch3);
  __ add(scratch3, scratch3, Operand(kOffset));

  __ ldr(scratch2, MemOperand(scratch1, scratch3));
  __ LoadRoot(scratch3, Heap::kTheHoleValueRootIndex);
  __ cmp(scratch2, scratch3);
  __ b(eq, unmapped_case);

  // Load value from context and return it. We can reuse scratch1 because
  // we do not jump to the unmapped lookup (which requires the parameter
  // map in scratch1).
  __ ldr(scratch1, FieldMemOperand(scratch1, FixedArray::kHeaderSize));
  __ mov(scratch3, Operand(kPointerSize >> 1));
  __ mul(scratch3, scratch2, scratch3);
  __ add(scratch3, scratch3, Operand(Context::kHeaderSize - kHeapObjectTag));
  return MemOperand(scratch1, scratch3);
}


static MemOperand GenerateUnmappedArgumentsLookup(MacroAssembler* masm,
                                                  Register key,
                                                  Register parameter_map,
                                                  Register scratch,
                                                  Label* slow_case) {
  // Element is in arguments backing store, which is referenced by the
  // second element of the parameter_map. The parameter_map register
  // must be loaded with the parameter map of the arguments object and is
  // overwritten.
  const int kBackingStoreOffset = FixedArray::kHeaderSize + kPointerSize;
  Register backing_store = parameter_map;
  __ ldr(backing_store, FieldMemOperand(parameter_map, kBackingStoreOffset));
  Handle<Map> fixed_array_map(masm->isolate()->heap()->fixed_array_map());
  __ CheckMap(backing_store, scratch, fixed_array_map, slow_case,
              DONT_DO_SMI_CHECK);
  __ ldr(scratch, FieldMemOperand(backing_store, FixedArray::kLengthOffset));
  __ cmp(key, Operand(scratch));
  __ b(cs, slow_case);
  __ mov(scratch, Operand(kPointerSize >> 1));
  __ mul(scratch, key, scratch);
  __ add(scratch,
         scratch,
         Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  return MemOperand(backing_store, scratch);
}


void KeyedLoadIC::GenerateSloppyArguments(MacroAssembler* masm) {
  // The return address is in lr.
  Register receiver = ReceiverRegister();
  Register key = NameRegister();
  DCHECK(receiver.is(r1));
  DCHECK(key.is(r2));

  Label slow, notin;
  MemOperand mapped_location =
      GenerateMappedArgumentsLookup(
          masm, receiver, key, r0, r3, r4, &notin, &slow);
  __ ldr(r0, mapped_location);
  __ Ret();
  __ bind(&notin);
  // The unmapped lookup expects that the parameter map is in r0.
  MemOperand unmapped_location =
      GenerateUnmappedArgumentsLookup(masm, key, r0, r3, &slow);
  __ ldr(r0, unmapped_location);
  __ LoadRoot(r3, Heap::kTheHoleValueRootIndex);
  __ cmp(r0, r3);
  __ b(eq, &slow);
  __ Ret();
  __ bind(&slow);
  GenerateMiss(masm);
}


void KeyedStoreIC::GenerateSloppyArguments(MacroAssembler* masm) {
  Register receiver = ReceiverRegister();
  Register key = NameRegister();
  Register value = ValueRegister();
  DCHECK(receiver.is(r1));
  DCHECK(key.is(r2));
  DCHECK(value.is(r0));

  Label slow, notin;
  MemOperand mapped_location = GenerateMappedArgumentsLookup(
      masm, receiver, key, r3, r4, r5, &notin, &slow);
  __ str(value, mapped_location);
  __ add(r6, r3, r5);
  __ mov(r9, value);
  __ RecordWrite(r3, r6, r9, kLRHasNotBeenSaved, kDontSaveFPRegs);
  __ Ret();
  __ bind(&notin);
  // The unmapped lookup expects that the parameter map is in r3.
  MemOperand unmapped_location =
      GenerateUnmappedArgumentsLookup(masm, key, r3, r4, &slow);
  __ str(value, unmapped_location);
  __ add(r6, r3, r4);
  __ mov(r9, value);
  __ RecordWrite(r3, r6, r9, kLRHasNotBeenSaved, kDontSaveFPRegs);
  __ Ret();
  __ bind(&slow);
  GenerateMiss(masm);
}


void KeyedLoadIC::GenerateMiss(MacroAssembler* masm) {
  // The return address is in lr.
  Isolate* isolate = masm->isolate();

  __ IncrementCounter(isolate->counters()->keyed_load_miss(), 1, r3, r4);

  __ Push(ReceiverRegister(), NameRegister());

  // Perform tail call to the entry.
  ExternalReference ref =
      ExternalReference(IC_Utility(kKeyedLoadIC_Miss), isolate);

  __ TailCallExternalReference(ref, 2, 1);
}


// IC register specifications
const Register LoadIC::ReceiverRegister() { return r1; }
const Register LoadIC::NameRegister() { return r2; }


const Register LoadIC::SlotRegister() {
  DCHECK(FLAG_vector_ics);
  return r0;
}


const Register LoadIC::VectorRegister() {
  DCHECK(FLAG_vector_ics);
  return r3;
}


const Register StoreIC::ReceiverRegister() { return r1; }
const Register StoreIC::NameRegister() { return r2; }
const Register StoreIC::ValueRegister() { return r0; }


const Register KeyedStoreIC::MapRegister() {
  return r3;
}


void KeyedLoadIC::GenerateRuntimeGetProperty(MacroAssembler* masm) {
  // The return address is in lr.

  __ Push(ReceiverRegister(), NameRegister());

  __ TailCallRuntime(Runtime::kKeyedGetProperty, 2, 1);
}


void KeyedLoadIC::GenerateGeneric(MacroAssembler* masm) {
  // The return address is in lr.
  Label slow, check_name, index_smi, index_name, property_array_property;
  Label probe_dictionary, check_number_dictionary;

  Register key = NameRegister();
  Register receiver = ReceiverRegister();
  DCHECK(key.is(r2));
  DCHECK(receiver.is(r1));

  Isolate* isolate = masm->isolate();

  // Check that the key is a smi.
  __ JumpIfNotSmi(key, &check_name);
  __ bind(&index_smi);
  // Now the key is known to be a smi. This place is also jumped to from below
  // where a numeric string is converted to a smi.

  GenerateKeyedLoadReceiverCheck(
      masm, receiver, r0, r3, Map::kHasIndexedInterceptor, &slow);

  // Check the receiver's map to see if it has fast elements.
  __ CheckFastElements(r0, r3, &check_number_dictionary);

  GenerateFastArrayLoad(
      masm, receiver, key, r0, r3, r4, r0, NULL, &slow);
  __ IncrementCounter(isolate->counters()->keyed_load_generic_smi(), 1, r4, r3);
  __ Ret();

  __ bind(&check_number_dictionary);
  __ ldr(r4, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ ldr(r3, FieldMemOperand(r4, JSObject::kMapOffset));

  // Check whether the elements is a number dictionary.
  // r3: elements map
  // r4: elements
  __ LoadRoot(ip, Heap::kHashTableMapRootIndex);
  __ cmp(r3, ip);
  __ b(ne, &slow);
  __ SmiUntag(r0, key);
  __ LoadFromNumberDictionary(&slow, r4, key, r0, r0, r3, r5);
  __ Ret();

  // Slow case, key and receiver still in r2 and r1.
  __ bind(&slow);
  __ IncrementCounter(isolate->counters()->keyed_load_generic_slow(),
                      1, r4, r3);
  GenerateRuntimeGetProperty(masm);

  __ bind(&check_name);
  GenerateKeyNameCheck(masm, key, r0, r3, &index_name, &slow);

  GenerateKeyedLoadReceiverCheck(
      masm, receiver, r0, r3, Map::kHasNamedInterceptor, &slow);

  // If the receiver is a fast-case object, check the keyed lookup
  // cache. Otherwise probe the dictionary.
  __ ldr(r3, FieldMemOperand(receiver, JSObject::kPropertiesOffset));
  __ ldr(r4, FieldMemOperand(r3, HeapObject::kMapOffset));
  __ LoadRoot(ip, Heap::kHashTableMapRootIndex);
  __ cmp(r4, ip);
  __ b(eq, &probe_dictionary);

  // Load the map of the receiver, compute the keyed lookup cache hash
  // based on 32 bits of the map pointer and the name hash.
  __ ldr(r0, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ mov(r3, Operand(r0, ASR, KeyedLookupCache::kMapHashShift));
  __ ldr(r4, FieldMemOperand(key, Name::kHashFieldOffset));
  __ eor(r3, r3, Operand(r4, ASR, Name::kHashShift));
  int mask = KeyedLookupCache::kCapacityMask & KeyedLookupCache::kHashMask;
  __ And(r3, r3, Operand(mask));

  // Load the key (consisting of map and unique name) from the cache and
  // check for match.
  Label load_in_object_property;
  static const int kEntriesPerBucket = KeyedLookupCache::kEntriesPerBucket;
  Label hit_on_nth_entry[kEntriesPerBucket];
  ExternalReference cache_keys =
      ExternalReference::keyed_lookup_cache_keys(isolate);

  __ mov(r4, Operand(cache_keys));
  __ add(r4, r4, Operand(r3, LSL, kPointerSizeLog2 + 1));

  for (int i = 0; i < kEntriesPerBucket - 1; i++) {
    Label try_next_entry;
    // Load map and move r4 to next entry.
    __ ldr(r5, MemOperand(r4, kPointerSize * 2, PostIndex));
    __ cmp(r0, r5);
    __ b(ne, &try_next_entry);
    __ ldr(r5, MemOperand(r4, -kPointerSize));  // Load name
    __ cmp(key, r5);
    __ b(eq, &hit_on_nth_entry[i]);
    __ bind(&try_next_entry);
  }

  // Last entry: Load map and move r4 to name.
  __ ldr(r5, MemOperand(r4, kPointerSize, PostIndex));
  __ cmp(r0, r5);
  __ b(ne, &slow);
  __ ldr(r5, MemOperand(r4));
  __ cmp(key, r5);
  __ b(ne, &slow);

  // Get field offset.
  // r0     : receiver's map
  // r3     : lookup cache index
  ExternalReference cache_field_offsets =
      ExternalReference::keyed_lookup_cache_field_offsets(isolate);

  // Hit on nth entry.
  for (int i = kEntriesPerBucket - 1; i >= 0; i--) {
    __ bind(&hit_on_nth_entry[i]);
    __ mov(r4, Operand(cache_field_offsets));
    if (i != 0) {
      __ add(r3, r3, Operand(i));
    }
    __ ldr(r5, MemOperand(r4, r3, LSL, kPointerSizeLog2));
    __ ldrb(r6, FieldMemOperand(r0, Map::kInObjectPropertiesOffset));
    __ sub(r5, r5, r6, SetCC);
    __ b(ge, &property_array_property);
    if (i != 0) {
      __ jmp(&load_in_object_property);
    }
  }

  // Load in-object property.
  __ bind(&load_in_object_property);
  __ ldrb(r6, FieldMemOperand(r0, Map::kInstanceSizeOffset));
  __ add(r6, r6, r5);  // Index from start of object.
  __ sub(receiver, receiver, Operand(kHeapObjectTag));  // Remove the heap tag.
  __ ldr(r0, MemOperand(receiver, r6, LSL, kPointerSizeLog2));
  __ IncrementCounter(isolate->counters()->keyed_load_generic_lookup_cache(),
                      1, r4, r3);
  __ Ret();

  // Load property array property.
  __ bind(&property_array_property);
  __ ldr(receiver, FieldMemOperand(receiver, JSObject::kPropertiesOffset));
  __ add(receiver, receiver, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ ldr(r0, MemOperand(receiver, r5, LSL, kPointerSizeLog2));
  __ IncrementCounter(isolate->counters()->keyed_load_generic_lookup_cache(),
                      1, r4, r3);
  __ Ret();

  // Do a quick inline probe of the receiver's dictionary, if it
  // exists.
  __ bind(&probe_dictionary);
  // r3: elements
  __ ldr(r0, FieldMemOperand(receiver, HeapObject::kMapOffset));
  __ ldrb(r0, FieldMemOperand(r0, Map::kInstanceTypeOffset));
  GenerateGlobalInstanceTypeCheck(masm, r0, &slow);
  // Load the property to r0.
  GenerateDictionaryLoad(masm, &slow, r3, key, r0, r5, r4);
  __ IncrementCounter(
      isolate->counters()->keyed_load_generic_symbol(), 1, r4, r3);
  __ Ret();

  __ bind(&index_name);
  __ IndexFromHash(r3, key);
  // Now jump to the place where smi keys are handled.
  __ jmp(&index_smi);
}


void KeyedLoadIC::GenerateString(MacroAssembler* masm) {
  // Return address is in lr.
  Label miss;

  Register receiver = ReceiverRegister();
  Register index = NameRegister();
  Register scratch = r3;
  Register result = r0;
  DCHECK(!scratch.is(receiver) && !scratch.is(index));

  StringCharAtGenerator char_at_generator(receiver,
                                          index,
                                          scratch,
                                          result,
                                          &miss,  // When not a string.
                                          &miss,  // When not a number.
                                          &miss,  // When index out of range.
                                          STRING_INDEX_IS_ARRAY_INDEX);
  char_at_generator.GenerateFast(masm);
  __ Ret();

  StubRuntimeCallHelper call_helper;
  char_at_generator.GenerateSlow(masm, call_helper);

  __ bind(&miss);
  GenerateMiss(masm);
}


void KeyedLoadIC::GenerateIndexedInterceptor(MacroAssembler* masm) {
  // Return address is in lr.
  Label slow;

  Register receiver = ReceiverRegister();
  Register key = NameRegister();
  Register scratch1 = r3;
  Register scratch2 = r4;
  DCHECK(!scratch1.is(receiver) && !scratch1.is(key));
  DCHECK(!scratch2.is(receiver) && !scratch2.is(key));

  // Check that the receiver isn't a smi.
  __ JumpIfSmi(receiver, &slow);

  // Check that the key is an array index, that is Uint32.
  __ NonNegativeSmiTst(key);
  __ b(ne, &slow);

  // Get the map of the receiver.
  __ ldr(scratch1, FieldMemOperand(receiver, HeapObject::kMapOffset));

  // Check that it has indexed interceptor and access checks
  // are not enabled for this object.
  __ ldrb(scratch2, FieldMemOperand(scratch1, Map::kBitFieldOffset));
  __ and_(scratch2, scratch2, Operand(kSlowCaseBitFieldMask));
  __ cmp(scratch2, Operand(1 << Map::kHasIndexedInterceptor));
  __ b(ne, &slow);

  // Everything is fine, call runtime.
  __ Push(receiver, key);  // Receiver, key.

  // Perform tail call to the entry.
  __ TailCallExternalReference(
      ExternalReference(IC_Utility(kLoadElementWithInterceptor),
                        masm->isolate()),
      2, 1);

  __ bind(&slow);
  GenerateMiss(masm);
}


void KeyedStoreIC::GenerateMiss(MacroAssembler* masm) {
  // Push receiver, key and value for runtime call.
  __ Push(ReceiverRegister(), NameRegister(), ValueRegister());

  ExternalReference ref =
      ExternalReference(IC_Utility(kKeyedStoreIC_Miss), masm->isolate());
  __ TailCallExternalReference(ref, 3, 1);
}


void StoreIC::GenerateSlow(MacroAssembler* masm) {
  // Push receiver, key and value for runtime call.
  __ Push(ReceiverRegister(), NameRegister(), ValueRegister());

  // The slow case calls into the runtime to complete the store without causing
  // an IC miss that would otherwise cause a transition to the generic stub.
  ExternalReference ref =
      ExternalReference(IC_Utility(kStoreIC_Slow), masm->isolate());
  __ TailCallExternalReference(ref, 3, 1);
}


void KeyedStoreIC::GenerateSlow(MacroAssembler* masm) {
  // Push receiver, key and value for runtime call.
  __ Push(ReceiverRegister(), NameRegister(), ValueRegister());

  // The slow case calls into the runtime to complete the store without causing
  // an IC miss that would otherwise cause a transition to the generic stub.
  ExternalReference ref =
      ExternalReference(IC_Utility(kKeyedStoreIC_Slow), masm->isolate());
  __ TailCallExternalReference(ref, 3, 1);
}


void KeyedStoreIC::GenerateRuntimeSetProperty(MacroAssembler* masm,
                                              StrictMode strict_mode) {
  // Push receiver, key and value for runtime call.
  __ Push(ReceiverRegister(), NameRegister(), ValueRegister());

  __ mov(r0, Operand(Smi::FromInt(strict_mode)));   // Strict mode.
  __ Push(r0);

  __ TailCallRuntime(Runtime::kSetProperty, 4, 1);
}


static void KeyedStoreGenerateGenericHelper(
    MacroAssembler* masm,
    Label* fast_object,
    Label* fast_double,
    Label* slow,
    KeyedStoreCheckMap check_map,
    KeyedStoreIncrementLength increment_length,
    Register value,
    Register key,
    Register receiver,
    Register receiver_map,
    Register elements_map,
    Register elements) {
  Label transition_smi_elements;
  Label finish_object_store, non_double_value, transition_double_elements;
  Label fast_double_without_map_check;

  // Fast case: Do the store, could be either Object or double.
  __ bind(fast_object);
  Register scratch_value = r4;
  Register address = r5;
  if (check_map == kCheckMap) {
    __ ldr(elements_map, FieldMemOperand(elements, HeapObject::kMapOffset));
    __ cmp(elements_map,
           Operand(masm->isolate()->factory()->fixed_array_map()));
    __ b(ne, fast_double);
  }

  // HOLECHECK: guards "A[i] = V"
  // We have to go to the runtime if the current value is the hole because
  // there may be a callback on the element
  Label holecheck_passed1;
  __ add(address, elements, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ ldr(scratch_value,
         MemOperand::PointerAddressFromSmiKey(address, key, PreIndex));
  __ cmp(scratch_value, Operand(masm->isolate()->factory()->the_hole_value()));
  __ b(ne, &holecheck_passed1);
  __ JumpIfDictionaryInPrototypeChain(receiver, elements_map, scratch_value,
                                      slow);

  __ bind(&holecheck_passed1);

  // Smi stores don't require further checks.
  Label non_smi_value;
  __ JumpIfNotSmi(value, &non_smi_value);

  if (increment_length == kIncrementLength) {
    // Add 1 to receiver->length.
    __ add(scratch_value, key, Operand(Smi::FromInt(1)));
    __ str(scratch_value, FieldMemOperand(receiver, JSArray::kLengthOffset));
  }
  // It's irrelevant whether array is smi-only or not when writing a smi.
  __ add(address, elements, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ str(value, MemOperand::PointerAddressFromSmiKey(address, key));
  __ Ret();

  __ bind(&non_smi_value);
  // Escape to elements kind transition case.
  __ CheckFastObjectElements(receiver_map, scratch_value,
                             &transition_smi_elements);

  // Fast elements array, store the value to the elements backing store.
  __ bind(&finish_object_store);
  if (increment_length == kIncrementLength) {
    // Add 1 to receiver->length.
    __ add(scratch_value, key, Operand(Smi::FromInt(1)));
    __ str(scratch_value, FieldMemOperand(receiver, JSArray::kLengthOffset));
  }
  __ add(address, elements, Operand(FixedArray::kHeaderSize - kHeapObjectTag));
  __ add(address, address, Operand::PointerOffsetFromSmiKey(key));
  __ str(value, MemOperand(address));
  // Update write barrier for the elements array address.
  __ mov(scratch_value, value);  // Preserve the value which is returned.
  __ RecordWrite(elements,
                 address,
                 scratch_value,
                 kLRHasNotBeenSaved,
                 kDontSaveFPRegs,
                 EMIT_REMEMBERED_SET,
                 OMIT_SMI_CHECK);
  __ Ret();

  __ bind(fast_double);
  if (check_map == kCheckMap) {
    // Check for fast double array case. If this fails, call through to the
    // runtime.
    __ CompareRoot(elements_map, Heap::kFixedDoubleArrayMapRootIndex);
    __ b(ne, slow);
  }

  // HOLECHECK: guards "A[i] double hole?"
  // We have to see if the double version of the hole is present. If so
  // go to the runtime.
  __ add(address, elements,
         Operand((FixedDoubleArray::kHeaderSize + sizeof(kHoleNanLower32))
                 - kHeapObjectTag));
  __ ldr(scratch_value,
         MemOperand(address, key, LSL, kPointerSizeLog2, PreIndex));
  __ cmp(scratch_value, Operand(kHoleNanUpper32));
  __ b(ne, &fast_double_without_map_check);
  __ JumpIfDictionaryInPrototypeChain(receiver, elements_map, scratch_value,
                                      slow);

  __ bind(&fast_double_without_map_check);
  __ StoreNumberToDoubleElements(value, key, elements, r3, d0,
                                 &transition_double_elements);
  if (increment_length == kIncrementLength) {
    // Add 1 to receiver->length.
    __ add(scratch_value, key, Operand(Smi::FromInt(1)));
    __ str(scratch_value, FieldMemOperand(receiver, JSArray::kLengthOffset));
  }
  __ Ret();

  __ bind(&transition_smi_elements);
  // Transition the array appropriately depending on the value type.
  __ ldr(r4, FieldMemOperand(value, HeapObject::kMapOffset));
  __ CompareRoot(r4, Heap::kHeapNumberMapRootIndex);
  __ b(ne, &non_double_value);

  // Value is a double. Transition FAST_SMI_ELEMENTS ->
  // FAST_DOUBLE_ELEMENTS and complete the store.
  __ LoadTransitionedArrayMapConditional(FAST_SMI_ELEMENTS,
                                         FAST_DOUBLE_ELEMENTS,
                                         receiver_map,
                                         r4,
                                         slow);
  AllocationSiteMode mode = AllocationSite::GetMode(FAST_SMI_ELEMENTS,
                                                    FAST_DOUBLE_ELEMENTS);
  ElementsTransitionGenerator::GenerateSmiToDouble(
      masm, receiver, key, value, receiver_map, mode, slow);
  __ ldr(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ jmp(&fast_double_without_map_check);

  __ bind(&non_double_value);
  // Value is not a double, FAST_SMI_ELEMENTS -> FAST_ELEMENTS
  __ LoadTransitionedArrayMapConditional(FAST_SMI_ELEMENTS,
                                         FAST_ELEMENTS,
                                         receiver_map,
                                         r4,
                                         slow);
  mode = AllocationSite::GetMode(FAST_SMI_ELEMENTS, FAST_ELEMENTS);
  ElementsTransitionGenerator::GenerateMapChangeElementsTransition(
      masm, receiver, key, value, receiver_map, mode, slow);
  __ ldr(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ jmp(&finish_object_store);

  __ bind(&transition_double_elements);
  // Elements are FAST_DOUBLE_ELEMENTS, but value is an Object that's not a
  // HeapNumber. Make sure that the receiver is a Array with FAST_ELEMENTS and
  // transition array from FAST_DOUBLE_ELEMENTS to FAST_ELEMENTS
  __ LoadTransitionedArrayMapConditional(FAST_DOUBLE_ELEMENTS,
                                         FAST_ELEMENTS,
                                         receiver_map,
                                         r4,
                                         slow);
  mode = AllocationSite::GetMode(FAST_DOUBLE_ELEMENTS, FAST_ELEMENTS);
  ElementsTransitionGenerator::GenerateDoubleToObject(
      masm, receiver, key, value, receiver_map, mode, slow);
  __ ldr(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  __ jmp(&finish_object_store);
}


void KeyedStoreIC::GenerateGeneric(MacroAssembler* masm,
                                   StrictMode strict_mode) {
  // ---------- S t a t e --------------
  //  -- r0     : value
  //  -- r1     : key
  //  -- r2     : receiver
  //  -- lr     : return address
  // -----------------------------------
  Label slow, fast_object, fast_object_grow;
  Label fast_double, fast_double_grow;
  Label array, extra, check_if_double_array;

  // Register usage.
  Register value = ValueRegister();
  Register key = NameRegister();
  Register receiver = ReceiverRegister();
  DCHECK(receiver.is(r1));
  DCHECK(key.is(r2));
  DCHECK(value.is(r0));
  Register receiver_map = r3;
  Register elements_map = r6;
  Register elements = r9;  // Elements array of the receiver.
  // r4 and r5 are used as general scratch registers.

  // Check that the key is a smi.
  __ JumpIfNotSmi(key, &slow);
  // Check that the object isn't a smi.
  __ JumpIfSmi(receiver, &slow);
  // Get the map of the object.
  __ ldr(receiver_map, FieldMemOperand(receiver, HeapObject::kMapOffset));
  // Check that the receiver does not require access checks and is not observed.
  // The generic stub does not perform map checks or handle observed objects.
  __ ldrb(ip, FieldMemOperand(receiver_map, Map::kBitFieldOffset));
  __ tst(ip, Operand(1 << Map::kIsAccessCheckNeeded | 1 << Map::kIsObserved));
  __ b(ne, &slow);
  // Check if the object is a JS array or not.
  __ ldrb(r4, FieldMemOperand(receiver_map, Map::kInstanceTypeOffset));
  __ cmp(r4, Operand(JS_ARRAY_TYPE));
  __ b(eq, &array);
  // Check that the object is some kind of JSObject.
  __ cmp(r4, Operand(FIRST_JS_OBJECT_TYPE));
  __ b(lt, &slow);

  // Object case: Check key against length in the elements array.
  __ ldr(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));
  // Check array bounds. Both the key and the length of FixedArray are smis.
  __ ldr(ip, FieldMemOperand(elements, FixedArray::kLengthOffset));
  __ cmp(key, Operand(ip));
  __ b(lo, &fast_object);

  // Slow case, handle jump to runtime.
  __ bind(&slow);
  // Entry registers are intact.
  // r0: value.
  // r1: key.
  // r2: receiver.
  GenerateRuntimeSetProperty(masm, strict_mode);

  // Extra capacity case: Check if there is extra capacity to
  // perform the store and update the length. Used for adding one
  // element to the array by writing to array[array.length].
  __ bind(&extra);
  // Condition code from comparing key and array length is still available.
  __ b(ne, &slow);  // Only support writing to writing to array[array.length].
  // Check for room in the elements backing store.
  // Both the key and the length of FixedArray are smis.
  __ ldr(ip, FieldMemOperand(elements, FixedArray::kLengthOffset));
  __ cmp(key, Operand(ip));
  __ b(hs, &slow);
  __ ldr(elements_map, FieldMemOperand(elements, HeapObject::kMapOffset));
  __ cmp(elements_map,
         Operand(masm->isolate()->factory()->fixed_array_map()));
  __ b(ne, &check_if_double_array);
  __ jmp(&fast_object_grow);

  __ bind(&check_if_double_array);
  __ cmp(elements_map,
         Operand(masm->isolate()->factory()->fixed_double_array_map()));
  __ b(ne, &slow);
  __ jmp(&fast_double_grow);

  // Array case: Get the length and the elements array from the JS
  // array. Check that the array is in fast mode (and writable); if it
  // is the length is always a smi.
  __ bind(&array);
  __ ldr(elements, FieldMemOperand(receiver, JSObject::kElementsOffset));

  // Check the key against the length in the array.
  __ ldr(ip, FieldMemOperand(receiver, JSArray::kLengthOffset));
  __ cmp(key, Operand(ip));
  __ b(hs, &extra);

  KeyedStoreGenerateGenericHelper(masm, &fast_object, &fast_double,
                                  &slow, kCheckMap, kDontIncrementLength,
                                  value, key, receiver, receiver_map,
                                  elements_map, elements);
  KeyedStoreGenerateGenericHelper(masm, &fast_object_grow, &fast_double_grow,
                                  &slow, kDontCheckMap, kIncrementLength,
                                  value, key, receiver, receiver_map,
                                  elements_map, elements);
}


void StoreIC::GenerateMegamorphic(MacroAssembler* masm) {
  Register receiver = ReceiverRegister();
  Register name = NameRegister();
  DCHECK(receiver.is(r1));
  DCHECK(name.is(r2));
  DCHECK(ValueRegister().is(r0));

  // Get the receiver from the stack and probe the stub cache.
  Code::Flags flags = Code::RemoveTypeAndHolderFromFlags(
      Code::ComputeHandlerFlags(Code::STORE_IC));

  masm->isolate()->stub_cache()->GenerateProbe(
      masm, flags, receiver, name, r3, r4, r5, r6);

  // Cache miss: Jump to runtime.
  GenerateMiss(masm);
}


void StoreIC::GenerateMiss(MacroAssembler* masm) {
  __ Push(ReceiverRegister(), NameRegister(), ValueRegister());

  // Perform tail call to the entry.
  ExternalReference ref =
      ExternalReference(IC_Utility(kStoreIC_Miss), masm->isolate());
  __ TailCallExternalReference(ref, 3, 1);
}


void StoreIC::GenerateNormal(MacroAssembler* masm) {
  Label miss;
  Register receiver = ReceiverRegister();
  Register name = NameRegister();
  Register value = ValueRegister();
  Register dictionary = r3;
  DCHECK(receiver.is(r1));
  DCHECK(name.is(r2));
  DCHECK(value.is(r0));

  __ ldr(dictionary, FieldMemOperand(receiver, JSObject::kPropertiesOffset));

  GenerateDictionaryStore(masm, &miss, dictionary, name, value, r4, r5);
  Counters* counters = masm->isolate()->counters();
  __ IncrementCounter(counters->store_normal_hit(),
                      1, r4, r5);
  __ Ret();

  __ bind(&miss);
  __ IncrementCounter(counters->store_normal_miss(), 1, r4, r5);
  GenerateMiss(masm);
}


void StoreIC::GenerateRuntimeSetProperty(MacroAssembler* masm,
                                         StrictMode strict_mode) {
  __ Push(ReceiverRegister(), NameRegister(), ValueRegister());

  __ mov(r0, Operand(Smi::FromInt(strict_mode)));
  __ Push(r0);

  // Do tail-call to runtime routine.
  __ TailCallRuntime(Runtime::kSetProperty, 4, 1);
}


#undef __


Condition CompareIC::ComputeCondition(Token::Value op) {
  switch (op) {
    case Token::EQ_STRICT:
    case Token::EQ:
      return eq;
    case Token::LT:
      return lt;
    case Token::GT:
      return gt;
    case Token::LTE:
      return le;
    case Token::GTE:
      return ge;
    default:
      UNREACHABLE();
      return kNoCondition;
  }
}


bool CompareIC::HasInlinedSmiCode(Address address) {
  // The address of the instruction following the call.
  Address cmp_instruction_address =
      Assembler::return_address_from_call_start(address);

  // If the instruction following the call is not a cmp rx, #yyy, nothing
  // was inlined.
  Instr instr = Assembler::instr_at(cmp_instruction_address);
  return Assembler::IsCmpImmediate(instr);
}


void PatchInlinedSmiCode(Address address, InlinedSmiCheck check) {
  Address cmp_instruction_address =
      Assembler::return_address_from_call_start(address);

  // If the instruction following the call is not a cmp rx, #yyy, nothing
  // was inlined.
  Instr instr = Assembler::instr_at(cmp_instruction_address);
  if (!Assembler::IsCmpImmediate(instr)) {
    return;
  }

  // The delta to the start of the map check instruction and the
  // condition code uses at the patched jump.
  int delta = Assembler::GetCmpImmediateRawImmediate(instr);
  delta +=
      Assembler::GetCmpImmediateRegister(instr).code() * kOff12Mask;
  // If the delta is 0 the instruction is cmp r0, #0 which also signals that
  // nothing was inlined.
  if (delta == 0) {
    return;
  }

  if (FLAG_trace_ic) {
    PrintF("[  patching ic at %p, cmp=%p, delta=%d\n",
           address, cmp_instruction_address, delta);
  }

  Address patch_address =
      cmp_instruction_address - delta * Instruction::kInstrSize;
  Instr instr_at_patch = Assembler::instr_at(patch_address);
  Instr branch_instr =
      Assembler::instr_at(patch_address + Instruction::kInstrSize);
  // This is patching a conditional "jump if not smi/jump if smi" site.
  // Enabling by changing from
  //   cmp rx, rx
  //   b eq/ne, <target>
  // to
  //   tst rx, #kSmiTagMask
  //   b ne/eq, <target>
  // and vice-versa to be disabled again.
  CodePatcher patcher(patch_address, 2);
  Register reg = Assembler::GetRn(instr_at_patch);
  if (check == ENABLE_INLINED_SMI_CHECK) {
    DCHECK(Assembler::IsCmpRegister(instr_at_patch));
    DCHECK_EQ(Assembler::GetRn(instr_at_patch).code(),
              Assembler::GetRm(instr_at_patch).code());
    patcher.masm()->tst(reg, Operand(kSmiTagMask));
  } else {
    DCHECK(check == DISABLE_INLINED_SMI_CHECK);
    DCHECK(Assembler::IsTstImmediate(instr_at_patch));
    patcher.masm()->cmp(reg, reg);
  }
  DCHECK(Assembler::IsBranch(branch_instr));
  if (Assembler::GetCondition(branch_instr) == eq) {
    patcher.EmitCondition(ne);
  } else {
    DCHECK(Assembler::GetCondition(branch_instr) == ne);
    patcher.EmitCondition(eq);
  }
}


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM
