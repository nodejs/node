// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_WASM_COMPILER_H_
#define V8_COMPILER_WASM_COMPILER_H_

#include <memory>

// Clients of this interface shouldn't depend on lots of compiler internals.
// Do not include anything from src/compiler here!
#include "src/compilation-info.h"
#include "src/compiler.h"
#include "src/trap-handler/trap-handler.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes.h"
#include "src/wasm/wasm-result.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

namespace compiler {
// Forward declarations for some compiler data structures.
class Node;
class JSGraph;
class Graph;
class Operator;
class SourcePositionTable;
}  // namespace compiler

namespace wasm {
struct DecodeStruct;
class SignatureMap;
// Expose {Node} and {Graph} opaquely as {wasm::TFNode} and {wasm::TFGraph}.
typedef compiler::Node TFNode;
typedef compiler::JSGraph TFGraph;
}  // namespace wasm

namespace compiler {

// The {ModuleEnv} encapsulates the module data that is used by the
// {WasmGraphBuilder} during graph building. It represents the parameters to
// which the  compiled code should be specialized, including which code to call
// for direct calls {function_code}, which tables to use for indirect calls
// {function_tables}, memory start address and size {mem_start, mem_size},
// globals start address {globals_start}, as well as signature maps
// {signature_maps} and the module itself {module}.
// ModuleEnvs are shareable across multiple compilations.
struct ModuleEnv {
  // A pointer to the decoded module's static representation.
  const wasm::WasmModule* module;
  // The function tables are FixedArrays of code used to dispatch indirect
  // calls. (the same length as module.function_tables). We use the address
  // to a global handle to the FixedArray.
  const std::vector<Address> function_tables;
  // The signatures tables are FixedArrays of SMIs used to check signatures
  // match at runtime.
  // (the same length as module.function_tables)
  //  We use the address to a global handle to the FixedArray.
  const std::vector<Address> signature_tables;
  // Signature maps canonicalize {FunctionSig*} to indexes. New entries can be
  // added to a signature map during graph building.
  // Normally, these signature maps correspond to the signature maps in the
  // function tables stored in the {module}.
  const std::vector<wasm::SignatureMap*> signature_maps;
  // Contains the code objects to call for each indirect call.
  // (the same length as module.functions)
  const std::vector<Handle<Code>> function_code;
  // If the default code is not a null handle, always use it for direct calls.
  const Handle<Code> default_function_code;
  // Address of the start of the globals region.
  const uintptr_t globals_start;
};

enum RuntimeExceptionSupport : bool {
  kRuntimeExceptionSupport = true,
  kNoRuntimeExceptionSupport = false
};

class WasmCompilationUnit final {
 public:
  // If constructing from a background thread, pass in a Counters*, and ensure
  // that the Counters live at least as long as this compilation unit (which
  // typically means to hold a std::shared_ptr<Counters>).
  // If no such pointer is passed, Isolate::counters() will be called. This is
  // only allowed to happen on the foreground thread.
  WasmCompilationUnit(Isolate*, ModuleEnv*, wasm::FunctionBody, wasm::WasmName,
                      int index, Handle<Code> centry_stub, Counters* = nullptr,
                      RuntimeExceptionSupport = kRuntimeExceptionSupport,
                      bool lower_simd = false);

  int func_index() const { return func_index_; }

  void ExecuteCompilation();
  MaybeHandle<Code> FinishCompilation(wasm::ErrorThrower* thrower);

  static MaybeHandle<Code> CompileWasmFunction(
      wasm::ErrorThrower* thrower, Isolate* isolate,
      const wasm::ModuleWireBytes& wire_bytes, ModuleEnv* env,
      const wasm::WasmFunction* function);

  void set_memory_cost(size_t memory_cost) { memory_cost_ = memory_cost; }
  size_t memory_cost() const { return memory_cost_; }

 private:
  SourcePositionTable* BuildGraphForWasmFunction(double* decode_ms);
  Counters* counters() { return counters_; }

