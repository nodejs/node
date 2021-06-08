// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_COMPILER_WASM_COMPILER_H_
#define V8_COMPILER_WASM_COMPILER_H_

#include <memory>
#include <utility>

// Clients of this interface shouldn't depend on lots of compiler internals.
// Do not include anything from src/compiler here!
#include "src/base/small-vector.h"
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
class OptimizedCompilationJob;

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
class WasmGraphAssembler;
enum class TrapId : uint32_t;
struct Int64LoweringSpecialCase;
template <size_t VarCount>
class GraphAssemblerLabel;
enum class BranchHint : uint8_t;
}  // namespace compiler

namespace wasm {
struct DecodeStruct;
// Expose {Node} and {Graph} opaquely as {wasm::TFNode} and {wasm::TFGraph}.
using TFNode = compiler::Node;
using TFGraph = compiler::MachineGraph;
class WasmCode;
class WasmFeatures;
enum class LoadTransformationKind : uint8_t;
}  // namespace wasm

namespace compiler {

wasm::WasmCompilationResult ExecuteTurbofanWasmCompilation(
    wasm::WasmEngine*, wasm::CompilationEnv*, const wasm::FunctionBody&,
    int func_index, Counters*, wasm::WasmFeatures* detected);

// Calls to Wasm imports are handled in several different ways, depending on the
// type of the target function/callable and whether the signature matches the
// argument arity.
enum class WasmImportCallKind : uint8_t {
  kLinkError,                // static Wasm->Wasm type error
  kRuntimeTypeError,         // runtime Wasm->JS type error
  kWasmToCapi,               // fast Wasm->C-API call
  kWasmToWasm,               // fast Wasm->Wasm call
  kJSFunctionArityMatch,     // fast Wasm->JS call
  kJSFunctionArityMismatch,  // Wasm->JS, needs adapter frame
  // Math functions imported from JavaScript that are intrinsified
  kFirstMathIntrinsic,
  kF64Acos = kFirstMathIntrinsic,
  kF64Asin,
  kF64Atan,
  kF64Cos,
  kF64Sin,
  kF64Tan,
  kF64Exp,
  kF64Log,
  kF64Atan2,
  kF64Pow,
  kF64Ceil,
  kF64Floor,
  kF64Sqrt,
  kF64Min,
  kF64Max,
  kF64Abs,
  kF32Min,
  kF32Max,
  kF32Abs,
  kF32Ceil,
  kF32Floor,
  kF32Sqrt,
  kF32ConvertF64,
  kLastMathIntrinsic = kF32ConvertF64,
  // For everything else, there's the call builtin.
  kUseCallBuiltin
};

constexpr WasmImportCallKind kDefaultImportCallKind =
    WasmImportCallKind::kJSFunctionArityMatch;

// Resolves which import call wrapper is required for the given JS callable.
// Returns the kind of wrapper need and the ultimate target callable. Note that
// some callables (e.g. a {WasmExportedFunction} or {WasmJSFunction}) just wrap
// another target, which is why the ultimate target is returned as well.
V8_EXPORT_PRIVATE std::pair<WasmImportCallKind, Handle<JSReceiver>>
ResolveWasmImportCall(Handle<JSReceiver> callable, const wasm::FunctionSig* sig,
                      const wasm::WasmModule* module,
                      const wasm::WasmFeatures& enabled_features);

// Compiles an import call wrapper, which allows Wasm to call imports.
V8_EXPORT_PRIVATE wasm::WasmCompilationResult CompileWasmImportCallWrapper(
    wasm::WasmEngine*, wasm::CompilationEnv* env, WasmImportCallKind,
    const wasm::FunctionSig*, bool source_positions, int expected_arity);

// Compiles a host call wrapper, which allows Wasm to call host functions.
wasm::WasmCode* CompileWasmCapiCallWrapper(wasm::WasmEngine*,
                                           wasm::NativeModule*,
                                           const wasm::FunctionSig*,
                                           Address address);

// Returns an OptimizedCompilationJob object for a JS to Wasm wrapper.
std::unique_ptr<OptimizedCompilationJob> NewJSToWasmCompilationJob(
    Isolate* isolate, wasm::WasmEngine* wasm_engine,
    const wasm::FunctionSig* sig, const wasm::WasmModule* module,
    bool is_import, const wasm::WasmFeatures& enabled_features);

MaybeHandle<Code> CompileWasmToJSWrapper(Isolate* isolate,
                                         const wasm::FunctionSig* sig,
                                         WasmImportCallKind kind,
                                         int expected_arity);

// Compiles a stub with JS linkage that serves as an adapter for function
// objects constructed via {WebAssembly.Function}. It performs a round-trip
// simulating a JS-to-Wasm-to-JS coercion of parameter and return values.
MaybeHandle<Code> CompileJSToJSWrapper(Isolate*, const wasm::FunctionSig*,
                                       const wasm::WasmModule* module);

enum CWasmEntryParameters {
  kCodeEntry,
  kObjectRef,
  kArgumentsBuffer,
  kCEntryFp,
  // marker:
  kNumParameters
};

// Compiles a stub with C++ linkage, to be called from Execution::CallWasm,
// which knows how to feed it its parameters.
V8_EXPORT_PRIVATE Handle<Code> CompileCWasmEntry(
    Isolate*, const wasm::FunctionSig*, const wasm::WasmModule* module);

class JSWasmCallData {
 public:
  explicit JSWasmCallData(const wasm::FunctionSig* wasm_signature);

