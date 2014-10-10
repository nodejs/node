// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#if V8_TARGET_ARCH_ARM64

#include "src/ic/ic.h"
#include "src/ic/ic-compiler.h"

namespace v8 {
namespace internal {

#define __ ACCESS_MASM(masm)

void PropertyICCompiler::GenerateRuntimeSetProperty(MacroAssembler* masm,
                                                    StrictMode strict_mode) {
  ASM_LOCATION("PropertyICCompiler::GenerateRuntimeSetProperty");

  __ Push(StoreDescriptor::ReceiverRegister(), StoreDescriptor::NameRegister(),
          StoreDescriptor::ValueRegister());

  __ Mov(x10, Smi::FromInt(strict_mode));
  __ Push(x10);

  // Do tail-call to runtime routine.
  __ TailCallRuntime(Runtime::kSetProperty, 4, 1);
}


#undef __
#define __ ACCESS_MASM(masm())


Handle<Code> PropertyICCompiler::CompilePolymorphic(TypeHandleList* types,
                                                    CodeHandleList* handlers,
                                                    Handle<Name> name,
                                                    Code::StubType type,
                                                    IcCheckType check) {
  Label miss;

  if (check == PROPERTY &&
      (kind() == Code::KEYED_LOAD_IC || kind() == Code::KEYED_STORE_IC)) {
    // In case we are compiling an IC for dictionary loads and stores, just
    // check whether the name is unique.
    if (name.is_identical_to(isolate()->factory()->normal_ic_symbol())) {
      Register tmp = scratch1();
      __ JumpIfSmi(this->name(), &miss);
      __ Ldr(tmp, FieldMemOperand(this->name(), HeapObject::kMapOffset));
      __ Ldrb(tmp, FieldMemOperand(tmp, Map::kInstanceTypeOffset));
      __ JumpIfNotUniqueNameInstanceType(tmp, &miss);
    } else {
      __ CompareAndBranch(this->name(), Operand(name), ne, &miss);
    }
  }

  Label number_case;
  Label* smi_target = IncludesNumberType(types) ? &number_case : &miss;
  __ JumpIfSmi(receiver(), smi_target);

  // Polymorphic keyed stores may use the map register
  Register map_reg = scratch1();
  DCHECK(kind() != Code::KEYED_STORE_IC ||
         map_reg.is(ElementTransitionAndStoreDescriptor::MapRegister()));
  __ Ldr(map_reg, FieldMemOperand(receiver(), HeapObject::kMapOffset));
  int receiver_count = types->length();
  int number_of_handled_maps = 0;
  for (int current = 0; current < receiver_count; ++current) {
    Handle<HeapType> type = types->at(current);
    Handle<Map> map = IC::TypeToMap(*type, isolate());
    if (!map->is_deprecated()) {
      number_of_handled_maps++;
      Label try_next;
      __ Cmp(map_reg, Operand(map));
      __ B(ne, &try_next);
      if (type->Is(HeapType::Number())) {
        DCHECK(!number_case.is_unused());
        __ Bind(&number_case);
      }
      __ Jump(handlers->at(current), RelocInfo::CODE_TARGET);
      __ Bind(&try_next);
    }
  }
  DCHECK(number_of_handled_maps != 0);

  __ Bind(&miss);
  TailCallBuiltin(masm(), MissBuiltin(kind()));

  // Return the generated code.
  InlineCacheState state =
      (number_of_handled_maps > 1) ? POLYMORPHIC : MONOMORPHIC;
  return GetCode(kind(), type, name, state);
}


Handle<Code> PropertyICCompiler::CompileKeyedStorePolymorphic(
    MapHandleList* receiver_maps, CodeHandleList* handler_stubs,
    MapHandleList* transitioned_maps) {
  Label miss;

  ASM_LOCATION("PropertyICCompiler::CompileStorePolymorphic");

  __ JumpIfSmi(receiver(), &miss);

  int receiver_count = receiver_maps->length();
  __ Ldr(scratch1(), FieldMemOperand(receiver(), HeapObject::kMapOffset));
  for (int i = 0; i < receiver_count; i++) {
    __ Cmp(scratch1(), Operand(receiver_maps->at(i)));

    Label skip;
    __ B(&skip, ne);
    if (!transitioned_maps->at(i).is_null()) {
      // This argument is used by the handler stub. For example, see
      // ElementsTransitionGenerator::GenerateMapChangeElementsTransition.
      __ Mov(transition_map(), Operand(transitioned_maps->at(i)));
    }
    __ Jump(handler_stubs->at(i), RelocInfo::CODE_TARGET);
    __ Bind(&skip);
  }

  __ Bind(&miss);
  TailCallBuiltin(masm(), MissBuiltin(kind()));

  return GetCode(kind(), Code::NORMAL, factory()->empty_string(), POLYMORPHIC);
}


#undef __
}
}  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM64