  Isolate* isolate_;
  ModuleEnv* env_;
  wasm::FunctionBody func_body_;
  wasm::WasmName func_name_;
  Counters* counters_;
  // The graph zone is deallocated at the end of ExecuteCompilation by virtue of
  // it being zone allocated.
  JSGraph* jsgraph_ = nullptr;
  // the compilation_zone_, info_, and job_ fields need to survive past
  // ExecuteCompilation, onto FinishCompilation (which happens on the main
  // thread).
  std::unique_ptr<Zone> compilation_zone_;
  std::unique_ptr<CompilationInfo> info_;
  std::unique_ptr<CompilationJob> job_;
  Handle<Code> centry_stub_;
  int func_index_;
  wasm::Result<wasm::DecodeStruct*> graph_construction_result_;
  // See WasmGraphBuilder::runtime_exception_support_.
  RuntimeExceptionSupport runtime_exception_support_;
  bool ok_ = true;
  size_t memory_cost_ = 0;
  bool lower_simd_;

  DISALLOW_COPY_AND_ASSIGN(WasmCompilationUnit);
};

// Wraps a JS function, producing a code object that can be called from wasm.
// The global_js_imports_table is a global handle to a fixed array of target
// JSReceiver with the lifetime tied to the module. We store it's location (non
// GCable) in the generated code so that it can reside outside of GCed heap.
Handle<Code> CompileWasmToJSWrapper(Isolate* isolate, Handle<JSReceiver> target,
                                    wasm::FunctionSig* sig, uint32_t index,
                                    wasm::ModuleOrigin origin,
                                    Handle<FixedArray> global_js_imports_table);

// Wraps a given wasm code object, producing a code object.
Handle<Code> CompileJSToWasmWrapper(Isolate* isolate, wasm::WasmModule* module,
                                    Handle<Code> wasm_code, uint32_t index,
                                    Address wasm_context_address);

// Wraps a wasm function, producing a code object that can be called from other
// wasm instances (the WasmContext address must be changed).
Handle<Code> CompileWasmToWasmWrapper(Isolate* isolate, Handle<Code> target,
                                      wasm::FunctionSig* sig, uint32_t index,
                                      Address new_wasm_context_address);

// Compiles a stub that redirects a call to a wasm function to the wasm
// interpreter. It's ABI compatible with the compiled wasm function.
Handle<Code> CompileWasmInterpreterEntry(Isolate* isolate, uint32_t func_index,
                                         wasm::FunctionSig* sig,
                                         Handle<WasmInstanceObject> instance);

enum CWasmEntryParameters {
  kCodeObject,
  kArgumentsBuffer,
  // marker:
  kNumParameters
};

// Compiles a stub with JS linkage, taking parameters as described by
// {CWasmEntryParameters}. It loads the wasm parameters from the argument
// buffer and calls the wasm function given as first parameter.
Handle<Code> CompileCWasmEntry(Isolate* isolate, wasm::FunctionSig* sig,
                               Address wasm_context_address);

// Abstracts details of building TurboFan graph nodes for wasm to separate
// the wasm decoder from the internal details of TurboFan.
typedef ZoneVector<Node*> NodeVector;
class WasmGraphBuilder {
 public:
  WasmGraphBuilder(ModuleEnv*, Zone*, JSGraph*, Handle<Code> centry_stub_,
                   wasm::FunctionSig*, compiler::SourcePositionTable* = nullptr,
                   RuntimeExceptionSupport = kRuntimeExceptionSupport);

  Node** Buffer(size_t count) {
    if (count > cur_bufsize_) {
      size_t new_size = count + cur_bufsize_ + 5;
      cur_buffer_ =
          reinterpret_cast<Node**>(zone_->New(new_size * sizeof(Node*)));
      cur_bufsize_ = new_size;
    }
    return cur_buffer_;
  }