  bool arg_needs_conversion(size_t index) const {
    DCHECK_LT(index, arg_needs_conversion_.size());
    return arg_needs_conversion_[index];
  }
  bool result_needs_conversion() const { return result_needs_conversion_; }

 private:
  bool result_needs_conversion_;
  std::vector<bool> arg_needs_conversion_;
};

// Values from the instance object are cached between Wasm-level function calls.
// This struct allows the SSA environment handling this cache to be defined
// and manipulated in wasm-compiler.{h,cc} instead of inside the Wasm decoder.
// (Note that currently, the globals base is immutable, so not cached here.)
struct WasmInstanceCacheNodes {
  Node* mem_start;
  Node* mem_size;
  Node* mem_mask;
};

struct WasmLoopInfo {
  Node* header;
  uint32_t nesting_depth;
  bool is_innermost;

  WasmLoopInfo(Node* header, uint32_t nesting_depth, bool is_innermost)
      : header(header),
        nesting_depth(nesting_depth),
        is_innermost(is_innermost) {}
};

// Abstracts details of building TurboFan graph nodes for wasm to separate
// the wasm decoder from the internal details of TurboFan.
class WasmGraphBuilder {
 public:
  enum ReferenceKind : bool {  // --
    kArrayOrStruct = true,
    kFunction = false
  };
  struct ObjectReferenceKnowledge {
    bool object_can_be_null;
    ReferenceKind reference_kind;
    int8_t rtt_depth;
  };
  enum EnforceBoundsCheck : bool {  // --
    kNeedsBoundsCheck = true,
    kCanOmitBoundsCheck = false
  };
  enum UseRetpoline : bool {  // --
    kRetpoline = true,
    kNoRetpoline = false
  };
  enum CheckForNull : bool {  // --
    kWithNullCheck = true,
    kWithoutNullCheck = false
  };

  V8_EXPORT_PRIVATE WasmGraphBuilder(
      wasm::CompilationEnv* env, Zone* zone, MachineGraph* mcgraph,
      const wasm::FunctionSig* sig,
      compiler::SourcePositionTable* spt = nullptr)
      : WasmGraphBuilder(env, zone, mcgraph, sig, spt, nullptr) {}

  V8_EXPORT_PRIVATE ~WasmGraphBuilder();

