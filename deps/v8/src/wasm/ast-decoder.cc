// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/platform/elapsed-timer.h"
#include "src/signature.h"

#include "src/bit-vector.h"
#include "src/flags.h"
#include "src/handles.h"
#include "src/zone-containers.h"

#include "src/wasm/ast-decoder.h"
#include "src/wasm/decoder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes.h"

#include "src/compiler/wasm-compiler.h"

namespace v8 {
namespace internal {
namespace wasm {

#if DEBUG
#define TRACE(...)                                    \
  do {                                                \
    if (FLAG_trace_wasm_decoder) PrintF(__VA_ARGS__); \
  } while (false)
#else
#define TRACE(...)
#endif

// The root of a decoded tree.
struct Tree {
  LocalType type;     // tree type.
  uint32_t count;     // number of children.
  const byte* pc;     // start of the syntax tree.
  TFNode* node;       // node in the TurboFan graph.
  Tree* children[1];  // pointers to children.

  WasmOpcode opcode() const { return static_cast<WasmOpcode>(*pc); }
};

// A production represents an incomplete decoded tree in the LR decoder.
struct Production {
  Tree* tree;  // the root of the syntax tree.
  int index;   // the current index into the children of the tree.

  WasmOpcode opcode() const { return static_cast<WasmOpcode>(*pc()); }
  const byte* pc() const { return tree->pc; }
  bool done() const { return index >= static_cast<int>(tree->count); }
  Tree* last() const { return index > 0 ? tree->children[index - 1] : nullptr; }
};


// An SsaEnv environment carries the current local variable renaming
// as well as the current effect and control dependency in the TF graph.
// It maintains a control state that tracks whether the environment
// is reachable, has reached a control end, or has been merged.
struct SsaEnv {
  enum State { kControlEnd, kUnreachable, kReached, kMerged };

  State state;
  TFNode* control;
  TFNode* effect;
  TFNode** locals;

  bool go() { return state >= kReached; }
  void Kill(State new_state = kControlEnd) {
    state = new_state;
    locals = nullptr;
    control = nullptr;
    effect = nullptr;
  }
};


// An entry in the stack of blocks during decoding.
struct Block {
  SsaEnv* ssa_env;  // SSA renaming environment.
  int stack_depth;  // production stack depth.
};


// An entry in the stack of ifs during decoding.
struct IfEnv {
  SsaEnv* false_env;
  SsaEnv* merge_env;
  SsaEnv** case_envs;
};


// Macros that build nodes only if there is a graph and the current SSA
// environment is reachable from start. This avoids problems with malformed
// TF graphs when decoding inputs that have unreachable code.
#define BUILD(func, ...) (build() ? builder_->func(__VA_ARGS__) : nullptr)
#define BUILD0(func) (build() ? builder_->func() : nullptr)


// Generic Wasm bytecode decoder with utilities for decoding operands,
// lengths, etc.
class WasmDecoder : public Decoder {
 public:
  WasmDecoder() : Decoder(nullptr, nullptr), function_env_(nullptr) {}
  WasmDecoder(FunctionEnv* env, const byte* start, const byte* end)
      : Decoder(start, end), function_env_(env) {}
  FunctionEnv* function_env_;

  void Reset(FunctionEnv* function_env, const byte* start, const byte* end) {
    Decoder::Reset(start, end);
    function_env_ = function_env;
  }

  byte ByteOperand(const byte* pc, const char* msg = "missing 1-byte operand") {
    if ((pc + sizeof(byte)) >= limit_) {
      error(pc, msg);
      return 0;
    }
    return pc[1];
  }

  uint32_t Uint32Operand(const byte* pc) {
    if ((pc + sizeof(uint32_t)) >= limit_) {
      error(pc, "missing 4-byte operand");
      return 0;
    }
    return read_u32(pc + 1);
  }

  uint64_t Uint64Operand(const byte* pc) {
    if ((pc + sizeof(uint64_t)) >= limit_) {
      error(pc, "missing 8-byte operand");
      return 0;
    }
    return read_u64(pc + 1);
  }

  inline bool Validate(const byte* pc, LocalIndexOperand& operand) {
    if (operand.index < function_env_->total_locals) {
      operand.type = function_env_->GetLocalType(operand.index);
      return true;
    }
    error(pc, pc + 1, "invalid local index");
    return false;
  }

  inline bool Validate(const byte* pc, GlobalIndexOperand& operand) {
    ModuleEnv* m = function_env_->module;
    if (m && m->module && operand.index < m->module->globals->size()) {
      operand.machine_type = m->module->globals->at(operand.index).type;
      operand.type = WasmOpcodes::LocalTypeFor(operand.machine_type);
      return true;
    }
    error(pc, pc + 1, "invalid global index");
    return false;
  }

  inline bool Validate(const byte* pc, FunctionIndexOperand& operand) {
    ModuleEnv* m = function_env_->module;
    if (m && m->module && operand.index < m->module->functions->size()) {
      operand.sig = m->module->functions->at(operand.index).sig;
      return true;
    }
    error(pc, pc + 1, "invalid function index");
    return false;
  }

  inline bool Validate(const byte* pc, SignatureIndexOperand& operand) {
    ModuleEnv* m = function_env_->module;
    if (m && m->module && operand.index < m->module->signatures->size()) {
      operand.sig = m->module->signatures->at(operand.index);
      return true;
    }
    error(pc, pc + 1, "invalid signature index");
    return false;
  }

  inline bool Validate(const byte* pc, ImportIndexOperand& operand) {
    ModuleEnv* m = function_env_->module;
    if (m && m->module && operand.index < m->module->import_table->size()) {
      operand.sig = m->module->import_table->at(operand.index).sig;
      return true;
    }
    error(pc, pc + 1, "invalid signature index");
    return false;
  }

  inline bool Validate(const byte* pc, BreakDepthOperand& operand,
                       ZoneVector<Block>& blocks) {
    if (operand.depth < blocks.size()) {
      operand.target = &blocks[blocks.size() - operand.depth - 1];
      return true;
    }
    error(pc, pc + 1, "invalid break depth");
    return false;
  }

  bool Validate(const byte* pc, TableSwitchOperand& operand,
                size_t block_depth) {
    if (operand.table_count == 0) {
      error(pc, "tableswitch with 0 entries");
      return false;
    }
    // Verify table.
    for (uint32_t i = 0; i < operand.table_count; i++) {
      uint16_t target = operand.read_entry(this, i);
      if (target >= 0x8000) {
        size_t depth = target - 0x8000;
        if (depth > block_depth) {
          error(operand.table + i * 2, "improper branch in tableswitch");
          return false;
        }
      } else {
        if (target >= operand.case_count) {
          error(operand.table + i * 2, "invalid case target in tableswitch");
          return false;
        }
      }
    }
    return true;
  }