  //-----------------------------------------------------------------------
  // Operations independent of {control} or {effect}.
  //-----------------------------------------------------------------------
  Node* Error();
  Node* Start(unsigned params);
  Node* Param(unsigned index);
  Node* Loop(Node* entry);
  Node* Terminate(Node* effect, Node* control);
  Node* Merge(unsigned count, Node** controls);
  Node* Phi(wasm::ValueType type, unsigned count, Node** vals, Node* control);
  Node* EffectPhi(unsigned count, Node** effects, Node* control);
  Node* NumberConstant(int32_t value);
  Node* Uint32Constant(uint32_t value);
  Node* Int32Constant(int32_t value);
  Node* Int64Constant(int64_t value);
  Node* IntPtrConstant(intptr_t value);
  Node* Float32Constant(float value);
  Node* Float64Constant(double value);
  Node* HeapConstant(Handle<HeapObject> value);
  Node* Binop(wasm::WasmOpcode opcode, Node* left, Node* right,
              wasm::WasmCodePosition position = wasm::kNoCodePosition);
  Node* Unop(wasm::WasmOpcode opcode, Node* input,
             wasm::WasmCodePosition position = wasm::kNoCodePosition);
  Node* GrowMemory(Node* input);
  Node* Throw(uint32_t tag, const wasm::WasmException* exception,
              const Vector<Node*> values);
  Node* Rethrow();
  Node* ConvertExceptionTagToRuntimeId(uint32_t tag);
  Node* GetExceptionRuntimeId();
  Node** GetExceptionValues(const wasm::WasmException* except_decl);
  unsigned InputCount(Node* node);
  bool IsPhiWithMerge(Node* phi, Node* merge);
  bool ThrowsException(Node* node, Node** if_success, Node** if_exception);
  void AppendToMerge(Node* merge, Node* from);
  void AppendToPhi(Node* phi, Node* from);

  void StackCheck(wasm::WasmCodePosition position, Node** effect = nullptr,
                  Node** control = nullptr);

  void PatchInStackCheckIfNeeded();

  //-----------------------------------------------------------------------
  // Operations that read and/or write {control} and {effect}.
  //-----------------------------------------------------------------------
  Node* BranchNoHint(Node* cond, Node** true_node, Node** false_node);
  Node* BranchExpectTrue(Node* cond, Node** true_node, Node** false_node);
  Node* BranchExpectFalse(Node* cond, Node** true_node, Node** false_node);

  Node* TrapIfTrue(wasm::TrapReason reason, Node* cond,
                   wasm::WasmCodePosition position);
  Node* TrapIfFalse(wasm::TrapReason reason, Node* cond,
                    wasm::WasmCodePosition position);
  Node* TrapIfEq32(wasm::TrapReason reason, Node* node, int32_t val,
                   wasm::WasmCodePosition position);
  Node* ZeroCheck32(wasm::TrapReason reason, Node* node,
                    wasm::WasmCodePosition position);
  Node* TrapIfEq64(wasm::TrapReason reason, Node* node, int64_t val,
                   wasm::WasmCodePosition position);
  Node* ZeroCheck64(wasm::TrapReason reason, Node* node,
                    wasm::WasmCodePosition position);

  Node* Switch(unsigned count, Node* key);
  Node* IfValue(int32_t value, Node* sw);
  Node* IfDefault(Node* sw);
  Node* Return(unsigned count, Node** nodes);
  template <typename... Nodes>
  Node* Return(Node* fst, Nodes*... more) {
    Node* arr[] = {fst, more...};
    return Return(arraysize(arr), arr);
  }
  Node* ReturnVoid();
  Node* Unreachable(wasm::WasmCodePosition position);

  Node* CallDirect(uint32_t index, Node** args, Node*** rets,
                   wasm::WasmCodePosition position);
  Node* CallIndirect(uint32_t index, Node** args, Node*** rets,
                     wasm::WasmCodePosition position);

  void BuildJSToWasmWrapper(Handle<Code> wasm_code,
                            Address wasm_context_address);
  enum ImportDataType {
    kFunction = 1,
    kGlobalProxy = 2,
    kFunctionContext = 3,
  };
  Node* LoadImportDataAtOffset(int offset, Node* table);
  Node* LoadNativeContext(Node* table);
  Node* LoadImportData(int index, ImportDataType type, Node* table);
  bool BuildWasmToJSWrapper(Handle<JSReceiver> target,
                            Handle<FixedArray> global_js_imports_table,
                            int index);
  void BuildWasmToWasmWrapper(Handle<Code> target,
                              Address new_wasm_context_address);
  void BuildWasmInterpreterEntry(uint32_t func_index,
                                 Handle<WasmInstanceObject> instance);
  void BuildCWasmEntry(Address wasm_context_address);