  //-----------------------------------------------------------------------
  // Operations independent of {control} or {effect}.
  //-----------------------------------------------------------------------
  void Start(unsigned params);
  Node* Param(int index, const char* debug_name = nullptr);
  Node* Loop(Node* entry);
  void TerminateLoop(Node* effect, Node* control);
  Node* LoopExit(Node* loop_node);
  // Assumes current control() is the corresponding loop exit.
  Node* LoopExitValue(Node* value, MachineRepresentation representation);
  void TerminateThrow(Node* effect, Node* control);
  Node* Merge(unsigned count, Node** controls);
  template <typename... Nodes>
  Node* Merge(Node* fst, Nodes*... args);
  Node* Phi(wasm::ValueType type, unsigned count, Node** vals_and_control);
  Node* CreateOrMergeIntoPhi(MachineRepresentation rep, Node* merge,
                             Node* tnode, Node* fnode);
  Node* CreateOrMergeIntoEffectPhi(Node* merge, Node* tnode, Node* fnode);
  Node* EffectPhi(unsigned count, Node** effects_and_control);
  Node* RefNull();
  Node* RefFunc(uint32_t function_index);
  Node* RefAsNonNull(Node* arg, wasm::WasmCodePosition position);
  Node* Int32Constant(int32_t value);
  Node* Int64Constant(int64_t value);
  Node* Float32Constant(float value);
  Node* Float64Constant(double value);
  Node* Simd128Constant(const uint8_t value[16]);
  Node* Binop(wasm::WasmOpcode opcode, Node* left, Node* right,
              wasm::WasmCodePosition position = wasm::kNoCodePosition);
  Node* Unop(wasm::WasmOpcode opcode, Node* input,
             wasm::WasmCodePosition position = wasm::kNoCodePosition);
  Node* MemoryGrow(Node* input);
  Node* Throw(uint32_t exception_index, const wasm::WasmException* exception,
              const Vector<Node*> values, wasm::WasmCodePosition position);
  Node* Rethrow(Node* except_obj);
  Node* ExceptionTagEqual(Node* caught_tag, Node* expected_tag);
  Node* LoadExceptionTagFromTable(uint32_t exception_index);
  Node* GetExceptionTag(Node* except_obj);
  Node* GetExceptionValues(Node* except_obj,
                           const wasm::WasmException* exception,
                           Vector<Node*> values_out);
  bool IsPhiWithMerge(Node* phi, Node* merge);
  bool ThrowsException(Node* node, Node** if_success, Node** if_exception);
  void AppendToMerge(Node* merge, Node* from);
  void AppendToPhi(Node* phi, Node* from);

  void StackCheck(wasm::WasmCodePosition);

  void PatchInStackCheckIfNeeded();

  //-----------------------------------------------------------------------
  // Operations that read and/or write {control} and {effect}.
  //-----------------------------------------------------------------------
  Node* BranchNoHint(Node* cond, Node** true_node, Node** false_node);
  Node* BranchExpectFalse(Node* cond, Node** true_node, Node** false_node);

  void TrapIfTrue(wasm::TrapReason reason, Node* cond,
                  wasm::WasmCodePosition position);
  void TrapIfFalse(wasm::TrapReason reason, Node* cond,
                   wasm::WasmCodePosition position);
  Node* Select(Node *cond, Node* true_node, Node* false_node,
               wasm::ValueType type);

  void TrapIfEq32(wasm::TrapReason reason, Node* node, int32_t val,
                  wasm::WasmCodePosition position);
  void ZeroCheck32(wasm::TrapReason reason, Node* node,
                   wasm::WasmCodePosition position);
  void TrapIfEq64(wasm::TrapReason reason, Node* node, int64_t val,
                  wasm::WasmCodePosition position);
  void ZeroCheck64(wasm::TrapReason reason, Node* node,
                   wasm::WasmCodePosition position);

  Node* Switch(unsigned count, Node* key);
  Node* IfValue(int32_t value, Node* sw);
  Node* IfDefault(Node* sw);
  Node* Return(Vector<Node*> nodes);
  template <typename... Nodes>
  Node* Return(Node* fst, Nodes*... more) {
    Node* arr[] = {fst, more...};
    return Return(ArrayVector(arr));
  }