  int OpcodeArity(const byte* pc) {
#define DECLARE_ARITY(name, ...)                          \
  static const LocalType kTypes_##name[] = {__VA_ARGS__}; \
  static const int kArity_##name =                        \
      static_cast<int>(arraysize(kTypes_##name) - 1);

    FOREACH_SIGNATURE(DECLARE_ARITY);
#undef DECLARE_ARITY

    switch (static_cast<WasmOpcode>(*pc)) {
      case kExprI8Const:
      case kExprI32Const:
      case kExprI64Const:
      case kExprF64Const:
      case kExprF32Const:
      case kExprGetLocal:
      case kExprLoadGlobal:
      case kExprNop:
      case kExprUnreachable:
        return 0;

      case kExprBr:
      case kExprStoreGlobal:
      case kExprSetLocal:
        return 1;

      case kExprIf:
      case kExprBrIf:
        return 2;
      case kExprIfElse:
      case kExprSelect:
        return 3;

      case kExprBlock:
      case kExprLoop: {
        BlockCountOperand operand(this, pc);
        return operand.count;
      }

      case kExprCallFunction: {
        FunctionIndexOperand operand(this, pc);
        return static_cast<int>(
            function_env_->module->GetFunctionSignature(operand.index)
                ->parameter_count());
      }
      case kExprCallIndirect: {
        SignatureIndexOperand operand(this, pc);
        return 1 + static_cast<int>(
                       function_env_->module->GetSignature(operand.index)
                           ->parameter_count());
      }
      case kExprCallImport: {
        ImportIndexOperand operand(this, pc);
        return static_cast<int>(
            function_env_->module->GetImportSignature(operand.index)
                ->parameter_count());
      }
      case kExprReturn: {
        return static_cast<int>(function_env_->sig->return_count());
      }
      case kExprTableSwitch: {
        TableSwitchOperand operand(this, pc);
        return 1 + operand.case_count;
      }

#define DECLARE_OPCODE_CASE(name, opcode, sig) \
  case kExpr##name:                            \
    return kArity_##sig;

        FOREACH_LOAD_MEM_OPCODE(DECLARE_OPCODE_CASE)
        FOREACH_STORE_MEM_OPCODE(DECLARE_OPCODE_CASE)
        FOREACH_MISC_MEM_OPCODE(DECLARE_OPCODE_CASE)
        FOREACH_SIMPLE_OPCODE(DECLARE_OPCODE_CASE)
#undef DECLARE_OPCODE_CASE
    }
    UNREACHABLE();
    return 0;
  }

  int OpcodeLength(const byte* pc) {
    switch (static_cast<WasmOpcode>(*pc)) {
#define DECLARE_OPCODE_CASE(name, opcode, sig) case kExpr##name:
      FOREACH_LOAD_MEM_OPCODE(DECLARE_OPCODE_CASE)
      FOREACH_STORE_MEM_OPCODE(DECLARE_OPCODE_CASE)
#undef DECLARE_OPCODE_CASE
      {
        MemoryAccessOperand operand(this, pc);
        return 1 + operand.length;
      }
      case kExprBlock:
      case kExprLoop: {
        BlockCountOperand operand(this, pc);
        return 1 + operand.length;
      }
      case kExprBr:
      case kExprBrIf: {
        BreakDepthOperand operand(this, pc);
        return 1 + operand.length;
      }
      case kExprStoreGlobal:
      case kExprLoadGlobal: {
        GlobalIndexOperand operand(this, pc);
        return 1 + operand.length;
      }

      case kExprCallFunction: {
        FunctionIndexOperand operand(this, pc);
        return 1 + operand.length;
      }
      case kExprCallIndirect: {
        SignatureIndexOperand operand(this, pc);
        return 1 + operand.length;
      }
      case kExprCallImport: {
        ImportIndexOperand operand(this, pc);
        return 1 + operand.length;
      }

      case kExprSetLocal:
      case kExprGetLocal: {
        LocalIndexOperand operand(this, pc);
        return 1 + operand.length;
      }
      case kExprTableSwitch: {
        TableSwitchOperand operand(this, pc);
        return 1 + operand.length;
      }
      case kExprI8Const:
        return 2;
      case kExprI32Const:
      case kExprF32Const:
        return 5;
      case kExprI64Const:
      case kExprF64Const:
        return 9;

      default:
        return 1;
    }
  }
};


// A shift-reduce-parser strategy for decoding Wasm code that uses an explicit
// shift-reduce strategy with multiple internal stacks.
class LR_WasmDecoder : public WasmDecoder {
 public:
  LR_WasmDecoder(Zone* zone, TFBuilder* builder)
      : zone_(zone),
        builder_(builder),
        trees_(zone),
        stack_(zone),
        blocks_(zone),
        ifs_(zone) {}

  TreeResult Decode(FunctionEnv* function_env, const byte* base, const byte* pc,
                    const byte* end) {
    base::ElapsedTimer decode_timer;
    if (FLAG_trace_wasm_decode_time) {
      decode_timer.Start();
    }
    trees_.clear();
    stack_.clear();
    blocks_.clear();
    ifs_.clear();

    if (end < pc) {
      error(pc, "function body end < start");
      return result_;
    }

    base_ = base;
    Reset(function_env, pc, end);

    InitSsaEnv();
    DecodeFunctionBody();

    Tree* tree = nullptr;
    if (ok()) {
      if (ssa_env_->go()) {
        if (stack_.size() > 0) {
          error(stack_.back().pc(), end, "fell off end of code");
        }
        AddImplicitReturnAtEnd();
      }
      if (trees_.size() == 0) {
        if (function_env_->sig->return_count() > 0) {
          error(start_, "no trees created");
        }
      } else {
        tree = trees_[0];
      }
    }

    if (ok()) {
      if (FLAG_trace_wasm_ast) {
        PrintAst(function_env, pc, end);
      }
      if (FLAG_trace_wasm_decode_time) {
        double ms = decode_timer.Elapsed().InMillisecondsF();
        PrintF("wasm-decode ok (%0.3f ms)\n\n", ms);
      } else {
        TRACE("wasm-decode ok\n\n");
      }
    } else {
      TRACE("wasm-error module+%-6d func+%d: %s\n\n", baserel(error_pc_),
            startrel(error_pc_), error_msg_.get());
    }

    return toResult(tree);
  }

 private:
  static const size_t kErrorMsgSize = 128;

  Zone* zone_;
  TFBuilder* builder_;
  const byte* base_;
  TreeResult result_;

  SsaEnv* ssa_env_;

  ZoneVector<Tree*> trees_;
  ZoneVector<Production> stack_;
  ZoneVector<Block> blocks_;
  ZoneVector<IfEnv> ifs_;

  inline bool build() { return builder_ && ssa_env_->go(); }

  void InitSsaEnv() {
    FunctionSig* sig = function_env_->sig;
    int param_count = static_cast<int>(sig->parameter_count());
    TFNode* start = nullptr;
    SsaEnv* ssa_env = reinterpret_cast<SsaEnv*>(zone_->New(sizeof(SsaEnv)));
    size_t size = sizeof(TFNode*) * EnvironmentCount();
    ssa_env->state = SsaEnv::kReached;
    ssa_env->locals =
        size > 0 ? reinterpret_cast<TFNode**>(zone_->New(size)) : nullptr;

    int pos = 0;
    if (builder_) {
      start = builder_->Start(param_count + 1);
      // Initialize parameters.
      for (int i = 0; i < param_count; i++) {
        ssa_env->locals[pos++] = builder_->Param(i, sig->GetParam(i));
      }
      // Initialize int32 locals.
      if (function_env_->local_i32_count > 0) {
        TFNode* zero = builder_->Int32Constant(0);
        for (uint32_t i = 0; i < function_env_->local_i32_count; i++) {
          ssa_env->locals[pos++] = zero;
        }
      }
      // Initialize int64 locals.
      if (function_env_->local_i64_count > 0) {
        TFNode* zero = builder_->Int64Constant(0);
        for (uint32_t i = 0; i < function_env_->local_i64_count; i++) {
          ssa_env->locals[pos++] = zero;
        }
      }
      // Initialize float32 locals.
      if (function_env_->local_f32_count > 0) {
        TFNode* zero = builder_->Float32Constant(0);
        for (uint32_t i = 0; i < function_env_->local_f32_count; i++) {
          ssa_env->locals[pos++] = zero;
        }
      }
      // Initialize float64 locals.
      if (function_env_->local_f64_count > 0) {
        TFNode* zero = builder_->Float64Constant(0);
        for (uint32_t i = 0; i < function_env_->local_f64_count; i++) {
          ssa_env->locals[pos++] = zero;
        }
      }
      DCHECK_EQ(function_env_->total_locals, pos);
      DCHECK_EQ(EnvironmentCount(), pos);
      builder_->set_module(function_env_->module);
    }
    ssa_env->control = start;
    ssa_env->effect = start;
    SetEnv("initial", ssa_env);
  }

  void Leaf(LocalType type, TFNode* node = nullptr) {
    size_t size = sizeof(Tree);
    Tree* tree = reinterpret_cast<Tree*>(zone_->New(size));
    tree->type = type;
    tree->count = 0;
    tree->pc = pc_;
    tree->node = node;
    tree->children[0] = nullptr;
    Reduce(tree);
  }

  void Shift(LocalType type, uint32_t count) {
    size_t size =
        sizeof(Tree) + (count == 0 ? 0 : ((count - 1) * sizeof(Tree*)));
    Tree* tree = reinterpret_cast<Tree*>(zone_->New(size));
    tree->type = type;
    tree->count = count;
    tree->pc = pc_;
    tree->node = nullptr;
    for (uint32_t i = 0; i < count; i++) tree->children[i] = nullptr;
    if (count == 0) {
      Production p = {tree, 0};
      Reduce(&p);
      Reduce(tree);
    } else {
      stack_.push_back({tree, 0});
    }
  }

  void Reduce(Tree* tree) {
    while (true) {
      if (stack_.size() == 0) {
        trees_.push_back(tree);
        break;
      }
      Production* p = &stack_.back();
      p->tree->children[p->index++] = tree;
      Reduce(p);
      if (p->done()) {
        tree = p->tree;
        stack_.pop_back();
      } else {
        break;
      }
    }
  }

  char* indentation() {
    static const int kMaxIndent = 64;
    static char bytes[kMaxIndent + 1];
    for (int i = 0; i < kMaxIndent; i++) bytes[i] = ' ';
    bytes[kMaxIndent] = 0;
    if (stack_.size() < kMaxIndent / 2) {
      bytes[stack_.size() * 2] = 0;
    }
    return bytes;
  }

  // Decodes the body of a function, producing reduced trees into {result}.
  void DecodeFunctionBody() {
    TRACE("wasm-decode %p...%p (%d bytes) %s\n",
          reinterpret_cast<const void*>(start_),
          reinterpret_cast<const void*>(limit_),
          static_cast<int>(limit_ - start_), builder_ ? "graph building" : "");

    if (pc_ >= limit_) return;  // Nothing to do.

    while (true) {  // decoding loop.
      int len = 1;
      WasmOpcode opcode = static_cast<WasmOpcode>(*pc_);
      TRACE("wasm-decode module+%-6d %s func+%d: 0x%02x %s\n", baserel(pc_),
            indentation(), startrel(pc_), opcode,
            WasmOpcodes::OpcodeName(opcode));

      FunctionSig* sig = WasmOpcodes::Signature(opcode);
      if (sig) {
        // A simple expression with a fixed signature.
        Shift(sig->GetReturn(), static_cast<uint32_t>(sig->parameter_count()));
        pc_ += len;
        if (pc_ >= limit_) {
          // End of code reached or exceeded.
          if (pc_ > limit_ && ok()) {
            error("Beyond end of code");
          }
          return;
        }
        continue;  // back to decoding loop.
      }

      switch (opcode) {
        case kExprNop:
          Leaf(kAstStmt);
          break;
        case kExprBlock: {
          BlockCountOperand operand(this, pc_);
          if (operand.count < 1) {
            Leaf(kAstStmt);
          } else {
            Shift(kAstEnd, operand.count);
            // The break environment is the outer environment.
            SsaEnv* break_env = ssa_env_;
            PushBlock(break_env);
            SetEnv("block:start", Steal(break_env));
          }
          len = 1 + operand.length;
          break;
        }
        case kExprLoop: {
          BlockCountOperand operand(this, pc_);
          if (operand.count < 1) {
            Leaf(kAstStmt);
          } else {
            Shift(kAstEnd, operand.count);
            // The break environment is the outer environment.
            SsaEnv* break_env = ssa_env_;
            PushBlock(break_env);
            SsaEnv* cont_env = Steal(break_env);
            // The continue environment is the inner environment.
            PrepareForLoop(cont_env);
            SetEnv("loop:start", Split(cont_env));
            if (ssa_env_->go()) ssa_env_->state = SsaEnv::kReached;
            PushBlock(cont_env);
            blocks_.back().stack_depth = -1;  // no production for inner block.
          }
          len = 1 + operand.length;
          break;
        }
        case kExprIf:
          Shift(kAstStmt, 2);
          break;
        case kExprIfElse:
          Shift(kAstEnd, 3);  // Result type is typeof(x) in {c ? x : y}.
          break;
        case kExprSelect:
          Shift(kAstStmt, 3);  // Result type is typeof(x) in {c ? x : y}.
          break;
        case kExprBr: {
          BreakDepthOperand operand(this, pc_);
          if (Validate(pc_, operand, blocks_)) {
            Shift(kAstEnd, 1);
          }
          len = 1 + operand.length;
          break;
        }
        case kExprBrIf: {
          BreakDepthOperand operand(this, pc_);
          if (Validate(pc_, operand, blocks_)) {
            Shift(kAstStmt, 2);
          }
          len = 1 + operand.length;
          break;
        }
        case kExprTableSwitch: {
          TableSwitchOperand operand(this, pc_);
          if (Validate(pc_, operand, blocks_.size())) {
            Shift(kAstEnd, 1 + operand.case_count);
          }
          len = 1 + operand.length;
          break;
        }
        case kExprReturn: {
          int count = static_cast<int>(function_env_->sig->return_count());
          if (count == 0) {
            BUILD(Return, 0, builder_->Buffer(0));
            ssa_env_->Kill();
            Leaf(kAstEnd);
          } else {
            Shift(kAstEnd, count);
          }
          break;
        }
        case kExprUnreachable: {
          BUILD0(Unreachable);
          ssa_env_->Kill(SsaEnv::kControlEnd);
          Leaf(kAstEnd, nullptr);
          break;
        }
        case kExprI8Const: {
          ImmI8Operand operand(this, pc_);
          Leaf(kAstI32, BUILD(Int32Constant, operand.value));
          len = 1 + operand.length;
          break;
        }
        case kExprI32Const: {
          ImmI32Operand operand(this, pc_);
          Leaf(kAstI32, BUILD(Int32Constant, operand.value));
          len = 1 + operand.length;
          break;
        }
        case kExprI64Const: {
          ImmI64Operand operand(this, pc_);
          Leaf(kAstI64, BUILD(Int64Constant, operand.value));
          len = 1 + operand.length;
          break;
        }
        case kExprF32Const: {
          ImmF32Operand operand(this, pc_);
          Leaf(kAstF32, BUILD(Float32Constant, operand.value));
          len = 1 + operand.length;
          break;
        }
        case kExprF64Const: {
          ImmF64Operand operand(this, pc_);
          Leaf(kAstF64, BUILD(Float64Constant, operand.value));
          len = 1 + operand.length;
          break;
        }
        case kExprGetLocal: {
          LocalIndexOperand operand(this, pc_);
          if (Validate(pc_, operand)) {
            TFNode* val = build() ? ssa_env_->locals[operand.index] : nullptr;
            Leaf(operand.type, val);
          }
          len = 1 + operand.length;
          break;
        }
        case kExprSetLocal: {
          LocalIndexOperand operand(this, pc_);
          if (Validate(pc_, operand)) {
            Shift(operand.type, 1);
          }
          len = 1 + operand.length;
          break;
        }
        case kExprLoadGlobal: {
          GlobalIndexOperand operand(this, pc_);
          if (Validate(pc_, operand)) {
            Leaf(operand.type, BUILD(LoadGlobal, operand.index));
          }
          len = 1 + operand.length;
          break;
        }
        case kExprStoreGlobal: {
          GlobalIndexOperand operand(this, pc_);
          if (Validate(pc_, operand)) {
            Shift(operand.type, 1);
          }
          len = 1 + operand.length;
          break;
        }
        case kExprI32LoadMem8S:
        case kExprI32LoadMem8U:
        case kExprI32LoadMem16S:
        case kExprI32LoadMem16U:
        case kExprI32LoadMem:
          len = DecodeLoadMem(pc_, kAstI32);
          break;
        case kExprI64LoadMem8S:
        case kExprI64LoadMem8U:
        case kExprI64LoadMem16S:
        case kExprI64LoadMem16U:
        case kExprI64LoadMem32S:
        case kExprI64LoadMem32U:
        case kExprI64LoadMem:
          len = DecodeLoadMem(pc_, kAstI64);
          break;
        case kExprF32LoadMem:
          len = DecodeLoadMem(pc_, kAstF32);
          break;
        case kExprF64LoadMem:
          len = DecodeLoadMem(pc_, kAstF64);
          break;
        case kExprI32StoreMem8:
        case kExprI32StoreMem16:
        case kExprI32StoreMem:
          len = DecodeStoreMem(pc_, kAstI32);
          break;
        case kExprI64StoreMem8:
        case kExprI64StoreMem16:
        case kExprI64StoreMem32:
        case kExprI64StoreMem:
          len = DecodeStoreMem(pc_, kAstI64);
          break;
        case kExprF32StoreMem:
          len = DecodeStoreMem(pc_, kAstF32);
          break;
        case kExprF64StoreMem:
          len = DecodeStoreMem(pc_, kAstF64);
          break;
        case kExprMemorySize:
          Leaf(kAstI32, BUILD(MemSize, 0));
          break;
        case kExprGrowMemory:
          Shift(kAstI32, 1);
          break;
        case kExprCallFunction: {
          FunctionIndexOperand operand(this, pc_);
          if (Validate(pc_, operand)) {
            LocalType type = operand.sig->return_count() == 0
                                 ? kAstStmt
                                 : operand.sig->GetReturn();
            Shift(type, static_cast<int>(operand.sig->parameter_count()));
          }
          len = 1 + operand.length;
          break;
        }
        case kExprCallIndirect: {
          SignatureIndexOperand operand(this, pc_);
          if (Validate(pc_, operand)) {
            LocalType type = operand.sig->return_count() == 0
                                 ? kAstStmt
                                 : operand.sig->GetReturn();
            Shift(type, static_cast<int>(1 + operand.sig->parameter_count()));
          }
          len = 1 + operand.length;
          break;
        }
        case kExprCallImport: {
          ImportIndexOperand operand(this, pc_);
          if (Validate(pc_, operand)) {
            LocalType type = operand.sig->return_count() == 0
                                 ? kAstStmt
                                 : operand.sig->GetReturn();
            Shift(type, static_cast<int>(operand.sig->parameter_count()));
          }
          len = 1 + operand.length;
          break;
        }
        default:
          error("Invalid opcode");
          return;
      }
      pc_ += len;
      if (pc_ >= limit_) {
        // End of code reached or exceeded.
        if (pc_ > limit_ && ok()) {
          error("Beyond end of code");
        }
        return;
      }
    }
  }

  void PushBlock(SsaEnv* ssa_env) {
    blocks_.push_back({ssa_env, static_cast<int>(stack_.size() - 1)});
  }

  int DecodeLoadMem(const byte* pc, LocalType type) {
    MemoryAccessOperand operand(this, pc);
    Shift(type, 1);
    return 1 + operand.length;
  }

  int DecodeStoreMem(const byte* pc, LocalType type) {
    MemoryAccessOperand operand(this, pc);
    Shift(type, 2);
    return 1 + operand.length;
  }

  void AddImplicitReturnAtEnd() {
    int retcount = static_cast<int>(function_env_->sig->return_count());
    if (retcount == 0) {
      BUILD0(ReturnVoid);
      return;
    }

    if (static_cast<int>(trees_.size()) < retcount) {
      error(limit_, nullptr,
            "ImplicitReturn expects %d arguments, only %d remain", retcount,
            static_cast<int>(trees_.size()));
      return;
    }

    TRACE("wasm-decode implicit return of %d args\n", retcount);

    TFNode** buffer = BUILD(Buffer, retcount);
    for (int index = 0; index < retcount; index++) {
      Tree* tree = trees_[trees_.size() - 1 - index];
      if (buffer) buffer[index] = tree->node;
      LocalType expected = function_env_->sig->GetReturn(index);
      if (tree->type != expected) {
        error(limit_, tree->pc,
              "ImplicitReturn[%d] expected type %s, found %s of type %s", index,
              WasmOpcodes::TypeName(expected),
              WasmOpcodes::OpcodeName(tree->opcode()),
              WasmOpcodes::TypeName(tree->type));
        return;
      }
    }

    BUILD(Return, retcount, buffer);
  }

  int baserel(const byte* ptr) {
    return base_ ? static_cast<int>(ptr - base_) : 0;
  }

  int startrel(const byte* ptr) { return static_cast<int>(ptr - start_); }

  void Reduce(Production* p) {
    WasmOpcode opcode = p->opcode();
    TRACE("-----reduce module+%-6d %s func+%d: 0x%02x %s\n", baserel(p->pc()),
          indentation(), startrel(p->pc()), opcode,
          WasmOpcodes::OpcodeName(opcode));
    FunctionSig* sig = WasmOpcodes::Signature(opcode);
    if (sig) {
      // A simple expression with a fixed signature.
      TypeCheckLast(p, sig->GetParam(p->index - 1));
      if (p->done() && build()) {
        if (sig->parameter_count() == 2) {
          p->tree->node = builder_->Binop(opcode, p->tree->children[0]->node,
                                          p->tree->children[1]->node);
        } else if (sig->parameter_count() == 1) {
          p->tree->node = builder_->Unop(opcode, p->tree->children[0]->node);
        } else {
          UNREACHABLE();
        }
      }
      return;
    }

    switch (opcode) {
      case kExprBlock: {
        if (p->done()) {
          Block* last = &blocks_.back();
          DCHECK_EQ(stack_.size() - 1, last->stack_depth);
          // fallthrough with the last expression.
          ReduceBreakToExprBlock(p, last);
          SetEnv("block:end", last->ssa_env);
          blocks_.pop_back();
        }
        break;
      }
      case kExprLoop: {
        if (p->done()) {
          // Pop the continue environment.
          blocks_.pop_back();
          // Get the break environment.
          Block* last = &blocks_.back();
          DCHECK_EQ(stack_.size() - 1, last->stack_depth);
          // fallthrough with the last expression.
          ReduceBreakToExprBlock(p, last);
          SetEnv("loop:end", last->ssa_env);
          blocks_.pop_back();
        }
        break;
      }
      case kExprIf: {
        if (p->index == 1) {
          // Condition done. Split environment for true branch.
          TypeCheckLast(p, kAstI32);
          SsaEnv* false_env = ssa_env_;
          SsaEnv* true_env = Split(ssa_env_);
          ifs_.push_back({nullptr, false_env, nullptr});
          BUILD(Branch, p->last()->node, &true_env->control,
                &false_env->control);
          SetEnv("if:true", true_env);
        } else if (p->index == 2) {
          // True block done. Merge true and false environments.
          IfEnv* env = &ifs_.back();
          SsaEnv* merge = env->merge_env;
          if (merge->go()) {
            merge->state = SsaEnv::kReached;
            Goto(ssa_env_, merge);
          }
          SetEnv("if:merge", merge);
          ifs_.pop_back();
        }
        break;
      }
      case kExprIfElse: {
        if (p->index == 1) {
          // Condition done. Split environment for true and false branches.
          TypeCheckLast(p, kAstI32);
          SsaEnv* merge_env = ssa_env_;
          TFNode* if_true = nullptr;
          TFNode* if_false = nullptr;
          BUILD(Branch, p->last()->node, &if_true, &if_false);
          SsaEnv* false_env = Split(ssa_env_);
          SsaEnv* true_env = Steal(ssa_env_);
          false_env->control = if_false;
          true_env->control = if_true;
          ifs_.push_back({false_env, merge_env, nullptr});
          SetEnv("if_else:true", true_env);
        } else if (p->index == 2) {
          // True expr done.
          IfEnv* env = &ifs_.back();
          MergeIntoProduction(p, env->merge_env, p->last());
          // Switch to environment for false branch.
          SsaEnv* false_env = ifs_.back().false_env;
          SetEnv("if_else:false", false_env);
        } else if (p->index == 3) {
          // False expr done.
          IfEnv* env = &ifs_.back();
          MergeIntoProduction(p, env->merge_env, p->last());
          SetEnv("if_else:merge", env->merge_env);
          ifs_.pop_back();
        }
        break;
      }
      case kExprSelect: {
        if (p->index == 1) {
          // True expression done.
          p->tree->type = p->last()->type;
          if (p->tree->type == kAstStmt) {
            error(p->pc(), p->tree->children[1]->pc,
                  "select operand should be expression");
          }
        } else if (p->index == 2) {
          // False expression done.
          TypeCheckLast(p, p->tree->type);
        } else {
          // Condition done.
          DCHECK(p->done());
          TypeCheckLast(p, kAstI32);
          if (build()) {
            TFNode* controls[2];
            builder_->Branch(p->tree->children[2]->node, &controls[0],
                             &controls[1]);
            TFNode* merge = builder_->Merge(2, controls);
            TFNode* vals[2] = {p->tree->children[0]->node,
                               p->tree->children[1]->node};
            TFNode* phi = builder_->Phi(p->tree->type, 2, vals, merge);
            p->tree->node = phi;
            ssa_env_->control = merge;
          }
        }
        break;
      }
      case kExprBr: {
        BreakDepthOperand operand(this, p->pc());
        CHECK(Validate(p->pc(), operand, blocks_));
        ReduceBreakToExprBlock(p, operand.target);
        break;
      }
      case kExprBrIf: {
        if (p->done()) {
          TypeCheckLast(p, kAstI32);
          BreakDepthOperand operand(this, p->pc());
          CHECK(Validate(p->pc(), operand, blocks_));
          SsaEnv* fenv = ssa_env_;
          SsaEnv* tenv = Split(fenv);
          BUILD(Branch, p->tree->children[1]->node, &tenv->control,
                &fenv->control);
          ssa_env_ = tenv;
          ReduceBreakToExprBlock(p, operand.target, p->tree->children[0]);
          ssa_env_ = fenv;
        }
        break;
      }
      case kExprTableSwitch: {
        if (p->index == 1) {
          // Switch key finished.
          TypeCheckLast(p, kAstI32);
          if (failed()) break;

          TableSwitchOperand operand(this, p->pc());
          DCHECK(Validate(p->pc(), operand, blocks_.size()));

          // Build the switch only if it has more than just a default target.
          bool build_switch = operand.table_count > 1;
          TFNode* sw = nullptr;
          if (build_switch)
            sw = BUILD(Switch, operand.table_count, p->last()->node);

          // Allocate environments for each case.
          SsaEnv** case_envs = zone_->NewArray<SsaEnv*>(operand.case_count);
          for (uint32_t i = 0; i < operand.case_count; i++) {
            case_envs[i] = UnreachableEnv();
          }

          ifs_.push_back({nullptr, nullptr, case_envs});
          SsaEnv* break_env = ssa_env_;
          PushBlock(break_env);
          SsaEnv* copy = Steal(break_env);
          ssa_env_ = copy;

          // Build the environments for each case based on the table.
          for (uint32_t i = 0; i < operand.table_count; i++) {
            uint16_t target = operand.read_entry(this, i);
            SsaEnv* env = copy;
            if (build_switch) {
              env = Split(env);
              env->control = (i == operand.table_count - 1)
                                 ? BUILD(IfDefault, sw)
                                 : BUILD(IfValue, i, sw);
            }
            if (target >= 0x8000) {
              // Targets an outer block.
              int depth = target - 0x8000;
              SsaEnv* tenv = blocks_[blocks_.size() - depth - 1].ssa_env;
              Goto(env, tenv);
            } else {
              // Targets a case.
              Goto(env, case_envs[target]);
            }
          }
        }

        if (p->done()) {
          // Last case. Fall through to the end.
          Block* block = &blocks_.back();
          if (p->index > 1) ReduceBreakToExprBlock(p, block);
          SsaEnv* next = block->ssa_env;
          blocks_.pop_back();
          ifs_.pop_back();
          SetEnv("switch:end", next);
        } else {
          // Interior case. Maybe fall through to the next case.
          SsaEnv* next = ifs_.back().case_envs[p->index - 1];
          if (p->index > 1 && ssa_env_->go()) Goto(ssa_env_, next);
          SetEnv("switch:case", next);
        }
        break;
      }
      case kExprReturn: {
        TypeCheckLast(p, function_env_->sig->GetReturn(p->index - 1));
        if (p->done()) {
          if (build()) {
            int count = p->tree->count;
            TFNode** buffer = builder_->Buffer(count);
            for (int i = 0; i < count; i++) {
              buffer[i] = p->tree->children[i]->node;
            }
            BUILD(Return, count, buffer);
          }
          ssa_env_->Kill(SsaEnv::kControlEnd);
        }
        break;
      }
      case kExprSetLocal: {
        LocalIndexOperand operand(this, p->pc());
        CHECK(Validate(p->pc(), operand));
        Tree* val = p->last();
        if (operand.type == val->type) {
          if (build()) ssa_env_->locals[operand.index] = val->node;
          p->tree->node = val->node;
        } else {
          error(p->pc(), val->pc, "Typecheck failed in SetLocal");
        }
        break;
      }
      case kExprStoreGlobal: {
        GlobalIndexOperand operand(this, p->pc());
        CHECK(Validate(p->pc(), operand));
        Tree* val = p->last();
        if (operand.type == val->type) {
          BUILD(StoreGlobal, operand.index, val->node);
          p->tree->node = val->node;
        } else {
          error(p->pc(), val->pc, "Typecheck failed in StoreGlobal");
        }
        break;
      }

      case kExprI32LoadMem8S:
        return ReduceLoadMem(p, kAstI32, MachineType::Int8());
      case kExprI32LoadMem8U:
        return ReduceLoadMem(p, kAstI32, MachineType::Uint8());
      case kExprI32LoadMem16S:
        return ReduceLoadMem(p, kAstI32, MachineType::Int16());
      case kExprI32LoadMem16U:
        return ReduceLoadMem(p, kAstI32, MachineType::Uint16());
      case kExprI32LoadMem:
        return ReduceLoadMem(p, kAstI32, MachineType::Int32());

      case kExprI64LoadMem8S:
        return ReduceLoadMem(p, kAstI64, MachineType::Int8());
      case kExprI64LoadMem8U:
        return ReduceLoadMem(p, kAstI64, MachineType::Uint8());
      case kExprI64LoadMem16S:
        return ReduceLoadMem(p, kAstI64, MachineType::Int16());
      case kExprI64LoadMem16U:
        return ReduceLoadMem(p, kAstI64, MachineType::Uint16());
      case kExprI64LoadMem32S:
        return ReduceLoadMem(p, kAstI64, MachineType::Int32());
      case kExprI64LoadMem32U:
        return ReduceLoadMem(p, kAstI64, MachineType::Uint32());
      case kExprI64LoadMem:
        return ReduceLoadMem(p, kAstI64, MachineType::Int64());

      case kExprF32LoadMem:
        return ReduceLoadMem(p, kAstF32, MachineType::Float32());

      case kExprF64LoadMem:
        return ReduceLoadMem(p, kAstF64, MachineType::Float64());

      case kExprI32StoreMem8:
        return ReduceStoreMem(p, kAstI32, MachineType::Int8());
      case kExprI32StoreMem16:
        return ReduceStoreMem(p, kAstI32, MachineType::Int16());
      case kExprI32StoreMem:
        return ReduceStoreMem(p, kAstI32, MachineType::Int32());

      case kExprI64StoreMem8:
        return ReduceStoreMem(p, kAstI64, MachineType::Int8());
      case kExprI64StoreMem16:
        return ReduceStoreMem(p, kAstI64, MachineType::Int16());
      case kExprI64StoreMem32:
        return ReduceStoreMem(p, kAstI64, MachineType::Int32());
      case kExprI64StoreMem:
        return ReduceStoreMem(p, kAstI64, MachineType::Int64());

      case kExprF32StoreMem:
        return ReduceStoreMem(p, kAstF32, MachineType::Float32());

      case kExprF64StoreMem:
        return ReduceStoreMem(p, kAstF64, MachineType::Float64());

      case kExprGrowMemory:
        TypeCheckLast(p, kAstI32);
        // TODO(titzer): build node for GrowMemory
        p->tree->node = BUILD(Int32Constant, 0);
        return;

      case kExprCallFunction: {
        FunctionIndexOperand operand(this, p->pc());
        CHECK(Validate(p->pc(), operand));
        if (p->index > 0) {
          TypeCheckLast(p, operand.sig->GetParam(p->index - 1));
        }
        if (p->done() && build()) {
          uint32_t count = p->tree->count + 1;
          TFNode** buffer = builder_->Buffer(count);
          buffer[0] = nullptr;  // reserved for code object.
          for (uint32_t i = 1; i < count; i++) {
            buffer[i] = p->tree->children[i - 1]->node;
          }
          p->tree->node = builder_->CallDirect(operand.index, buffer);
        }
        break;
      }
      case kExprCallIndirect: {
        SignatureIndexOperand operand(this, p->pc());
        CHECK(Validate(p->pc(), operand));
        if (p->index == 1) {
          TypeCheckLast(p, kAstI32);
        } else {
          TypeCheckLast(p, operand.sig->GetParam(p->index - 2));
        }
        if (p->done() && build()) {
          uint32_t count = p->tree->count;
          TFNode** buffer = builder_->Buffer(count);
          for (uint32_t i = 0; i < count; i++) {
            buffer[i] = p->tree->children[i]->node;
          }
          p->tree->node = builder_->CallIndirect(operand.index, buffer);
        }
        break;
      }
      case kExprCallImport: {
        ImportIndexOperand operand(this, p->pc());
        CHECK(Validate(p->pc(), operand));
        if (p->index > 0) {
          TypeCheckLast(p, operand.sig->GetParam(p->index - 1));
        }
        if (p->done() && build()) {
          uint32_t count = p->tree->count + 1;
          TFNode** buffer = builder_->Buffer(count);
          buffer[0] = nullptr;  // reserved for code object.
          for (uint32_t i = 1; i < count; i++) {
            buffer[i] = p->tree->children[i - 1]->node;
          }
          p->tree->node = builder_->CallImport(operand.index, buffer);
        }
        break;
      }
      default:
        break;
    }
  }

  void ReduceBreakToExprBlock(Production* p, Block* block) {
    ReduceBreakToExprBlock(p, block, p->tree->count > 0 ? p->last() : nullptr);
  }

  void ReduceBreakToExprBlock(Production* p, Block* block, Tree* val) {
    if (block->stack_depth < 0) {
      // This is the inner loop block, which does not have a value.
      Goto(ssa_env_, block->ssa_env);
    } else {
      // Merge the value into the production for the block.
      Production* bp = &stack_[block->stack_depth];
      MergeIntoProduction(bp, block->ssa_env, val);
    }
  }

  void MergeIntoProduction(Production* p, SsaEnv* target, Tree* expr) {
    if (!ssa_env_->go()) return;

    bool first = target->state == SsaEnv::kUnreachable;
    Goto(ssa_env_, target);
    if (expr == nullptr || expr->type == kAstEnd) return;

    if (first) {
      // first merge to this environment; set the type and the node.
      p->tree->type = expr->type;
      p->tree->node = expr->node;
    } else {
      // merge with the existing value for this block.
      LocalType type = p->tree->type;
      if (expr->type != type) {
        type = kAstStmt;
        p->tree->type = kAstStmt;
        p->tree->node = nullptr;
      } else if (type != kAstStmt) {
        p->tree->node = CreateOrMergeIntoPhi(type, target->control,
                                             p->tree->node, expr->node);
      }
    }
  }

  void ReduceLoadMem(Production* p, LocalType type, MachineType mem_type) {
    DCHECK_EQ(1, p->index);
    TypeCheckLast(p, kAstI32);  // index
    if (build()) {
      MemoryAccessOperand operand(this, p->pc());
      p->tree->node =
          builder_->LoadMem(type, mem_type, p->last()->node, operand.offset);
    }
  }

  void ReduceStoreMem(Production* p, LocalType type, MachineType mem_type) {
    if (p->index == 1) {
      TypeCheckLast(p, kAstI32);  // index
    } else {
      DCHECK_EQ(2, p->index);
      TypeCheckLast(p, type);
      if (build()) {
        MemoryAccessOperand operand(this, p->pc());
        TFNode* val = p->tree->children[1]->node;
        builder_->StoreMem(mem_type, p->tree->children[0]->node, operand.offset,
                           val);
        p->tree->node = val;
      }
    }
  }

  void TypeCheckLast(Production* p, LocalType expected) {
    LocalType result = p->last()->type;
    if (result == expected) return;
    if (result == kAstEnd) return;
    if (expected != kAstStmt) {
      error(p->pc(), p->last()->pc,
            "%s[%d] expected type %s, found %s of type %s",
            WasmOpcodes::OpcodeName(p->opcode()), p->index - 1,
            WasmOpcodes::TypeName(expected),
            WasmOpcodes::OpcodeName(p->last()->opcode()),
            WasmOpcodes::TypeName(p->last()->type));
    }
  }

  void SetEnv(const char* reason, SsaEnv* env) {
    TRACE("  env = %p, block depth = %d, reason = %s", static_cast<void*>(env),
          static_cast<int>(blocks_.size()), reason);
    if (FLAG_trace_wasm_decoder && env && env->control) {
      TRACE(", control = ");
      compiler::WasmGraphBuilder::PrintDebugName(env->control);
    }
    TRACE("\n");
    ssa_env_ = env;
    if (builder_) {
      builder_->set_control_ptr(&env->control);
      builder_->set_effect_ptr(&env->effect);
    }
  }

  void Goto(SsaEnv* from, SsaEnv* to) {
    DCHECK_NOT_NULL(to);
    if (!from->go()) return;
    switch (to->state) {
      case SsaEnv::kUnreachable: {  // Overwrite destination.
        to->state = SsaEnv::kReached;
        to->locals = from->locals;
        to->control = from->control;
        to->effect = from->effect;
        break;
      }
      case SsaEnv::kReached: {  // Create a new merge.
        to->state = SsaEnv::kMerged;
        if (!builder_) break;
        // Merge control.
        TFNode* controls[] = {to->control, from->control};
        TFNode* merge = builder_->Merge(2, controls);
        to->control = merge;
        // Merge effects.
        if (from->effect != to->effect) {
          TFNode* effects[] = {to->effect, from->effect, merge};
          to->effect = builder_->EffectPhi(2, effects, merge);
        }
        // Merge SSA values.
        for (int i = EnvironmentCount() - 1; i >= 0; i--) {
          TFNode* a = to->locals[i];
          TFNode* b = from->locals[i];
          if (a != b) {
            TFNode* vals[] = {a, b};
            to->locals[i] =
                builder_->Phi(function_env_->GetLocalType(i), 2, vals, merge);
          }
        }
        break;
      }
      case SsaEnv::kMerged: {
        if (!builder_) break;
        TFNode* merge = to->control;
        // Extend the existing merge.
        builder_->AppendToMerge(merge, from->control);
        // Merge effects.
        if (builder_->IsPhiWithMerge(to->effect, merge)) {
          builder_->AppendToPhi(merge, to->effect, from->effect);
        } else if (to->effect != from->effect) {
          uint32_t count = builder_->InputCount(merge);
          TFNode** effects = builder_->Buffer(count);
          for (uint32_t j = 0; j < count - 1; j++) {
            effects[j] = to->effect;
          }
          effects[count - 1] = from->effect;
          to->effect = builder_->EffectPhi(count, effects, merge);
        }
        // Merge locals.
        for (int i = EnvironmentCount() - 1; i >= 0; i--) {
          TFNode* tnode = to->locals[i];
          TFNode* fnode = from->locals[i];
          if (builder_->IsPhiWithMerge(tnode, merge)) {
            builder_->AppendToPhi(merge, tnode, fnode);
          } else if (tnode != fnode) {
            uint32_t count = builder_->InputCount(merge);
            TFNode** vals = builder_->Buffer(count);
            for (uint32_t j = 0; j < count - 1; j++) {
              vals[j] = tnode;
            }
            vals[count - 1] = fnode;
            to->locals[i] = builder_->Phi(function_env_->GetLocalType(i), count,
                                          vals, merge);
          }
        }
        break;
      }
      default:
        UNREACHABLE();
    }
    return from->Kill();
  }

  TFNode* CreateOrMergeIntoPhi(LocalType type, TFNode* merge, TFNode* tnode,
                               TFNode* fnode) {
    if (builder_->IsPhiWithMerge(tnode, merge)) {
      builder_->AppendToPhi(merge, tnode, fnode);
    } else if (tnode != fnode) {
      uint32_t count = builder_->InputCount(merge);
      TFNode** vals = builder_->Buffer(count);
      for (uint32_t j = 0; j < count - 1; j++) vals[j] = tnode;
      vals[count - 1] = fnode;
      return builder_->Phi(type, count, vals, merge);
    }
    return tnode;
  }

  void BuildInfiniteLoop() {
    if (ssa_env_->go()) {
      PrepareForLoop(ssa_env_);
      SsaEnv* cont_env = ssa_env_;
      ssa_env_ = Split(ssa_env_);
      ssa_env_->state = SsaEnv::kReached;
      Goto(ssa_env_, cont_env);
    }
  }

  void PrepareForLoop(SsaEnv* env) {
    if (env->go()) {
      env->state = SsaEnv::kMerged;
      if (builder_) {
        env->control = builder_->Loop(env->control);
        env->effect = builder_->EffectPhi(1, &env->effect, env->control);
        builder_->Terminate(env->effect, env->control);
        for (int i = EnvironmentCount() - 1; i >= 0; i--) {
          env->locals[i] = builder_->Phi(function_env_->GetLocalType(i), 1,
                                         &env->locals[i], env->control);
        }
      }
    }
  }

  // Create a complete copy of the {from}.
  SsaEnv* Split(SsaEnv* from) {
    DCHECK_NOT_NULL(from);
    SsaEnv* result = reinterpret_cast<SsaEnv*>(zone_->New(sizeof(SsaEnv)));
    size_t size = sizeof(TFNode*) * EnvironmentCount();
    result->control = from->control;
    result->effect = from->effect;
    result->state = from->state == SsaEnv::kUnreachable ? SsaEnv::kUnreachable
                                                        : SsaEnv::kReached;

    if (from->go()) {
      result->state = SsaEnv::kReached;
      result->locals =
          size > 0 ? reinterpret_cast<TFNode**>(zone_->New(size)) : nullptr;
      memcpy(result->locals, from->locals, size);
    } else {
      result->state = SsaEnv::kUnreachable;
      result->locals = nullptr;
    }

    return result;
  }

  // Create a copy of {from} that steals its state and leaves {from}
  // unreachable.
  SsaEnv* Steal(SsaEnv* from) {
    DCHECK_NOT_NULL(from);
    if (!from->go()) return UnreachableEnv();
    SsaEnv* result = reinterpret_cast<SsaEnv*>(zone_->New(sizeof(SsaEnv)));
    result->state = SsaEnv::kReached;
    result->locals = from->locals;
    result->control = from->control;
    result->effect = from->effect;
    from->Kill(SsaEnv::kUnreachable);
    return result;
  }

  // Create an unreachable environment.
  SsaEnv* UnreachableEnv() {
    SsaEnv* result = reinterpret_cast<SsaEnv*>(zone_->New(sizeof(SsaEnv)));
    result->state = SsaEnv::kUnreachable;
    result->control = nullptr;
    result->effect = nullptr;
    result->locals = nullptr;
    return result;
  }

  int EnvironmentCount() {
    if (builder_) return static_cast<int>(function_env_->GetLocalCount());
    return 0;  // if we aren't building a graph, don't bother with SSA renaming.
  }

  virtual void onFirstError() {
    limit_ = start_;     // Terminate decoding loop.
    builder_ = nullptr;  // Don't build any more nodes.
#if DEBUG
    PrintStackForDebugging();
#endif
  }

#if DEBUG
  void PrintStackForDebugging() { PrintProduction(0); }

  void PrintProduction(size_t depth) {
    if (depth >= stack_.size()) return;
    Production* p = &stack_[depth];
    for (size_t d = 0; d < depth; d++) PrintF("  ");

    PrintF("@%d %s [%d]\n", static_cast<int>(p->tree->pc - start_),
           WasmOpcodes::OpcodeName(p->opcode()), p->tree->count);
    for (int i = 0; i < p->index; i++) {
      Tree* child = p->tree->children[i];
      for (size_t d = 0; d <= depth; d++) PrintF("  ");
      PrintF("@%d %s [%d]", static_cast<int>(child->pc - start_),
             WasmOpcodes::OpcodeName(child->opcode()), child->count);
      if (child->node) {
        PrintF(" => TF");
        compiler::WasmGraphBuilder::PrintDebugName(child->node);
      }
      PrintF("\n");
    }
    PrintProduction(depth + 1);
  }
#endif
};


TreeResult VerifyWasmCode(FunctionEnv* env, const byte* base, const byte* start,
                          const byte* end) {
  Zone zone;
  LR_WasmDecoder decoder(&zone, nullptr);
  TreeResult result = decoder.Decode(env, base, start, end);
  return result;
}


TreeResult BuildTFGraph(TFBuilder* builder, FunctionEnv* env, const byte* base,
                        const byte* start, const byte* end) {
  Zone zone;
  LR_WasmDecoder decoder(&zone, builder);
  TreeResult result = decoder.Decode(env, base, start, end);
  return result;
}


std::ostream& operator<<(std::ostream& os, const Tree& tree) {
  if (tree.pc == nullptr) {
    os << "null";
    return os;
  }
  PrintF("%s", WasmOpcodes::OpcodeName(tree.opcode()));
  if (tree.count > 0) os << "(";
  for (uint32_t i = 0; i < tree.count; i++) {
    if (i > 0) os << ", ";
    os << *tree.children[i];
  }
  if (tree.count > 0) os << ")";
  return os;
}


ReadUnsignedLEB128ErrorCode ReadUnsignedLEB128Operand(const byte* pc,
                                                      const byte* limit,
                                                      int* length,
                                                      uint32_t* result) {
  Decoder decoder(pc, limit);
  *result = decoder.checked_read_u32v(pc, 0, length);
  if (decoder.ok()) return kNoError;
  return (limit - pc) > 1 ? kInvalidLEB128 : kMissingLEB128;
}

int OpcodeLength(const byte* pc, const byte* end) {
  WasmDecoder decoder(nullptr, pc, end);
  return decoder.OpcodeLength(pc);
}

int OpcodeArity(FunctionEnv* env, const byte* pc, const byte* end) {
  WasmDecoder decoder(env, pc, end);
  return decoder.OpcodeArity(pc);
}

void PrintAst(FunctionEnv* env, const byte* start, const byte* end) {
  WasmDecoder decoder(env, start, end);
  const byte* pc = start;
  std::vector<int> arity_stack;
  while (pc < end) {
    int arity = decoder.OpcodeArity(pc);
    size_t length = decoder.OpcodeLength(pc);

    for (auto arity : arity_stack) {
      printf("  ");
      USE(arity);
    }

    WasmOpcode opcode = static_cast<WasmOpcode>(*pc);
    printf("k%s,", WasmOpcodes::OpcodeName(opcode));

    for (size_t i = 1; i < length; i++) {
      printf(" 0x%02x,", pc[i]);
    }
    pc += length;
    printf("\n");

    arity_stack.push_back(arity);
    while (arity_stack.back() == 0) {
      arity_stack.pop_back();
      if (arity_stack.empty()) break;
      arity_stack.back()--;
    }
  }
}

// Analyzes loop bodies for static assignments to locals, which helps in
// reducing the number of phis introduced at loop headers.
class LoopAssignmentAnalyzer : public WasmDecoder {
 public:
  LoopAssignmentAnalyzer(Zone* zone, FunctionEnv* function_env) : zone_(zone) {
    function_env_ = function_env;
  }

  BitVector* Analyze(const byte* pc, const byte* limit) {
    Decoder::Reset(pc, limit);
    if (pc_ >= limit_) return nullptr;
    if (*pc_ != kExprLoop) return nullptr;

    BitVector* assigned =
        new (zone_) BitVector(function_env_->total_locals, zone_);
    // Keep a stack to model the nesting of expressions.
    std::vector<int> arity_stack;
    arity_stack.push_back(OpcodeArity(pc_));
    pc_ += OpcodeLength(pc_);

    // Iteratively process all AST nodes nested inside the loop.
    while (pc_ < limit_) {
      WasmOpcode opcode = static_cast<WasmOpcode>(*pc_);
      int arity = 0;
      int length = 1;
      if (opcode == kExprSetLocal) {
        LocalIndexOperand operand(this, pc_);
        if (assigned->length() > 0 &&
            static_cast<int>(operand.index) < assigned->length()) {
          // Unverified code might have an out-of-bounds index.
          assigned->Add(operand.index);
        }
        arity = 1;
        length = 1 + operand.length;
      } else {
        arity = OpcodeArity(pc_);
        length = OpcodeLength(pc_);
      }

      pc_ += length;
      arity_stack.push_back(arity);
      while (arity_stack.back() == 0) {
        arity_stack.pop_back();
        if (arity_stack.empty()) return assigned;  // reached end of loop
        arity_stack.back()--;
      }
    }
    return assigned;
  }

 private:
  Zone* zone_;
};


BitVector* AnalyzeLoopAssignmentForTesting(Zone* zone, FunctionEnv* env,
                                           const byte* start, const byte* end) {
  LoopAssignmentAnalyzer analyzer(zone, env);
  return analyzer.Analyze(start, end);
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
