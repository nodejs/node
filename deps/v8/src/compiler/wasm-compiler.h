// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_WASM_COMPILER_H_
#define V8_COMPILER_WASM_COMPILER_H_

#include <memory>

// Clients of this interface shouldn't depend on lots of compiler internals.
// Do not include anything from src/compiler here!
#include "src/runtime/runtime.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/function-compiler.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes.h"
#include "src/wasm/wasm-result.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
struct AssemblerOptions;

namespace compiler {
// Forward declarations for some compiler data structures.
class CallDescriptor;
class Graph;
class MachineGraph;
class Node;
class NodeOriginTable;
class Operator;
class SourcePositionTable;
class WasmDecorator;
enum class TrapId : uint32_t;
}  // namespace compiler

namespace wasm {
struct DecodeStruct;
// Expose {Node} and {Graph} opaquely as {wasm::TFNode} and {wasm::TFGraph}.
typedef compiler::Node TFNode;
typedef compiler::MachineGraph TFGraph;
class WasmCode;
struct WasmFeatures;
}  // namespace wasm

namespace compiler {

class TurbofanWasmCompilationUnit {
 public:
  explicit TurbofanWasmCompilationUnit(wasm::WasmCompilationUnit* wasm_unit);
  ~TurbofanWasmCompilationUnit();

  SourcePositionTable* BuildGraphForWasmFunction(wasm::WasmFeatures* detected,
                                                 double* decode_ms,
                                                 MachineGraph* mcgraph,
                                                 NodeOriginTable* node_origins);

  void ExecuteCompilation(wasm::WasmFeatures* detected);

  wasm::WasmCode* FinishCompilation(wasm::ErrorThrower*);

 private:
  wasm::WasmCompilationUnit* const wasm_unit_;
  bool ok_ = true;
  wasm::WasmCode* wasm_code_ = nullptr;
  wasm::Result<wasm::DecodeStruct*> graph_construction_result_;

  DISALLOW_COPY_AND_ASSIGN(TurbofanWasmCompilationUnit);
};

// Calls to WASM imports are handled in several different ways, depending
// on the type of the target function/callable and whether the signature
// matches the argument arity.
enum class WasmImportCallKind {
  kLinkError,                      // static WASM->WASM type error
  kRuntimeTypeError,               // runtime WASM->JS type error
  kWasmToWasm,                     // fast WASM->WASM call
  kJSFunctionArityMatch,           // fast WASM->JS call
  kJSFunctionArityMatchSloppy,     // fast WASM->JS call, sloppy receiver
  kJSFunctionArityMismatch,        // WASM->JS, needs adapter frame
  kJSFunctionArityMismatchSloppy,  // WASM->JS, needs adapter frame, sloppy
  kUseCallBuiltin                  // everything else
};

WasmImportCallKind GetWasmImportCallKind(Handle<JSReceiver> callable,
                                         wasm::FunctionSig* sig);

// Compiles an import call wrapper, which allows WASM to call imports.
MaybeHandle<Code> CompileWasmImportCallWrapper(Isolate*, WasmImportCallKind,
                                               wasm::FunctionSig*,
                                               uint32_t index,
                                               wasm::ModuleOrigin,
                                               wasm::UseTrapHandler);

// Creates a code object calling a wasm function with the given signature,
// callable from JS.
V8_EXPORT_PRIVATE MaybeHandle<Code> CompileJSToWasmWrapper(Isolate*,
                                                           wasm::FunctionSig*,
                                                           bool is_import);

// Compiles a stub that redirects a call to a wasm function to the wasm
// interpreter. It's ABI compatible with the compiled wasm function.
MaybeHandle<Code> CompileWasmInterpreterEntry(Isolate*, uint32_t func_index,
                                              wasm::FunctionSig*);

enum CWasmEntryParameters {
  kCodeEntry,
  kWasmInstance,
  kArgumentsBuffer,
  // marker:
  kNumParameters
};

// Compiles a stub with JS linkage, taking parameters as described by
// {CWasmEntryParameters}. It loads the wasm parameters from the argument
// buffer and calls the wasm function given as first parameter.
MaybeHandle<Code> CompileCWasmEntry(Isolate* isolate, wasm::FunctionSig* sig);

// Values from the instance object are cached between WASM-level function calls.
// This struct allows the SSA environment handling this cache to be defined
// and manipulated in wasm-compiler.{h,cc} instead of inside the WASM decoder.
// (Note that currently, the globals base is immutable, so not cached here.)
struct WasmInstanceCacheNodes {
  Node* mem_start;
  Node* mem_size;
  Node* mem_mask;
};

// Abstracts details of building TurboFan graph nodes for wasm to separate
// the wasm decoder from the internal details of TurboFan.
class WasmGraphBuilder {
 public:
  enum EnforceBoundsCheck : bool {
    kNeedsBoundsCheck = true,
    kCanOmitBoundsCheck = false
  };
  enum UseRetpoline : bool { kRetpoline = true, kNoRetpoline = false };