  void TraceFunctionEntry(wasm::WasmCodePosition position);
  void TraceFunctionExit(Vector<Node*> vals, wasm::WasmCodePosition position);

  void Trap(wasm::TrapReason reason, wasm::WasmCodePosition position);

  Node* CallDirect(uint32_t index, Vector<Node*> args, Vector<Node*> rets,
                   wasm::WasmCodePosition position);
  Node* CallIndirect(uint32_t table_index, uint32_t sig_index,
                     Vector<Node*> args, Vector<Node*> rets,
                     wasm::WasmCodePosition position);
  Node* CallRef(uint32_t sig_index, Vector<Node*> args, Vector<Node*> rets,
                CheckForNull null_check, wasm::WasmCodePosition position);

  Node* ReturnCall(uint32_t index, Vector<Node*> args,
                   wasm::WasmCodePosition position);
  Node* ReturnCallIndirect(uint32_t table_index, uint32_t sig_index,
                           Vector<Node*> args, wasm::WasmCodePosition position);
  Node* ReturnCallRef(uint32_t sig_index, Vector<Node*> args,
                      CheckForNull null_check, wasm::WasmCodePosition position);

  void BrOnNull(Node* ref_object, Node** non_null_node, Node** null_node);

  Node* Invert(Node* node);

  Node* GlobalGet(uint32_t index);
  void GlobalSet(uint32_t index, Node* val);
  Node* TableGet(uint32_t table_index, Node* index,
                 wasm::WasmCodePosition position);
  void TableSet(uint32_t table_index, Node* index, Node* val,
                wasm::WasmCodePosition position);
  //-----------------------------------------------------------------------
  // Operations that concern the linear memory.
  //-----------------------------------------------------------------------
  Node* CurrentMemoryPages();
  void TraceMemoryOperation(bool is_store, MachineRepresentation, Node* index,
                            uintptr_t offset, wasm::WasmCodePosition);
  Node* LoadMem(wasm::ValueType type, MachineType memtype, Node* index,
                uint64_t offset, uint32_t alignment,
                wasm::WasmCodePosition position);
#if defined(V8_TARGET_BIG_ENDIAN) || defined(V8_TARGET_ARCH_S390_LE_SIM)
  Node* LoadTransformBigEndian(wasm::ValueType type, MachineType memtype,
                               wasm::LoadTransformationKind transform,
                               Node* index, uint64_t offset, uint32_t alignment,
                               wasm::WasmCodePosition position);
#endif
  Node* LoadTransform(wasm::ValueType type, MachineType memtype,
                      wasm::LoadTransformationKind transform, Node* index,
                      uint64_t offset, uint32_t alignment,
                      wasm::WasmCodePosition position);
  Node* LoadLane(wasm::ValueType type, MachineType memtype, Node* value,
                 Node* index, uint64_t offset, uint32_t alignment,
                 uint8_t laneidx, wasm::WasmCodePosition position);
  void StoreMem(MachineRepresentation mem_rep, Node* index, uint64_t offset,
                uint32_t alignment, Node* val, wasm::WasmCodePosition position,
                wasm::ValueType type);
  void StoreLane(MachineRepresentation mem_rep, Node* index, uint64_t offset,
                 uint32_t alignment, Node* val, uint8_t laneidx,
                 wasm::WasmCodePosition position, wasm::ValueType type);
  static void PrintDebugName(Node* node);

  Node* effect();
  Node* control();
  Node* SetEffect(Node* node);
  Node* SetControl(Node* node);
  void SetEffectControl(Node* effect, Node* control);
  Node* SetEffectControl(Node* effect_and_control) {
    SetEffectControl(effect_and_control, effect_and_control);
    return effect_and_control;
  }

  Node* GetImportedMutableGlobals();

  void GetGlobalBaseAndOffset(MachineType mem_type, const wasm::WasmGlobal&,
                              Node** base_node, Node** offset_node);

