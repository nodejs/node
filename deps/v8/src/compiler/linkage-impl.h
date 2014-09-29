// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_LINKAGE_IMPL_H_
#define V8_COMPILER_LINKAGE_IMPL_H_

namespace v8 {
namespace internal {
namespace compiler {

class LinkageHelper {
 public:
  static LinkageLocation TaggedStackSlot(int index) {
    DCHECK(index < 0);
    return LinkageLocation(kMachineTagged, index);
  }

  static LinkageLocation TaggedRegisterLocation(Register reg) {
    return LinkageLocation(kMachineTagged, Register::ToAllocationIndex(reg));
  }

  static inline LinkageLocation WordRegisterLocation(Register reg) {
    return LinkageLocation(MachineOperatorBuilder::pointer_rep(),
                           Register::ToAllocationIndex(reg));
  }

  static LinkageLocation UnconstrainedRegister(MachineType rep) {
    return LinkageLocation(rep, LinkageLocation::ANY_REGISTER);
  }

  static const RegList kNoCalleeSaved = 0;

  // TODO(turbofan): cache call descriptors for JSFunction calls.
  template <typename LinkageTraits>
  static CallDescriptor* GetJSCallDescriptor(Zone* zone, int parameter_count) {
    const int jsfunction_count = 1;
    const int context_count = 1;
    int input_count = jsfunction_count + parameter_count + context_count;

    const int return_count = 1;
    LinkageLocation* locations =
        zone->NewArray<LinkageLocation>(return_count + input_count);

    int index = 0;
    locations[index++] =
        TaggedRegisterLocation(LinkageTraits::ReturnValueReg());
    locations[index++] =
        TaggedRegisterLocation(LinkageTraits::JSCallFunctionReg());

    for (int i = 0; i < parameter_count; i++) {
      // All parameters to JS calls go on the stack.
      int spill_slot_index = i - parameter_count;
      locations[index++] = TaggedStackSlot(spill_slot_index);
    }
    locations[index++] = TaggedRegisterLocation(LinkageTraits::ContextReg());

    // TODO(titzer): refactor TurboFan graph to consider context a value input.
    return new (zone)
        CallDescriptor(CallDescriptor::kCallJSFunction,  // kind
                       return_count,                     // return_count
                       parameter_count,                  // parameter_count
                       input_count - context_count,      // input_count
                       locations,                        // locations
                       Operator::kNoProperties,          // properties
                       kNoCalleeSaved,  // callee-saved registers
                       CallDescriptor::kCanDeoptimize);  // deoptimization
  }


  // TODO(turbofan): cache call descriptors for runtime calls.
  template <typename LinkageTraits>
  static CallDescriptor* GetRuntimeCallDescriptor(
      Zone* zone, Runtime::FunctionId function_id, int parameter_count,
      Operator::Property properties,
      CallDescriptor::DeoptimizationSupport can_deoptimize) {
    const int code_count = 1;
    const int function_count = 1;
    const int num_args_count = 1;
    const int context_count = 1;
    const int input_count = code_count + parameter_count + function_count +
                            num_args_count + context_count;

    const Runtime::Function* function = Runtime::FunctionForId(function_id);
    const int return_count = function->result_size;
    LinkageLocation* locations =
        zone->NewArray<LinkageLocation>(return_count + input_count);

    int index = 0;
    if (return_count > 0) {
      locations[index++] =
          TaggedRegisterLocation(LinkageTraits::ReturnValueReg());
    }
    if (return_count > 1) {
      locations[index++] =
          TaggedRegisterLocation(LinkageTraits::ReturnValue2Reg());
    }

    DCHECK_LE(return_count, 2);

    locations[index++] = UnconstrainedRegister(kMachineTagged);  // CEntryStub

    for (int i = 0; i < parameter_count; i++) {
      // All parameters to runtime calls go on the stack.
      int spill_slot_index = i - parameter_count;
      locations[index++] = TaggedStackSlot(spill_slot_index);
    }
    locations[index++] =
        TaggedRegisterLocation(LinkageTraits::RuntimeCallFunctionReg());
    locations[index++] =
        WordRegisterLocation(LinkageTraits::RuntimeCallArgCountReg());
    locations[index++] = TaggedRegisterLocation(LinkageTraits::ContextReg());

    // TODO(titzer): refactor TurboFan graph to consider context a value input.
    return new (zone) CallDescriptor(CallDescriptor::kCallCodeObject,  // kind
                                     return_count,     // return_count
                                     parameter_count,  // parameter_count
                                     input_count,      // input_count
                                     locations,        // locations
                                     properties,       // properties
                                     kNoCalleeSaved,   // callee-saved registers
                                     can_deoptimize,   // deoptimization
                                     function->name);
  }