  WasmGraphBuilder(wasm::ModuleEnv* env, Zone* zone, MachineGraph* mcgraph,
                   wasm::FunctionSig* sig,
                   compiler::SourcePositionTable* spt = nullptr);

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
  Node* CreateOrMergeIntoPhi(MachineRepresentation rep, Node* merge,
                             Node* tnode, Node* fnode);
  Node* CreateOrMergeIntoEffectPhi(Node* merge, Node* tnode, Node* fnode);
  Node* EffectPhi(unsigned count, Node** effects, Node* control);
  Node* RefNull();
  Node* Uint32Constant(uint32_t value);
  Node* Int32Constant(int32_t value);
  Node* Int64Constant(int64_t value);
  Node* IntPtrConstant(intptr_t value);
  Node* Float32Constant(float value);
  Node* Float64Constant(double value);
  Node* Binop(wasm::WasmOpcode opcode, Node* left, Node* right,
              wasm::WasmCodePosition position = wasm::kNoCodePosition);
  Node* Unop(wasm::WasmOpcode opcode, Node* input,
             wasm::WasmCodePosition position = wasm::kNoCodePosition);
  Node* GrowMemory(Node* input);
  Node* Throw(uint32_t exception_index, const wasm::WasmException* exception,
              const Vector<Node*> values);
  Node* Rethrow(Node* except_obj);
  Node* ExceptionTagEqual(Node* caught_tag, Node* expected_tag);
  Node* LoadExceptionTagFromTable(uint32_t exception_index);
  Node* GetExceptionTag(Node* except_obj);
  Node** GetExceptionValues(Node* except_obj,
                            const wasm::WasmException* except_decl);
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

  Node* Invert(Node* node);

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
  Node* StoreMem(MachineRepresentation mem_rep, Node* index, uint32_t offset,
                 uint32_t alignment, Node* val, wasm::WasmCodePosition position,
                 wasm::ValueType type);
  static void PrintDebugName(Node* node);

  void set_instance_node(Node* instance_node) {
    this->instance_node_ = instance_node;
  }

  Node* Control() {
    DCHECK_NOT_NULL(*control_);
    return *control_;
  }
  Node* Effect() {
    DCHECK_NOT_NULL(*effect_);
    return *effect_;
  }
  Node* SetControl(Node* node) {
    *control_ = node;
    return node;
  }
  Node* SetEffect(Node* node) {
    *effect_ = node;
    return node;
  }

  void set_control_ptr(Node** control) { this->control_ = control; }

  void set_effect_ptr(Node** effect) { this->effect_ = effect; }

  void GetGlobalBaseAndOffset(MachineType mem_type, const wasm::WasmGlobal&,
                              Node** base_node, Node** offset_node);

  // Utilities to manipulate sets of instance cache nodes.
  void InitInstanceCache(WasmInstanceCacheNodes* instance_cache);
  void PrepareInstanceCacheForLoop(WasmInstanceCacheNodes* instance_cache,
                                   Node* control);
  void NewInstanceCacheMerge(WasmInstanceCacheNodes* to,
                             WasmInstanceCacheNodes* from, Node* merge);
  void MergeInstanceCacheInto(WasmInstanceCacheNodes* to,
                              WasmInstanceCacheNodes* from, Node* merge);

  void set_instance_cache(WasmInstanceCacheNodes* instance_cache) {
    this->instance_cache_ = instance_cache;
  }

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

  bool use_trap_handler() const { return env_ && env_->use_trap_handler; }

  MachineGraph* mcgraph() { return mcgraph_; }
  Graph* graph();

  void AddBytecodePositionDecorator(NodeOriginTable* node_origins,
                                    wasm::Decoder* decoder);

  void RemoveBytecodePositionDecorator();

 protected:
  static const int kDefaultBufferSize = 16;

  Zone* const zone_;
  MachineGraph* const mcgraph_;
  wasm::ModuleEnv* const env_;

  Node** control_ = nullptr;
  Node** effect_ = nullptr;
  WasmInstanceCacheNodes* instance_cache_ = nullptr;

  SetOncePointer<Node> instance_node_;
  SetOncePointer<Node> globals_start_;
  SetOncePointer<Node> imported_mutable_globals_;
  SetOncePointer<Node> stack_check_code_node_;
  SetOncePointer<const Operator> stack_check_call_operator_;

  Node** cur_buffer_;
  size_t cur_bufsize_;
  Node* def_buffer_[kDefaultBufferSize];
  bool has_simd_ = false;
  bool needs_stack_check_ = false;
  const bool untrusted_code_mitigations_ = true;

  wasm::FunctionSig* const sig_;

  compiler::WasmDecorator* decorator_ = nullptr;

  compiler::SourcePositionTable* const source_position_table_ = nullptr;

  Node* NoContextConstant();