  void GetBaseAndOffsetForImportedMutableExternRefGlobal(
      const wasm::WasmGlobal& global, Node** base, Node** offset);

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

  const wasm::FunctionSig* GetFunctionSignature() { return sig_; }

  enum CallOrigin { kCalledFromWasm, kCalledFromJS };

  // Overload for when we want to provide a specific signature, rather than
  // build one using sig_, for example after scalar lowering.
  V8_EXPORT_PRIVATE void LowerInt64(Signature<MachineRepresentation>* sig);
  V8_EXPORT_PRIVATE void LowerInt64(CallOrigin origin);

  void SetSourcePosition(Node* node, wasm::WasmCodePosition position);

  Node* S128Zero();
  Node* S1x4Zero();
  Node* S1x8Zero();
  Node* S1x16Zero();

  Node* SimdOp(wasm::WasmOpcode opcode, Node* const* inputs);

  Node* SimdLaneOp(wasm::WasmOpcode opcode, uint8_t lane, Node* const* inputs);

  Node* Simd8x16ShuffleOp(const uint8_t shuffle[16], Node* const* inputs);

  Node* AtomicOp(wasm::WasmOpcode opcode, Node* const* inputs,
                 uint32_t alignment, uint64_t offset,
                 wasm::WasmCodePosition position);
  void AtomicFence();

  void MemoryInit(uint32_t data_segment_index, Node* dst, Node* src, Node* size,
                  wasm::WasmCodePosition position);
  void MemoryCopy(Node* dst, Node* src, Node* size,
                  wasm::WasmCodePosition position);
  void DataDrop(uint32_t data_segment_index, wasm::WasmCodePosition position);
  void MemoryFill(Node* dst, Node* fill, Node* size,
                  wasm::WasmCodePosition position);

  void TableInit(uint32_t table_index, uint32_t elem_segment_index, Node* dst,
                 Node* src, Node* size, wasm::WasmCodePosition position);
  void ElemDrop(uint32_t elem_segment_index, wasm::WasmCodePosition position);
  void TableCopy(uint32_t table_dst_index, uint32_t table_src_index, Node* dst,
                 Node* src, Node* size, wasm::WasmCodePosition position);
  Node* TableGrow(uint32_t table_index, Node* value, Node* delta);
  Node* TableSize(uint32_t table_index);
  void TableFill(uint32_t table_index, Node* start, Node* value, Node* count);

  Node* StructNewWithRtt(uint32_t struct_index, const wasm::StructType* type,
                         Node* rtt, Vector<Node*> fields);
  Node* StructGet(Node* struct_object, const wasm::StructType* struct_type,
                  uint32_t field_index, CheckForNull null_check, bool is_signed,
                  wasm::WasmCodePosition position);
  void StructSet(Node* struct_object, const wasm::StructType* struct_type,
                 uint32_t field_index, Node* value, CheckForNull null_check,
                 wasm::WasmCodePosition position);
  Node* ArrayNewWithRtt(uint32_t array_index, const wasm::ArrayType* type,
                        Node* length, Node* initial_value, Node* rtt,
                        wasm::WasmCodePosition position);
  void BoundsCheck(Node* array, Node* index, wasm::WasmCodePosition position);
  Node* ArrayGet(Node* array_object, const wasm::ArrayType* type, Node* index,
                 CheckForNull null_check, bool is_signed,
                 wasm::WasmCodePosition position);
  void ArraySet(Node* array_object, const wasm::ArrayType* type, Node* index,
                Node* value, CheckForNull null_check,
                wasm::WasmCodePosition position);
  Node* ArrayLen(Node* array_object, CheckForNull null_check,
                 wasm::WasmCodePosition position);
  Node* I31New(Node* input);
  Node* I31GetS(Node* input);
  Node* I31GetU(Node* input);
  Node* RttCanon(uint32_t type_index);
  Node* RttSub(uint32_t type_index, Node* parent_rtt);