  Node* ToJS(Node* node, wasm::ValueType type);
  Node* FromJS(Node* node, Node* js_context, wasm::ValueType type);
  Node* Invert(Node* node);
  void EnsureFunctionTableNodes();

  //-----------------------------------------------------------------------
  // Operations that concern the linear memory.
  //-----------------------------------------------------------------------
  Node* CurrentMemoryPages();
  Node* GetGlobal(uint32_t index);
  Node* SetGlobal(uint32_t index, Node* val);
  Node* TraceMemoryOperation(bool is_store, MachineRepresentation, Node* index,
                             uint32_t offset, wasm::WasmCodePosition);
  Node* LoadMem(wasm::ValueType type, MachineType memtype, Node* index,
                uint32_t offset, uint32_t alignment,
                wasm::WasmCodePosition position);
  Node* StoreMem(MachineType memtype, Node* index, uint32_t offset,
                 uint32_t alignment, Node* val, wasm::WasmCodePosition position,
                 wasm::ValueType type);
  static void PrintDebugName(Node* node);

  void set_wasm_context(Node* wasm_context) {
    this->wasm_context_ = wasm_context;
  }

  Node* Control() { return *control_; }
  Node* Effect() { return *effect_; }

  void set_control_ptr(Node** control) { this->control_ = control; }

  void set_effect_ptr(Node** effect) { this->effect_ = effect; }

  Node* LoadMemSize();
  Node* LoadMemStart();

  void set_mem_size(Node** mem_size) { this->mem_size_ = mem_size; }

  void set_mem_start(Node** mem_start) { this->mem_start_ = mem_start; }

  wasm::FunctionSig* GetFunctionSignature() { return sig_; }

  void LowerInt64();

  void SimdScalarLoweringForTesting();

  void SetSourcePosition(Node* node, wasm::WasmCodePosition position);

  Node* S128Zero();
  Node* S1x4Zero();
  Node* S1x8Zero();
  Node* S1x16Zero();

  Node* SimdOp(wasm::WasmOpcode opcode, Node* const* inputs);

  Node* SimdLaneOp(wasm::WasmOpcode opcode, uint8_t lane, Node* const* inputs);

  Node* SimdShiftOp(wasm::WasmOpcode opcode, uint8_t shift,
                    Node* const* inputs);

  Node* Simd8x16ShuffleOp(const uint8_t shuffle[16], Node* const* inputs);

  Node* AtomicOp(wasm::WasmOpcode opcode, Node* const* inputs,
                 uint32_t alignment, uint32_t offset,
                 wasm::WasmCodePosition position);

  bool has_simd() const { return has_simd_; }

  const wasm::WasmModule* module() { return env_ ? env_->module : nullptr; }

 private:
  static const int kDefaultBufferSize = 16;

  Zone* zone_;
  JSGraph* jsgraph_;
  Node* centry_stub_node_;
  ModuleEnv* env_ = nullptr;
  Node* wasm_context_ = nullptr;
  NodeVector signature_tables_;
  NodeVector function_tables_;
  NodeVector function_table_sizes_;
  Node** control_ = nullptr;
  Node** effect_ = nullptr;
  Node** mem_size_ = nullptr;
  Node** mem_start_ = nullptr;
  Node** cur_buffer_;
  size_t cur_bufsize_;
  Node* def_buffer_[kDefaultBufferSize];
  bool has_simd_ = false;
  bool needs_stack_check_ = false;
  // If the runtime doesn't support exception propagation,
  // we won't generate stack checks, and trap handling will also
  // be generated differently.
  RuntimeExceptionSupport runtime_exception_support_;

  wasm::FunctionSig* sig_;
  SetOncePointer<const Operator> allocate_heap_number_operator_;

  compiler::SourcePositionTable* source_position_table_ = nullptr;

  // Internal helper methods.
  JSGraph* jsgraph() { return jsgraph_; }
  Graph* graph();