  // TODO(turbofan): cache call descriptors for code stub calls.
  template <typename LinkageTraits>
  static CallDescriptor* GetStubCallDescriptor(
      Zone* zone, CodeStubInterfaceDescriptor* descriptor,
      int stack_parameter_count,
      CallDescriptor::DeoptimizationSupport can_deoptimize) {
    int register_parameter_count = descriptor->GetEnvironmentParameterCount();
    int parameter_count = register_parameter_count + stack_parameter_count;
    const int code_count = 1;
    const int context_count = 1;
    int input_count = code_count + parameter_count + context_count;

    const int return_count = 1;
    LinkageLocation* locations =
        zone->NewArray<LinkageLocation>(return_count + input_count);

    int index = 0;
    locations[index++] =
        TaggedRegisterLocation(LinkageTraits::ReturnValueReg());
    locations[index++] = UnconstrainedRegister(kMachineTagged);  // code
    for (int i = 0; i < parameter_count; i++) {
      if (i < register_parameter_count) {
        // The first parameters to code stub calls go in registers.
        Register reg = descriptor->GetEnvironmentParameterRegister(i);
        locations[index++] = TaggedRegisterLocation(reg);
      } else {
        // The rest of the parameters go on the stack.
        int stack_slot = i - register_parameter_count - stack_parameter_count;
        locations[index++] = TaggedStackSlot(stack_slot);
      }
    }
    locations[index++] = TaggedRegisterLocation(LinkageTraits::ContextReg());

    // TODO(titzer): refactor TurboFan graph to consider context a value input.
    return new (zone)
        CallDescriptor(CallDescriptor::kCallCodeObject,  // kind
                       return_count,                     // return_count
                       parameter_count,                  // parameter_count
                       input_count,                      // input_count
                       locations,                        // locations
                       Operator::kNoProperties,          // properties
                       kNoCalleeSaved,  // callee-saved registers
                       can_deoptimize,  // deoptimization
                       CodeStub::MajorName(descriptor->MajorKey(), false));
  }


  template <typename LinkageTraits>
  static CallDescriptor* GetSimplifiedCDescriptor(
      Zone* zone, int num_params, MachineType return_type,
      const MachineType* param_types) {
    LinkageLocation* locations =
        zone->NewArray<LinkageLocation>(num_params + 2);
    int index = 0;
    locations[index++] =
        TaggedRegisterLocation(LinkageTraits::ReturnValueReg());
    locations[index++] = LinkageHelper::UnconstrainedRegister(
        MachineOperatorBuilder::pointer_rep());
    // TODO(dcarney): test with lots of parameters.
    int i = 0;
    for (; i < LinkageTraits::CRegisterParametersLength() && i < num_params;
         i++) {
      locations[index++] = LinkageLocation(
          param_types[i],
          Register::ToAllocationIndex(LinkageTraits::CRegisterParameter(i)));
    }
    for (; i < num_params; i++) {
      locations[index++] = LinkageLocation(param_types[i], -1 - i);
    }
    return new (zone) CallDescriptor(
        CallDescriptor::kCallAddress, 1, num_params, num_params + 1, locations,
        Operator::kNoProperties, LinkageTraits::CCalleeSaveRegisters(),
        CallDescriptor::kCannotDeoptimize);  // TODO(jarin) should deoptimize!
  }
};
}
}
}  // namespace v8::internal::compiler

#endif  // V8_COMPILER_LINKAGE_IMPL_H_