  Node* RefTest(Node* object, Node* rtt, ObjectReferenceKnowledge config);
  Node* RefCast(Node* object, Node* rtt, ObjectReferenceKnowledge config,
                wasm::WasmCodePosition position);
  void BrOnCast(Node* object, Node* rtt, ObjectReferenceKnowledge config,
                Node** match_control, Node** match_effect,
                Node** no_match_control, Node** no_match_effect);
  Node* RefIsData(Node* object, bool object_can_be_null);
  Node* RefAsData(Node* object, bool object_can_be_null,
                  wasm::WasmCodePosition position);
  void BrOnData(Node* object, Node* rtt, ObjectReferenceKnowledge config,
                Node** match_control, Node** match_effect,
                Node** no_match_control, Node** no_match_effect);
  Node* RefIsFunc(Node* object, bool object_can_be_null);
  Node* RefAsFunc(Node* object, bool object_can_be_null,
                  wasm::WasmCodePosition position);
  void BrOnFunc(Node* object, Node* rtt, ObjectReferenceKnowledge config,
                Node** match_control, Node** match_effect,
                Node** no_match_control, Node** no_match_effect);
  Node* RefIsI31(Node* object);
  Node* RefAsI31(Node* object, wasm::WasmCodePosition position);
  void BrOnI31(Node* object, Node* rtt, ObjectReferenceKnowledge config,
               Node** match_control, Node** match_effect,
               Node** no_match_control, Node** no_match_effect);

  bool has_simd() const { return has_simd_; }

  wasm::UseTrapHandler use_trap_handler() const {
    return env_ ? env_->use_trap_handler : wasm::kNoTrapHandler;
  }

  MachineGraph* mcgraph() { return mcgraph_; }
  Graph* graph();

  void AddBytecodePositionDecorator(NodeOriginTable* node_origins,
                                    wasm::Decoder* decoder);

  void RemoveBytecodePositionDecorator();

 protected:
  V8_EXPORT_PRIVATE WasmGraphBuilder(wasm::CompilationEnv* env, Zone* zone,
                                     MachineGraph* mcgraph,
                                     const wasm::FunctionSig* sig,
                                     compiler::SourcePositionTable* spt,
                                     Isolate* isolate);

  Node* NoContextConstant();

  Node* GetInstance();
  Node* BuildLoadIsolateRoot();

  // MemBuffer is only called with valid offsets (after bounds checking), so the
  // offset fits in a platform-dependent uintptr_t.
  Node* MemBuffer(uintptr_t offset);

  // BoundsCheckMem receives a uint32 {index} node and returns a ptrsize index.
  Node* BoundsCheckMem(uint8_t access_size, Node* index, uint64_t offset,
                       wasm::WasmCodePosition, EnforceBoundsCheck);

  Node* CheckBoundsAndAlignment(int8_t access_size, Node* index,
                                uint64_t offset, wasm::WasmCodePosition);

  Node* Uint32ToUintptr(Node*);
  const Operator* GetSafeLoadOperator(int offset, wasm::ValueType type);
  const Operator* GetSafeStoreOperator(int offset, wasm::ValueType type);
  Node* BuildChangeEndiannessStore(Node* node, MachineRepresentation rep,
                                   wasm::ValueType wasmtype = wasm::kWasmVoid);
  Node* BuildChangeEndiannessLoad(Node* node, MachineType type,
                                  wasm::ValueType wasmtype = wasm::kWasmVoid);

  Node* MaskShiftCount32(Node* node);
  Node* MaskShiftCount64(Node* node);

  enum IsReturnCall : bool { kReturnCall = true, kCallContinues = false };

