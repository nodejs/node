// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <cstdint>

#include "src/compilation-info.h"
#include "src/compiler/graph.h"
#include "src/compiler/instruction-selector.h"
#include "src/compiler/linkage.h"
#include "src/compiler/node.h"
#include "src/compiler/operator.h"
#include "src/compiler/pipeline.h"
#include "src/compiler/raw-machine-assembler.h"
#include "src/machine-type.h"
#include "src/objects-inl.h"
#include "src/objects.h"
#include "src/simulator.h"
#include "src/zone/accounting-allocator.h"
#include "src/zone/zone.h"
#include "test/fuzzer/fuzzer-support.h"

namespace v8 {
namespace internal {
namespace compiler {
namespace fuzzer {

constexpr MachineType kTypes[] = {
    // The first entry is just a placeholder, because '0' is a separator.
    MachineType(),
#if !V8_TARGET_ARCH_32_BIT
    MachineType::Int64(),
#endif
    MachineType::Int32(), MachineType::Float32(), MachineType::Float64()};

static constexpr int kNumTypes = arraysize(kTypes);

class InputProvider {
 public:
  InputProvider(const uint8_t* data, size_t size)
      : current_(data), end_(data + size) {}

  size_t NumNonZeroBytes(size_t offset, int limit) {
    DCHECK_LE(limit, std::numeric_limits<uint8_t>::max());
    DCHECK_GE(current_ + offset, current_);
    const uint8_t* p;
    for (p = current_ + offset; p < end_; ++p) {
      if (*p % limit == 0) break;
    }
    return p - current_ - offset;
  }

  int NextInt8(int limit) {
    DCHECK_LE(limit, std::numeric_limits<uint8_t>::max());
    if (current_ == end_) return 0;
    uint8_t result = *current_;
    current_++;
    return static_cast<int>(result) % limit;
  }

  int NextInt32(int limit) {
    if (current_ + sizeof(uint32_t) > end_) return 0;
    int result = ReadLittleEndianValue<int>(current_);
    current_ += sizeof(uint32_t);
    return result % limit;
  }