  Node* String(const char* string);
  Node* MemBuffer(uint32_t offset);
  void BoundsCheckMem(MachineType memtype, Node* index, uint32_t offset,
                      wasm::WasmCodePosition position);
  const Operator* GetSafeLoadOperator(int offset, wasm::ValueType type);
  const Operator* GetSafeStoreOperator(int offset, wasm::ValueType type);
  Node* BuildChangeEndiannessStore(Node* node, MachineType type,
                                   wasm::ValueType wasmtype = wasm::kWasmStmt);
  Node* BuildChangeEndiannessLoad(Node* node, MachineType type,
                                  wasm::ValueType wasmtype = wasm::kWasmStmt);

  Node* MaskShiftCount32(Node* node);
  Node* MaskShiftCount64(Node* node);

  template <typename... Args>
  Node* BuildCCall(MachineSignature* sig, Node* function, Args... args);
  Node* BuildWasmCall(wasm::FunctionSig* sig, Node** args, Node*** rets,
                      wasm::WasmCodePosition position);

  Node* BuildF32CopySign(Node* left, Node* right);
  Node* BuildF64CopySign(Node* left, Node* right);
  Node* BuildI32SConvertF32(Node* input, wasm::WasmCodePosition position);
  Node* BuildI32SConvertF64(Node* input, wasm::WasmCodePosition position);
  Node* BuildI32UConvertF32(Node* input, wasm::WasmCodePosition position);
  Node* BuildI32UConvertF64(Node* input, wasm::WasmCodePosition position);
  Node* BuildI32Ctz(Node* input);
  Node* BuildI32Popcnt(Node* input);
  Node* BuildI64Ctz(Node* input);
  Node* BuildI64Popcnt(Node* input);
  Node* BuildBitCountingCall(Node* input, ExternalReference ref,
                             MachineRepresentation input_type);

  Node* BuildCFuncInstruction(ExternalReference ref, MachineType type,
                              Node* input0, Node* input1 = nullptr);
  Node* BuildF32Trunc(Node* input);
  Node* BuildF32Floor(Node* input);
  Node* BuildF32Ceil(Node* input);
  Node* BuildF32NearestInt(Node* input);
  Node* BuildF64Trunc(Node* input);
  Node* BuildF64Floor(Node* input);
  Node* BuildF64Ceil(Node* input);
  Node* BuildF64NearestInt(Node* input);
  Node* BuildI32Rol(Node* left, Node* right);
  Node* BuildI64Rol(Node* left, Node* right);

  Node* BuildF64Acos(Node* input);
  Node* BuildF64Asin(Node* input);
  Node* BuildF64Pow(Node* left, Node* right);
  Node* BuildF64Mod(Node* left, Node* right);

  Node* BuildIntToFloatConversionInstruction(
      Node* input, ExternalReference ref,
      MachineRepresentation parameter_representation,
      const MachineType result_type);
  Node* BuildF32SConvertI64(Node* input);
  Node* BuildF32UConvertI64(Node* input);
  Node* BuildF64SConvertI64(Node* input);
  Node* BuildF64UConvertI64(Node* input);

  Node* BuildFloatToIntConversionInstruction(
      Node* input, ExternalReference ref,
      MachineRepresentation parameter_representation,
      const MachineType result_type, wasm::WasmCodePosition position);
  Node* BuildI64SConvertF32(Node* input, wasm::WasmCodePosition position);
  Node* BuildI64UConvertF32(Node* input, wasm::WasmCodePosition position);
  Node* BuildI64SConvertF64(Node* input, wasm::WasmCodePosition position);
  Node* BuildI64UConvertF64(Node* input, wasm::WasmCodePosition position);

  Node* BuildI32DivS(Node* left, Node* right, wasm::WasmCodePosition position);
  Node* BuildI32RemS(Node* left, Node* right, wasm::WasmCodePosition position);
  Node* BuildI32DivU(Node* left, Node* right, wasm::WasmCodePosition position);
  Node* BuildI32RemU(Node* left, Node* right, wasm::WasmCodePosition position);