  template <typename... Args>
  Node* BuildCCall(MachineSignature* sig, Node* function, Args... args);
  Node* BuildCallNode(const wasm::FunctionSig* sig, Vector<Node*> args,
                      wasm::WasmCodePosition position, Node* instance_node,
                      const Operator* op, Node* frame_state = nullptr);
  // Helper function for {BuildIndirectCall}.
  void LoadIndirectFunctionTable(uint32_t table_index, Node** ift_size,
                                 Node** ift_sig_ids, Node** ift_targets,
                                 Node** ift_instances);
  Node* BuildIndirectCall(uint32_t table_index, uint32_t sig_index,
                          Vector<Node*> args, Vector<Node*> rets,
                          wasm::WasmCodePosition position,
                          IsReturnCall continuation);
  Node* BuildWasmCall(const wasm::FunctionSig* sig, Vector<Node*> args,
                      Vector<Node*> rets, wasm::WasmCodePosition position,
                      Node* instance_node, UseRetpoline use_retpoline,
                      Node* frame_state = nullptr);
  Node* BuildWasmReturnCall(const wasm::FunctionSig* sig, Vector<Node*> args,
                            wasm::WasmCodePosition position,
                            Node* instance_node, UseRetpoline use_retpoline);
  Node* BuildImportCall(const wasm::FunctionSig* sig, Vector<Node*> args,
                        Vector<Node*> rets, wasm::WasmCodePosition position,
                        int func_index, IsReturnCall continuation);
  Node* BuildImportCall(const wasm::FunctionSig* sig, Vector<Node*> args,
                        Vector<Node*> rets, wasm::WasmCodePosition position,
                        Node* func_index, IsReturnCall continuation);
  Node* BuildCallRef(uint32_t sig_index, Vector<Node*> args, Vector<Node*> rets,
                     CheckForNull null_check, IsReturnCall continuation,
                     wasm::WasmCodePosition position);

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

  Node* BuildTruncateIntPtrToInt32(Node* value);
  Node* BuildChangeInt32ToIntPtr(Node* value);
  Node* BuildChangeIntPtrToInt64(Node* value);
  Node* BuildChangeInt32ToSmi(Node* value);
  Node* BuildChangeUint31ToSmi(Node* value);
  Node* BuildSmiShiftBitsConstant();
  Node* BuildSmiShiftBitsConstant32();
  Node* BuildChangeSmiToInt32(Node* value);
  Node* BuildChangeSmiToIntPtr(Node* value);
  // generates {index > max ? Smi(max) : Smi(index)}
  Node* BuildConvertUint32ToSmiWithSaturation(Node* index, uint32_t maxval);

  using BranchBuilder = std::function<void(Node*, BranchHint)>;
  struct Callbacks {
    BranchBuilder succeed_if;
    BranchBuilder fail_if;
    BranchBuilder fail_if_not;
  };

  // This type is used to collect control/effect nodes we need to merge at the
  // end of BrOn* functions. Nodes are collected in {TypeCheck} etc. by calling
  // the passed callbacks succeed_if, fail_if and fail_if_not. We have up to 5
  // control nodes to merge; the EffectPhi needs an additional input.
  using SmallNodeVector = base::SmallVector<Node*, 6>;

  Callbacks TestCallbacks(GraphAssemblerLabel<1>* label);
  Callbacks CastCallbacks(GraphAssemblerLabel<0>* label,
                          wasm::WasmCodePosition position);
  Callbacks BranchCallbacks(SmallNodeVector& no_match_controls,
                            SmallNodeVector& no_match_effects,
                            SmallNodeVector& match_controls,
                            SmallNodeVector& match_effects);

  void TypeCheck(Node* object, Node* rtt, ObjectReferenceKnowledge config,
                 bool null_succeeds, Callbacks callbacks);
  void DataCheck(Node* object, bool object_can_be_null, Callbacks callbacks);
  void FuncCheck(Node* object, bool object_can_be_null, Callbacks callbacks);

  void BrOnCastAbs(Node** match_control, Node** match_effect,
                   Node** no_match_control, Node** no_match_effect,
                   std::function<void(Callbacks)> type_checker);

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

  // Wasm SIMD.
  Node* BuildF64x2Ceil(Node* input);
  Node* BuildF64x2Floor(Node* input);
  Node* BuildF64x2Trunc(Node* input);
  Node* BuildF64x2NearestInt(Node* input);
  Node* BuildF32x4Ceil(Node* input);
  Node* BuildF32x4Floor(Node* input);
  Node* BuildF32x4Trunc(Node* input);
  Node* BuildF32x4NearestInt(Node* input);