  Node* MemBuffer(uint32_t offset);
  // BoundsCheckMem receives a uint32 {index} node and returns a ptrsize index.
  Node* BoundsCheckMem(uint8_t access_size, Node* index, uint32_t offset,
                       wasm::WasmCodePosition, EnforceBoundsCheck);
  Node* CheckBoundsAndAlignment(uint8_t access_size, Node* index,
                                uint32_t offset, wasm::WasmCodePosition);
  Node* Uint32ToUintptr(Node*);
  const Operator* GetSafeLoadOperator(int offset, wasm::ValueType type);
  const Operator* GetSafeStoreOperator(int offset, wasm::ValueType type);
  Node* BuildChangeEndiannessStore(Node* node, MachineRepresentation rep,
                                   wasm::ValueType wasmtype = wasm::kWasmStmt);
  Node* BuildChangeEndiannessLoad(Node* node, MachineType type,
                                  wasm::ValueType wasmtype = wasm::kWasmStmt);

  Node* MaskShiftCount32(Node* node);
  Node* MaskShiftCount64(Node* node);

  template <typename... Args>
  Node* BuildCCall(MachineSignature* sig, Node* function, Args... args);
  Node* BuildWasmCall(wasm::FunctionSig* sig, Node** args, Node*** rets,
                      wasm::WasmCodePosition position, Node* instance_node,
                      UseRetpoline use_retpoline);
  Node* BuildImportCall(wasm::FunctionSig* sig, Node** args, Node*** rets,
                        wasm::WasmCodePosition position, int func_index);
  Node* BuildImportCall(wasm::FunctionSig* sig, Node** args, Node*** rets,
                        wasm::WasmCodePosition position, Node* func_index);

  Node* BuildF32CopySign(Node* left, Node* right);
  Node* BuildF64CopySign(Node* left, Node* right);

  Node* BuildIntConvertFloat(Node* input, wasm::WasmCodePosition position,
                             wasm::WasmOpcode);
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

  Node* BuildCcallConvertFloat(Node* input, wasm::WasmCodePosition position,
                               wasm::WasmOpcode opcode);

  Node* BuildI32DivS(Node* left, Node* right, wasm::WasmCodePosition position);
  Node* BuildI32RemS(Node* left, Node* right, wasm::WasmCodePosition position);
  Node* BuildI32DivU(Node* left, Node* right, wasm::WasmCodePosition position);
  Node* BuildI32RemU(Node* left, Node* right, wasm::WasmCodePosition position);

  Node* BuildI64DivS(Node* left, Node* right, wasm::WasmCodePosition position);
  Node* BuildI64RemS(Node* left, Node* right, wasm::WasmCodePosition position);
  Node* BuildI64DivU(Node* left, Node* right, wasm::WasmCodePosition position);
  Node* BuildI64RemU(Node* left, Node* right, wasm::WasmCodePosition position);
  Node* BuildDiv64Call(Node* left, Node* right, ExternalReference ref,
                       MachineType result_type, wasm::TrapReason trap_zero,
                       wasm::WasmCodePosition position);

  Node* BuildChangeInt32ToIntPtr(Node* value);
  Node* BuildChangeInt32ToSmi(Node* value);
  Node* BuildChangeUint31ToSmi(Node* value);
  Node* BuildSmiShiftBitsConstant();
  Node* BuildChangeSmiToInt32(Node* value);

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
  void BuildEncodeException32BitValue(Node* except_obj, uint32_t* index,
                                      Node* value);
  Node* BuildDecodeException32BitValue(Node* const* values, uint32_t* index);

  Node** Realloc(Node* const* buffer, size_t old_count, size_t new_count) {
    Node** buf = Buffer(new_count);
    if (buf != buffer) memcpy(buf, buffer, old_count * sizeof(Node*));
    return buf;
  }

  Node* BuildLoadBuiltinFromInstance(int builtin_index);

  //-----------------------------------------------------------------------
  // Operations involving the CEntry, a dependency we want to remove
  // to get off the GC heap.
  //-----------------------------------------------------------------------
  Node* BuildCallToRuntime(Runtime::FunctionId f, Node** parameters,
                           int parameter_count);

  Node* BuildCallToRuntimeWithContext(Runtime::FunctionId f, Node* js_context,
                                      Node** parameters, int parameter_count);
  TrapId GetTrapIdForTrap(wasm::TrapReason reason);
};

V8_EXPORT_PRIVATE CallDescriptor* GetWasmCallDescriptor(
    Zone* zone, wasm::FunctionSig* signature,
    WasmGraphBuilder::UseRetpoline use_retpoline =
        WasmGraphBuilder::kNoRetpoline);

V8_EXPORT_PRIVATE CallDescriptor* GetI32WasmCallDescriptor(
    Zone* zone, CallDescriptor* call_descriptor);

V8_EXPORT_PRIVATE CallDescriptor* GetI32WasmCallDescriptorForSimd(
    Zone* zone, CallDescriptor* call_descriptor);

AssemblerOptions WasmAssemblerOptions();

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_WASM_COMPILER_H_