  Node* BuildI64DivS(Node* left, Node* right, wasm::WasmCodePosition position);
  Node* BuildI64RemS(Node* left, Node* right, wasm::WasmCodePosition position);
  Node* BuildI64DivU(Node* left, Node* right, wasm::WasmCodePosition position);
  Node* BuildI64RemU(Node* left, Node* right, wasm::WasmCodePosition position);
  Node* BuildDiv64Call(Node* left, Node* right, ExternalReference ref,
                       MachineType result_type, int trap_zero,
                       wasm::WasmCodePosition position);

  Node* BuildJavaScriptToNumber(Node* node, Node* js_context);

  Node* BuildChangeInt32ToTagged(Node* value);
  Node* BuildChangeFloat64ToTagged(Node* value);
  Node* BuildChangeTaggedToFloat64(Node* value);

  Node* BuildChangeInt32ToSmi(Node* value);
  Node* BuildChangeSmiToInt32(Node* value);
  Node* BuildChangeUint32ToSmi(Node* value);
  Node* BuildChangeSmiToFloat64(Node* value);
  Node* BuildTestNotSmi(Node* value);
  Node* BuildSmiShiftBitsConstant();

  Node* BuildAllocateHeapNumberWithValue(Node* value, Node* control);
  Node* BuildLoadHeapNumberValue(Node* value, Node* control);
  Node* BuildHeapNumberValueIndexConstant();

  // Asm.js specific functionality.
  Node* BuildI32AsmjsSConvertF32(Node* input);
  Node* BuildI32AsmjsSConvertF64(Node* input);
  Node* BuildI32AsmjsUConvertF32(Node* input);
  Node* BuildI32AsmjsUConvertF64(Node* input);
  Node* BuildI32AsmjsDivS(Node* left, Node* right);
  Node* BuildI32AsmjsRemS(Node* left, Node* right);
  Node* BuildI32AsmjsDivU(Node* left, Node* right);
  Node* BuildI32AsmjsRemU(Node* left, Node* right);
  Node* BuildAsmjsLoadMem(MachineType type, Node* index);
  Node* BuildAsmjsStoreMem(MachineType type, Node* index, Node* val);

  uint32_t GetExceptionEncodedSize(const wasm::WasmException* exception) const;
  void BuildEncodeException32BitValue(uint32_t* index, Node* value);
  Node* BuildDecodeException32BitValue(Node* const* values, uint32_t* index);

  Node** Realloc(Node* const* buffer, size_t old_count, size_t new_count) {
    Node** buf = Buffer(new_count);
    if (buf != buffer) memcpy(buf, buffer, old_count * sizeof(Node*));
    return buf;
  }

  int AddParameterNodes(Node** args, int pos, int param_count,
                        wasm::FunctionSig* sig);

  void SetNeedsStackCheck() { needs_stack_check_ = true; }

  //-----------------------------------------------------------------------
  // Operations involving the CEntryStub, a dependency we want to remove
  // to get off the GC heap.
  //-----------------------------------------------------------------------
  Node* BuildCallToRuntime(Runtime::FunctionId f, Node** parameters,
                           int parameter_count);

  Node* BuildCallToRuntimeWithContext(Runtime::FunctionId f, Node* js_context,
                                      Node** parameters, int parameter_count);
  Node* BuildCallToRuntimeWithContextFromJS(Runtime::FunctionId f,
                                            Node* js_context,
                                            Node* const* parameters,
                                            int parameter_count);
  Node* BuildModifyThreadInWasmFlag(bool new_value);
  Builtins::Name GetBuiltinIdForTrap(wasm::TrapReason reason);
};

// The parameter index where the wasm_context paramter should be placed in wasm
// call descriptors. This is used by the Int64Lowering::LowerNode method.
constexpr int kWasmContextParameterIndex = 0;

V8_EXPORT_PRIVATE CallDescriptor* GetWasmCallDescriptor(Zone* zone,
                                                        wasm::FunctionSig* sig);
V8_EXPORT_PRIVATE CallDescriptor* GetI32WasmCallDescriptor(
    Zone* zone, CallDescriptor* descriptor);
V8_EXPORT_PRIVATE CallDescriptor* GetI32WasmCallDescriptorForSimd(
    Zone* zone, CallDescriptor* descriptor);

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_WASM_COMPILER_H_