  void BuildEncodeException32BitValue(Node* values_array, uint32_t* index,
                                      Node* value);
  Node* BuildDecodeException32BitValue(Node* values_array, uint32_t* index);
  Node* BuildDecodeException64BitValue(Node* values_array, uint32_t* index);

  Node* BuildMultiReturnFixedArrayFromIterable(const wasm::FunctionSig* sig,
                                               Node* iterable, Node* context);

  Node* BuildLoadJumpTableOffsetFromExportedFunctionData(Node* function_data);

  //-----------------------------------------------------------------------
  // Operations involving the CEntry, a dependency we want to remove
  // to get off the GC heap.
  //-----------------------------------------------------------------------
  Node* BuildCallToRuntime(Runtime::FunctionId f, Node** parameters,
                           int parameter_count);

  Node* BuildCallToRuntimeWithContext(Runtime::FunctionId f, Node* js_context,
                                      Node** parameters, int parameter_count);
  TrapId GetTrapIdForTrap(wasm::TrapReason reason);

  void AddInt64LoweringReplacement(CallDescriptor* original,
                                   CallDescriptor* replacement);

  CallDescriptor* GetI32AtomicWaitCallDescriptor();

  CallDescriptor* GetI64AtomicWaitCallDescriptor();

  Node* StoreArgsInStackSlot(
      std::initializer_list<std::pair<MachineRepresentation, Node*>> args);

  std::unique_ptr<WasmGraphAssembler> gasm_;
  Zone* const zone_;
  MachineGraph* const mcgraph_;
  wasm::CompilationEnv* const env_;

  Node** parameters_;

  WasmInstanceCacheNodes* instance_cache_ = nullptr;

  SetOncePointer<Node> stack_check_code_node_;
  SetOncePointer<const Operator> stack_check_call_operator_;

  bool use_js_isolate_and_params() const { return isolate_ != nullptr; }
  bool has_simd_ = false;
  bool needs_stack_check_ = false;
  const bool untrusted_code_mitigations_ = true;

  const wasm::FunctionSig* const sig_;

  compiler::WasmDecorator* decorator_ = nullptr;

  compiler::SourcePositionTable* const source_position_table_ = nullptr;
  Isolate* const isolate_;
  SetOncePointer<Node> instance_node_;

  std::unique_ptr<Int64LoweringSpecialCase> lowering_special_case_;
  CallDescriptor* i32_atomic_wait_descriptor_ = nullptr;
  CallDescriptor* i64_atomic_wait_descriptor_ = nullptr;
};

enum WasmCallKind { kWasmFunction, kWasmImportWrapper, kWasmCapiFunction };

V8_EXPORT_PRIVATE void BuildInlinedJSToWasmWrapper(
    Zone* zone, MachineGraph* mcgraph, const wasm::FunctionSig* signature,
    const wasm::WasmModule* module, Isolate* isolate,
    compiler::SourcePositionTable* spt, StubCallMode stub_mode,
    wasm::WasmFeatures features, const JSWasmCallData* js_wasm_call_data,
    Node* frame_state);

V8_EXPORT_PRIVATE CallDescriptor* GetWasmCallDescriptor(
    Zone* zone, const wasm::FunctionSig* signature,
    WasmGraphBuilder::UseRetpoline use_retpoline =
        WasmGraphBuilder::kNoRetpoline,
    WasmCallKind kind = kWasmFunction, bool need_frame_state = false);

V8_EXPORT_PRIVATE CallDescriptor* GetI32WasmCallDescriptor(
    Zone* zone, const CallDescriptor* call_descriptor);

V8_EXPORT_PRIVATE CallDescriptor* GetI32WasmCallDescriptorForSimd(
    Zone* zone, CallDescriptor* call_descriptor);

AssemblerOptions WasmAssemblerOptions();
AssemblerOptions WasmStubAssemblerOptions();

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_WASM_COMPILER_H_
