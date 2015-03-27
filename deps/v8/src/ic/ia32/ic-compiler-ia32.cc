// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_IA32

#include "src/ic/ic.h"
#include "src/ic/ic-compiler.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)


void PropertyICCompiler::GenerateRuntimeSetProperty(
    MacroAssembler* masm, LanguageMode language_mode) {
  // Return address is on the stack.
  DCHECK(!ebx.is(StoreDescriptor::ReceiverRegister()) &&
         !ebx.is(StoreDescriptor::NameRegister()) &&
         !ebx.is(StoreDescriptor::ValueRegister()));
  __ pop(ebx);
  __ push(StoreDescriptor::ReceiverRegister());
  __ push(StoreDescriptor::NameRegister());
  __ push(StoreDescriptor::ValueRegister());
  __ push(Immediate(Smi::FromInt(language_mode)));
  __ push(ebx);  // return address

  // Do tail-call to runtime routine.
  __ TailCallRuntime(Runtime::kSetProperty, 4, 1);
}


#undef __
#define __ ACCESS_MASM(masm())

Handle<Code> PropertyICCompiler::CompilePolymorphic(MapHandleList* maps,
                                                    CodeHandleList* handlers,
                                                    Handle<Name> name,
                                                    Code::StubType type,
                                                    IcCheckType check) {
  Label miss;

  if (check == PROPERTY &&
      (kind() == Code::KEYED_STORE_IC || kind() == Code::KEYED_LOAD_IC)) {
    // In case we are compiling an IC for dictionary loads or stores, just
    // check whether the name is unique.
    if (name.is_identical_to(isolate()->factory()->normal_ic_symbol())) {
      // Keyed loads with dictionaries shouldn't be here, they go generic.
      // The DCHECK is to protect assumptions when --vector-ics is on.
      DCHECK(kind() != Code::KEYED_LOAD_IC);
      Register tmp = scratch1();
      __ JumpIfSmi(this->name(), &miss);
      __ mov(tmp, FieldOperand(this->name(), HeapObject::kMapOffset));
      __ movzx_b(tmp, FieldOperand(tmp, Map::kInstanceTypeOffset));
      __ JumpIfNotUniqueNameInstanceType(tmp, &miss);
    } else {
      __ cmp(this->name(), Immediate(name));
      __ j(not_equal, &miss);
    }
  }

  Label number_case;
  Label* smi_target = IncludesNumberMap(maps) ? &number_case : &miss;
  __ JumpIfSmi(receiver(), smi_target);

  // Polymorphic keyed stores may use the map register
  Register map_reg = scratch1();
  DCHECK(kind() != Code::KEYED_STORE_IC ||
         map_reg.is(ElementTransitionAndStoreDescriptor::MapRegister()));
  __ mov(map_reg, FieldOperand(receiver(), HeapObject::kMapOffset));
  int receiver_count = maps->length();
  int number_of_handled_maps = 0;
  for (int current = 0; current < receiver_count; ++current) {
    Handle<Map> map = maps->at(current);
    if (!map->is_deprecated()) {
      number_of_handled_maps++;
      Handle<WeakCell> cell = Map::WeakCellForMap(map);
      __ CmpWeakValue(map_reg, cell, scratch2());
      if (map->instance_type() == HEAP_NUMBER_TYPE) {
        DCHECK(!number_case.is_unused());
        __ bind(&number_case);
      }
      __ j(equal, handlers->at(current));
    }
  }
  DCHECK(number_of_handled_maps != 0);

  __ bind(&miss);
  TailCallBuiltin(masm(), MissBuiltin(kind()));

  // Return the generated code.
  InlineCacheState state =
      number_of_handled_maps > 1 ? POLYMORPHIC : MONOMORPHIC;
  return GetCode(kind(), type, name, state);
}


Handle<Code> PropertyICCompiler::CompileKeyedStorePolymorphic(
    MapHandleList* receiver_maps, CodeHandleList* handler_stubs,
    MapHandleList* transitioned_maps) {
  Label miss;
  __ JumpIfSmi(receiver(), &miss);
  Register map_reg = scratch1();
  __ mov(map_reg, FieldOperand(receiver(), HeapObject::kMapOffset));
  for (int i = 0; i < receiver_maps->length(); ++i) {
    Handle<WeakCell> cell = Map::WeakCellForMap(receiver_maps->at(i));
    __ CmpWeakValue(map_reg, cell, scratch2());
    if (transitioned_maps->at(i).is_null()) {
      __ j(equal, handler_stubs->at(i));
    } else {
      Label next_map;
      __ j(not_equal, &next_map, Label::kNear);
      Handle<WeakCell> cell = Map::WeakCellForMap(transitioned_maps->at(i));
      __ LoadWeakValue(transition_map(), cell, &miss);
      __ jmp(handler_stubs->at(i), RelocInfo::CODE_TARGET);
      __ bind(&next_map);
    }
  }
  __ bind(&miss);
  TailCallBuiltin(masm(), MissBuiltin(kind()));

  // Return the generated code.
  return GetCode(kind(), Code::NORMAL, factory()->empty_string(), POLYMORPHIC);
}


#undef __
}
}  // namespace v8::internal

#endif  // V8_TARGET_ARCH_IA32
