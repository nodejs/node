// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ffi/ffi-compiler.h"
#include "src/api.h"
#include "src/code-factory.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

void InstallFFIMap(Isolate* isolate) {
  Handle<Context> context(isolate->context());
  DCHECK(!context->get(Context::NATIVE_FUNCTION_MAP_INDEX)->IsMap());
  Handle<Map> prev_map = Handle<Map>(context->sloppy_function_map(), isolate);

  InstanceType instance_type = prev_map->instance_type();
  int embedder_fields = JSObject::GetEmbedderFieldCount(*prev_map);
  CHECK_EQ(0, embedder_fields);
  int pre_allocated =
      prev_map->GetInObjectProperties() - prev_map->unused_property_fields();
  int instance_size;
  int in_object_properties;
  JSFunction::CalculateInstanceSizeHelper(
      instance_type, embedder_fields, 0, &instance_size, &in_object_properties);
  int unused_property_fields = in_object_properties - pre_allocated;
  Handle<Map> map = Map::CopyInitialMap(
      prev_map, instance_size, in_object_properties, unused_property_fields);
  context->set_native_function_map(*map);
}

namespace ffi {

class FFIAssembler : public CodeStubAssembler {
 public:
  explicit FFIAssembler(CodeAssemblerState* state) : CodeStubAssembler(state) {}

  Node* ToJS(Node* node, Node* context, FFIType type) {
    switch (type) {
      case FFIType::kInt32:
        return ChangeInt32ToTagged(node);
    }
    UNREACHABLE();
    return nullptr;
  }

  Node* FromJS(Node* node, Node* context, FFIType type) {
    switch (type) {
      case FFIType::kInt32:
        return TruncateTaggedToWord32(context, node);
    }
    UNREACHABLE();
    return nullptr;
  }

  MachineType FFIToMachineType(FFIType type) {
    switch (type) {
      case FFIType::kInt32:
        return MachineType::Int32();
    }
    UNREACHABLE();
    return MachineType::None();
  }

  Signature<MachineType>* FFIToMachineSignature(FFISignature* sig) {
    Signature<MachineType>::Builder sig_builder(zone(), sig->return_count(),
                                                sig->parameter_count());
    for (size_t i = 0; i < sig->return_count(); i++) {
      sig_builder.AddReturn(FFIToMachineType(sig->GetReturn(i)));
    }
    for (size_t j = 0; j < sig->parameter_count(); j++) {
      sig_builder.AddParam(FFIToMachineType(sig->GetParam(j)));
    }
    return sig_builder.Build();
  }

  void GenerateJSToNativeWrapper(NativeFunction* func) {
    int params = static_cast<int>(func->sig->parameter_count());
    int returns = static_cast<int>(func->sig->return_count());
    ApiFunction api_func(func->start);
    ExternalReference ref(&api_func, ExternalReference::BUILTIN_CALL,
                          isolate());

    Node* context_param = GetJSContextParameter();

    Node** inputs = zone()->NewArray<Node*>(params + 1);
    int input_count = 0;
    inputs[input_count++] = ExternalConstant(ref);
    for (int i = 0; i < params; i++) {
      inputs[input_count++] =
          FromJS(Parameter(i), context_param, func->sig->GetParam(i));
    }

    Node* call =
        CallCFunctionN(FFIToMachineSignature(func->sig), input_count, inputs);
    Node* return_val = UndefinedConstant();
    if (returns == 1) {
      return_val = ToJS(call, context_param, func->sig->GetReturn());
    }
    Return(return_val);
  }
};

Handle<JSFunction> CompileJSToNativeWrapper(Isolate* isolate,
                                            Handle<String> name,
                                            NativeFunction func) {
  int params = static_cast<int>(func.sig->parameter_count());
  Zone zone(isolate->allocator(), ZONE_NAME);
  CodeAssemblerState state(isolate, &zone, params,
                           Code::ComputeFlags(Code::BUILTIN), "js-to-native");
  FFIAssembler assembler(&state);
  assembler.GenerateJSToNativeWrapper(&func);
  Handle<Code> code = assembler.GenerateCode(&state);

  Handle<SharedFunctionInfo> shared =
      isolate->factory()->NewSharedFunctionInfo(name, code, false);
  shared->set_length(params);
  shared->set_internal_formal_parameter_count(params);
  Handle<JSFunction> function = isolate->factory()->NewFunction(
      isolate->native_function_map(), name, code);
  function->set_shared(*shared);
  return function;
}

}  // namespace ffi
}  // namespace internal
}  // namespace v8