 private:
  const uint8_t* current_;
  const uint8_t* end_;
};

MachineType RandomType(InputProvider* input) {
  return kTypes[input->NextInt8(kNumTypes)];
}

int num_registers(MachineType type) {
  const RegisterConfiguration* config = RegisterConfiguration::Default();
  switch (type.representation()) {
    case MachineRepresentation::kWord32:
    case MachineRepresentation::kWord64:
      return config->num_allocatable_general_registers();
    case MachineRepresentation::kFloat32:
      return config->num_allocatable_float_registers();
    case MachineRepresentation::kFloat64:
      return config->num_allocatable_double_registers();
    default:
      UNREACHABLE();
  }
}

int size(MachineType type) {
  return 1 << ElementSizeLog2Of(type.representation());
}

int index(MachineType type) { return static_cast<int>(type.representation()); }

const int* codes(MachineType type) {
  const RegisterConfiguration* config = RegisterConfiguration::Default();
  switch (type.representation()) {
    case MachineRepresentation::kWord32:
    case MachineRepresentation::kWord64:
      return config->allocatable_general_codes();
    case MachineRepresentation::kFloat32:
      return config->allocatable_float_codes();
    case MachineRepresentation::kFloat64:
      return config->allocatable_double_codes();
    default:
      UNREACHABLE();
  }
}

LinkageLocation AllocateLocation(MachineType type, int* int_count,
                                 int* float_count, int* stack_slots) {
  int* count = IsFloatingPoint(type.representation()) ? float_count : int_count;
  int reg_code = *count;
#if V8_TARGET_ARCH_ARM
  // Allocate floats using a double register, but modify the code to
  // reflect how ARM FP registers alias.
  if (type == MachineType::Float32()) {
    reg_code *= 2;
  }
#endif
  LinkageLocation location = LinkageLocation::ForAnyRegister();  // Dummy.
  if (reg_code < num_registers(type)) {
    location = LinkageLocation::ForRegister(codes(type)[reg_code], type);
  } else {
    location = LinkageLocation::ForCallerFrameSlot(-*stack_slots - 1, type);
    *stack_slots += std::max(1, size(type) / kPointerSize);
  }
  ++*count;
  return location;
}

Node* Constant(RawMachineAssembler& m, MachineType type, int value) {
  switch (type.representation()) {
    case MachineRepresentation::kWord32:
      return m.Int32Constant(static_cast<int32_t>(value));
    case MachineRepresentation::kWord64:
      return m.Int64Constant(static_cast<int64_t>(value));
    case MachineRepresentation::kFloat32:
      return m.Float32Constant(static_cast<float>(value));
    case MachineRepresentation::kFloat64:
      return m.Float64Constant(static_cast<double>(value));
    default:
      UNREACHABLE();
  }
}

Node* ToInt32(RawMachineAssembler& m, MachineType type, Node* a) {
  switch (type.representation()) {
    case MachineRepresentation::kWord32:
      return a;
    case MachineRepresentation::kWord64:
      return m.TruncateInt64ToInt32(a);
    case MachineRepresentation::kFloat32:
      return m.TruncateFloat32ToInt32(a);
    case MachineRepresentation::kFloat64:
      return m.RoundFloat64ToInt32(a);
    default:
      UNREACHABLE();
  }
}

CallDescriptor* CreateRandomCallDescriptor(Zone* zone, size_t return_count,
                                           size_t param_count,
                                           InputProvider* input) {
  LocationSignature::Builder locations(zone, return_count, param_count);

  int stack_slots = 0;
  int int_params = 0;
  int float_params = 0;
  for (size_t i = 0; i < param_count; i++) {
    MachineType type = RandomType(input);
    LinkageLocation location =
        AllocateLocation(type, &int_params, &float_params, &stack_slots);
    locations.AddParam(location);
  }
  // Read the end byte of the parameters.
  input->NextInt8(1);

  int stack_params = stack_slots;
#if V8_TARGET_ARCH_ARM64
  // Align the stack slots.
  stack_slots = stack_slots + (stack_slots % 2);
#endif
  int aligned_stack_params = stack_slots;
  int int_returns = 0;
  int float_returns = 0;
  for (size_t i = 0; i < return_count; i++) {
    MachineType type = RandomType(input);
    LinkageLocation location =
        AllocateLocation(type, &int_returns, &float_returns, &stack_slots);
    locations.AddReturn(location);
  }
  int stack_returns = stack_slots - aligned_stack_params;

  MachineType target_type = MachineType::AnyTagged();
  LinkageLocation target_loc = LinkageLocation::ForAnyRegister(target_type);
  return new (zone) CallDescriptor(       // --
      CallDescriptor::kCallCodeObject,    // kind
      target_type,                        // target MachineType
      target_loc,                         // target location
      locations.Build(),                  // location_sig
      stack_params,                       // on-stack parameter count
      compiler::Operator::kNoProperties,  // properties
      0,                                  // callee-saved registers
      0,                                  // callee-saved fp regs
      CallDescriptor::kNoFlags,           // flags
      "c-call",                           // debug name
      0,                                  // allocatable registers
      stack_returns);                     // on-stack return count
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  v8_fuzzer::FuzzerSupport* support = v8_fuzzer::FuzzerSupport::Get();
  v8::Isolate* isolate = support->GetIsolate();
  i::Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(support->GetContext());
  v8::TryCatch try_catch(isolate);
  v8::internal::AccountingAllocator allocator;
  Zone zone(&allocator, ZONE_NAME);

  InputProvider input(data, size);
  // Create randomized descriptor.
  size_t param_count = input.NumNonZeroBytes(0, kNumTypes);
  size_t return_count = input.NumNonZeroBytes(param_count + 1, kNumTypes);
  CallDescriptor* desc =
      CreateRandomCallDescriptor(&zone, return_count, param_count, &input);

  if (FLAG_wasm_fuzzer_gen_test) {
    // Print some debugging output which describes the produced signature.
    printf("[");
    for (size_t j = 0; j < desc->ParameterCount(); ++j) {
      printf(" %s",
             MachineReprToString(desc->GetParameterType(j).representation()));
    }
    printf(" ] -> [");
    for (size_t j = 0; j < desc->ReturnCount(); ++j) {
      printf(" %s",
             MachineReprToString(desc->GetReturnType(j).representation()));
    }
    printf(" ]\n\n");
  }

  // Count parameters of each type.
  constexpr size_t kNumMachineRepresentations =
      static_cast<size_t>(MachineRepresentation::kLastRepresentation) + 1;

  // Trivial hash table for the number of occurrences of parameter types. The
  // MachineRepresentation of the parameter types is used as hash code.
  int counts[kNumMachineRepresentations] = {0};
  for (size_t i = 0; i < desc->ParameterCount(); ++i) {
    ++counts[index(desc->GetParameterType(i))];
  }

  // Generate random inputs.
  std::unique_ptr<int[]> inputs(new int[desc->ParameterCount()]);
  std::unique_ptr<int[]> outputs(new int[desc->ReturnCount()]);
  for (size_t i = 0; i < desc->ParameterCount(); ++i) {
    inputs[i] = input.NextInt32(10000);
  }

  RawMachineAssembler callee(
      i_isolate, new (&zone) Graph(&zone), desc,
      MachineType::PointerRepresentation(),
      InstructionSelector::SupportedMachineOperatorFlags());

  // Generate callee, returning random picks of its parameters.
  std::unique_ptr<Node* []> params(new Node*[desc->ParameterCount() + 1]);
  std::unique_ptr<Node* []> returns(new Node*[desc->ReturnCount()]);
  for (size_t i = 0; i < desc->ParameterCount(); ++i) {
    params[i] = callee.Parameter(i);
  }
  for (size_t i = 0; i < desc->ReturnCount(); ++i) {
    MachineType type = desc->GetReturnType(i);
    // Find a random same-type parameter to return. Use a constant if none.
    if (counts[index(type)] == 0) {
      returns[i] = Constant(callee, type, 42);
      outputs[i] = 42;
    } else {
      int n = input.NextInt8(counts[index(type)]);
      int k = 0;
      while (desc->GetParameterType(k) != desc->GetReturnType(i) || --n > 0) {
        ++k;
      }
      returns[i] = params[k];
      outputs[i] = inputs[k];
    }
  }
  callee.Return(static_cast<int>(desc->ReturnCount()), returns.get());

  CompilationInfo info(ArrayVector("testing"), &zone, Code::STUB);
  Handle<Code> code = Pipeline::GenerateCodeForTesting(
      &info, i_isolate, desc, callee.graph(), callee.Export());

  // Generate wrapper.
  int expect = 0;

  MachineSignature::Builder sig_builder(&zone, 1, 0);
  sig_builder.AddReturn(MachineType::Int32());

  CallDescriptor* wrapper_desc =
      Linkage::GetSimplifiedCDescriptor(&zone, sig_builder.Build());
  RawMachineAssembler caller(
      i_isolate, new (&zone) Graph(&zone), wrapper_desc,
      MachineType::PointerRepresentation(),
      InstructionSelector::SupportedMachineOperatorFlags());

  params[0] = caller.HeapConstant(code);
  for (size_t i = 0; i < desc->ParameterCount(); ++i) {
    params[i + 1] = Constant(caller, desc->GetParameterType(i), inputs[i]);
  }
  Node* call = caller.AddNode(caller.common()->Call(desc),
                              static_cast<int>(desc->ParameterCount() + 1),
                              params.get());
  Node* ret = Constant(caller, MachineType::Int32(), 0);
  for (size_t i = 0; i < desc->ReturnCount(); ++i) {
    // Skip roughly one third of the outputs.
    if (input.NextInt8(3) == 0) continue;
    Node* ret_i = (desc->ReturnCount() == 1)
                      ? call
                      : caller.AddNode(caller.common()->Projection(i), call);
    ret = caller.Int32Add(ret, ToInt32(caller, desc->GetReturnType(i), ret_i));
    expect += outputs[i];
  }
  caller.Return(ret);

  // Call the wrapper.
  CompilationInfo wrapper_info(ArrayVector("wrapper"), &zone, Code::STUB);
  Handle<Code> wrapper_code = Pipeline::GenerateCodeForTesting(
      &wrapper_info, i_isolate, wrapper_desc, caller.graph(), caller.Export());
  auto fn = GeneratedCode<int32_t>::FromCode(*wrapper_code);
  int result = fn.Call();

  CHECK_EQ(expect, result);
  return 0;
}

}  // namespace fuzzer
}  // namespace compiler
}  // namespace internal
}  // namespace v8
