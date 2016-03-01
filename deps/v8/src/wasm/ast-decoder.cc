// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/platform/elapsed-timer.h"
#include "src/signature.h"

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


// A shift-reduce-parser strategy for decoding Wasm code that uses an explicit
// shift-reduce strategy with multiple internal stacks.
class LR_WasmDecoder : public Decoder {
 public:
  LR_WasmDecoder(Zone* zone, TFBuilder* builder)
      : Decoder(nullptr, nullptr),
        zone_(zone),
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
    Reset(pc, end);
    function_env_ = function_env;

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
      if (FLAG_trace_wasm_decode_time) {
        double ms = decode_timer.Elapsed().InMillisecondsF();
        PrintF(" - decoding took %0.3f ms\n", ms);
      }
      TRACE("wasm-decode ok\n\n");
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
  FunctionEnv* function_env_;

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
      if (function_env_->local_int32_count > 0) {
        TFNode* zero = builder_->Int32Constant(0);
        for (uint32_t i = 0; i < function_env_->local_int32_count; i++) {
          ssa_env->locals[pos++] = zero;
        }
      }
      // Initialize int64 locals.
      if (function_env_->local_int64_count > 0) {
        TFNode* zero = builder_->Int64Constant(0);
        for (uint32_t i = 0; i < function_env_->local_int64_count; i++) {
          ssa_env->locals[pos++] = zero;
        }
      }
      // Initialize float32 locals.
      if (function_env_->local_float32_count > 0) {
        TFNode* zero = builder_->Float32Constant(0);
        for (uint32_t i = 0; i < function_env_->local_float32_count; i++) {
          ssa_env->locals[pos++] = zero;
        }
      }
      // Initialize float64 locals.
      if (function_env_->local_float64_count > 0) {
        TFNode* zero = builder_->Float64Constant(0);
        for (uint32_t i = 0; i < function_env_->local_float64_count; i++) {
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
          int length = Operand<uint8_t>(pc_);
          if (length < 1) {
            Leaf(kAstStmt);
          } else {
            Shift(kAstEnd, length);
            // The break environment is the outer environment.
            SsaEnv* break_env = ssa_env_;
            PushBlock(break_env);
            SetEnv("block:start", Steal(break_env));
          }
          len = 2;
          break;
        }
        case kExprLoop: {
          int length = Operand<uint8_t>(pc_);
          if (length < 1) {
            Leaf(kAstStmt);
          } else {
            Shift(kAstEnd, length);
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
          len = 2;
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
          uint32_t depth = Operand<uint8_t>(pc_);
          Shift(kAstEnd, 1);
          if (depth >= blocks_.size()) {
            error("improperly nested branch");
          }
          len = 2;
          break;
        }
        case kExprBrIf: {
          uint32_t depth = Operand<uint8_t>(pc_);
          Shift(kAstStmt, 2);
          if (depth >= blocks_.size()) {
            error("improperly nested conditional branch");
          }
          len = 2;
          break;
        }
        case kExprTableSwitch: {
          if (!checkAvailable(5)) {
            error("expected #tableswitch <cases> <table>, fell off end");
            break;
          }
          uint16_t case_count = *reinterpret_cast<const uint16_t*>(pc_ + 1);
          uint16_t table_count = *reinterpret_cast<const uint16_t*>(pc_ + 3);
          len = 5 + table_count * 2;

          if (table_count == 0) {
            error("tableswitch with 0 entries");
            break;
          }

          if (!checkAvailable(len)) {
            error("expected #tableswitch <cases> <table>, fell off end");
            break;
          }

          Shift(kAstEnd, 1 + case_count);

          // Verify table.
          for (int i = 0; i < table_count; i++) {
            uint16_t target =
                *reinterpret_cast<const uint16_t*>(pc_ + 5 + i * 2);
            if (target >= 0x8000) {
              size_t depth = target - 0x8000;
              if (depth > blocks_.size()) {
                error(pc_ + 5 + i * 2, "improper branch in tableswitch");
              }
            } else {
              if (target >= case_count) {
                error(pc_ + 5 + i * 2, "invalid case target in tableswitch");
              }
            }
          }
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
          int32_t value = Operand<int8_t>(pc_);
          Leaf(kAstI32, BUILD(Int32Constant, value));
          len = 2;
          break;
        }
        case kExprI32Const: {
          int32_t value = Operand<int32_t>(pc_);
          Leaf(kAstI32, BUILD(Int32Constant, value));
          len = 5;
          break;
        }
        case kExprI64Const: {
          int64_t value = Operand<int64_t>(pc_);
          Leaf(kAstI64, BUILD(Int64Constant, value));
          len = 9;
          break;
        }
        case kExprF32Const: {
          float value = Operand<float>(pc_);
          Leaf(kAstF32, BUILD(Float32Constant, value));
          len = 5;
          break;
        }
        case kExprF64Const: {
          double value = Operand<double>(pc_);
          Leaf(kAstF64, BUILD(Float64Constant, value));
          len = 9;
          break;
        }
        case kExprGetLocal: {
          uint32_t index;
          LocalType type = LocalOperand(pc_, &index, &len);
          TFNode* val =
              build() && type != kAstStmt ? ssa_env_->locals[index] : nullptr;
          Leaf(type, val);
          break;
        }
        case kExprSetLocal: {
          uint32_t index;
          LocalType type = LocalOperand(pc_, &index, &len);
          Shift(type, 1);
          break;
        }
        case kExprLoadGlobal: {
          uint32_t index;
          LocalType type = GlobalOperand(pc_, &index, &len);
          Leaf(type, BUILD(LoadGlobal, index));
          break;
        }
        case kExprStoreGlobal: {
          uint32_t index;
          LocalType type = GlobalOperand(pc_, &index, &len);
          Shift(type, 1);
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
          uint32_t unused;
          FunctionSig* sig = FunctionSigOperand(pc_, &unused, &len);
          if (sig) {
            LocalType type =
                sig->return_count() == 0 ? kAstStmt : sig->GetReturn();
            Shift(type, static_cast<int>(sig->parameter_count()));
          } else {
            Leaf(kAstI32);  // error
          }
          break;
        }
        case kExprCallIndirect: {
          uint32_t unused;
          FunctionSig* sig = SigOperand(pc_, &unused, &len);
          if (sig) {
            LocalType type =
                sig->return_count() == 0 ? kAstStmt : sig->GetReturn();
            Shift(type, static_cast<int>(1 + sig->parameter_count()));
          } else {
            Leaf(kAstI32);  // error
          }
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
    int length = 2;
    uint32_t offset;
    MemoryAccessOperand(pc, &length, &offset);
    Shift(type, 1);
    return length;
  }

  int DecodeStoreMem(const byte* pc, LocalType type) {
    int length = 2;
    uint32_t offset;
    MemoryAccessOperand(pc, &length, &offset);
    Shift(type, 2);
    return length;
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
          // Condition done.
          TypeCheckLast(p, kAstI32);
        } else if (p->index == 2) {
          // True expression done.
          p->tree->type = p->last()->type;
          if (p->tree->type == kAstStmt) {
            error(p->pc(), p->tree->children[1]->pc,
                  "select operand should be expression");
          }
        } else {
          // False expression done.
          DCHECK(p->done());
          TypeCheckLast(p, p->tree->type);
          if (build()) {
            TFNode* controls[2];
            builder_->Branch(p->tree->children[0]->node, &controls[0],
                             &controls[1]);
            TFNode* merge = builder_->Merge(2, controls);
            TFNode* vals[2] = {p->tree->children[1]->node,
                               p->tree->children[2]->node};
            TFNode* phi = builder_->Phi(p->tree->type, 2, vals, merge);
            p->tree->node = phi;
            ssa_env_->control = merge;
          }
        }
        break;
      }
      case kExprBr: {
        uint32_t depth = Operand<uint8_t>(p->pc());
        if (depth >= blocks_.size()) {
          error("improperly nested branch");
          break;
        }
        Block* block = &blocks_[blocks_.size() - depth - 1];
        ReduceBreakToExprBlock(p, block);
        break;
      }
      case kExprBrIf: {
        if (p->index == 1) {
          TypeCheckLast(p, kAstI32);
        } else if (p->done()) {
          uint32_t depth = Operand<uint8_t>(p->pc());
          if (depth >= blocks_.size()) {
            error("improperly nested branch");
            break;
          }
          Block* block = &blocks_[blocks_.size() - depth - 1];
          SsaEnv* fenv = ssa_env_;
          SsaEnv* tenv = Split(fenv);
          BUILD(Branch, p->tree->children[0]->node, &tenv->control,
                &fenv->control);
          ssa_env_ = tenv;
          ReduceBreakToExprBlock(p, block);
          ssa_env_ = fenv;
        }
        break;
      }
      case kExprTableSwitch: {
        uint16_t table_count = *reinterpret_cast<const uint16_t*>(p->pc() + 3);
        if (table_count == 1) {
          // Degenerate switch with only a default target.
          if (p->index == 1) {
            SsaEnv* break_env = ssa_env_;
            PushBlock(break_env);
            SetEnv("switch:default", Steal(break_env));
          }
          if (p->done()) {
            Block* block = &blocks_.back();
            // fall through to the end.
            ReduceBreakToExprBlock(p, block);
            SetEnv("switch:end", block->ssa_env);
            blocks_.pop_back();
          }
          break;
        }

        if (p->index == 1) {
          // Switch key finished.
          TypeCheckLast(p, kAstI32);

          TFNode* sw = BUILD(Switch, table_count, p->last()->node);

          // Allocate environments for each case.
          uint16_t case_count = *reinterpret_cast<const uint16_t*>(p->pc() + 1);
          SsaEnv** case_envs = zone_->NewArray<SsaEnv*>(case_count);
          for (int i = 0; i < case_count; i++) {
            case_envs[i] = UnreachableEnv();
          }

          ifs_.push_back({nullptr, nullptr, case_envs});
          SsaEnv* break_env = ssa_env_;
          PushBlock(break_env);
          SsaEnv* copy = Steal(break_env);
          ssa_env_ = copy;

          // Build the environments for each case based on the table.
          const uint16_t* table =
              reinterpret_cast<const uint16_t*>(p->pc() + 5);
          for (int i = 0; i < table_count; i++) {
            uint16_t target = table[i];
            SsaEnv* env = Split(copy);
            env->control = (i == table_count - 1) ? BUILD(IfDefault, sw)
                                                  : BUILD(IfValue, i, sw);
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

          // Switch to the environment for the first case.
          SetEnv("switch:case", case_envs[0]);
        } else {
          // Switch case finished.
          if (p->done()) {
            // Last case. Fall through to the end.
            Block* block = &blocks_.back();
            ReduceBreakToExprBlock(p, block);
            SsaEnv* next = block->ssa_env;
            blocks_.pop_back();
            ifs_.pop_back();
            SetEnv("switch:end", next);
          } else {
            // Interior case. Maybe fall through to the next case.
            SsaEnv* next = ifs_.back().case_envs[p->index - 1];
            if (ssa_env_->go()) Goto(ssa_env_, next);
            SetEnv("switch:case", next);
          }
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
        int unused = 0;
        uint32_t index;
        LocalType type = LocalOperand(p->pc(), &index, &unused);
        Tree* val = p->last();
        if (type == val->type) {
          if (build()) ssa_env_->locals[index] = val->node;
          p->tree->node = val->node;
        } else {
          error(p->pc(), val->pc, "Typecheck failed in SetLocal");
        }
        break;
      }
      case kExprStoreGlobal: {
        int unused = 0;
        uint32_t index;
        LocalType type = GlobalOperand(p->pc(), &index, &unused);
        Tree* val = p->last();
        if (type == val->type) {
          BUILD(StoreGlobal, index, val->node);
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
        int len;
        uint32_t index;
        FunctionSig* sig = FunctionSigOperand(p->pc(), &index, &len);
        if (!sig) break;
        if (p->index > 0) {
          TypeCheckLast(p, sig->GetParam(p->index - 1));
        }
        if (p->done() && build()) {
          uint32_t count = p->tree->count + 1;
          TFNode** buffer = builder_->Buffer(count);
          FunctionSig* sig = FunctionSigOperand(p->pc(), &index, &len);
          USE(sig);
          buffer[0] = nullptr;  // reserved for code object.
          for (uint32_t i = 1; i < count; i++) {
            buffer[i] = p->tree->children[i - 1]->node;
          }
          p->tree->node = builder_->CallDirect(index, buffer);
        }
        break;
      }
      case kExprCallIndirect: {
        int len;
        uint32_t index;
        FunctionSig* sig = SigOperand(p->pc(), &index, &len);
        if (p->index == 1) {
          TypeCheckLast(p, kAstI32);
        } else {
          TypeCheckLast(p, sig->GetParam(p->index - 2));
        }
        if (p->done() && build()) {
          uint32_t count = p->tree->count;
          TFNode** buffer = builder_->Buffer(count);
          for (uint32_t i = 0; i < count; i++) {
            buffer[i] = p->tree->children[i]->node;
          }
          p->tree->node = builder_->CallIndirect(index, buffer);
        }
        break;
      }
      default:
        break;
    }
  }

  void ReduceBreakToExprBlock(Production* p, Block* block) {
    if (block->stack_depth < 0) {
      // This is the inner loop block, which does not have a value.
      Goto(ssa_env_, block->ssa_env);
    } else {
      // Merge the value into the production for the block.
      Production* bp = &stack_[block->stack_depth];
      MergeIntoProduction(bp, block->ssa_env, p->last());
    }
  }

  void MergeIntoProduction(Production* p, SsaEnv* target, Tree* expr) {
    if (!ssa_env_->go()) return;

    bool first = target->state == SsaEnv::kUnreachable;
    Goto(ssa_env_, target);
    if (expr->type == kAstEnd) return;

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
      int length = 0;
      uint32_t offset = 0;
      MemoryAccessOperand(p->pc(), &length, &offset);
      p->tree->node =
          builder_->LoadMem(type, mem_type, p->last()->node, offset);
    }
  }

  void ReduceStoreMem(Production* p, LocalType type, MachineType mem_type) {
    if (p->index == 1) {
      TypeCheckLast(p, kAstI32);  // index
    } else {
      DCHECK_EQ(2, p->index);
      TypeCheckLast(p, type);
      if (build()) {
        int length = 0;
        uint32_t offset = 0;
        MemoryAccessOperand(p->pc(), &length, &offset);
        TFNode* val = p->tree->children[1]->node;
        builder_->StoreMem(mem_type, p->tree->children[0]->node, offset, val);
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
    if (env->control != nullptr && FLAG_trace_wasm_decoder) {
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

  // Load an operand at [pc + 1].
  template <typename V>
  V Operand(const byte* pc) {
    if ((limit_ - pc) < static_cast<int>(1 + sizeof(V))) {
      const char* msg = "Expected operand following opcode";
      switch (sizeof(V)) {
        case 1:
          msg = "Expected 1-byte operand following opcode";
          break;
        case 2:
          msg = "Expected 2-byte operand following opcode";
          break;
        case 4:
          msg = "Expected 4-byte operand following opcode";
          break;
        default:
          break;
      }
      error(pc, msg);
      return -1;
    }
    return *reinterpret_cast<const V*>(pc + 1);
  }

  int EnvironmentCount() {
    if (builder_) return static_cast<int>(function_env_->GetLocalCount());
    return 0;  // if we aren't building a graph, don't bother with SSA renaming.
  }

  LocalType LocalOperand(const byte* pc, uint32_t* index, int* length) {
    *index = UnsignedLEB128Operand(pc, length);
    if (function_env_->IsValidLocal(*index)) {
      return function_env_->GetLocalType(*index);
    }
    error(pc, "invalid local variable index");
    return kAstStmt;
  }

  LocalType GlobalOperand(const byte* pc, uint32_t* index, int* length) {
    *index = UnsignedLEB128Operand(pc, length);
    if (function_env_->module->IsValidGlobal(*index)) {
      return WasmOpcodes::LocalTypeFor(
          function_env_->module->GetGlobalType(*index));
    }
    error(pc, "invalid global variable index");
    return kAstStmt;
  }

  FunctionSig* FunctionSigOperand(const byte* pc, uint32_t* index,
                                  int* length) {
    *index = UnsignedLEB128Operand(pc, length);
    if (function_env_->module->IsValidFunction(*index)) {
      return function_env_->module->GetFunctionSignature(*index);
    }
    error(pc, "invalid function index");
    return nullptr;
  }

  FunctionSig* SigOperand(const byte* pc, uint32_t* index, int* length) {
    *index = UnsignedLEB128Operand(pc, length);
    if (function_env_->module->IsValidSignature(*index)) {
      return function_env_->module->GetSignature(*index);
    }
    error(pc, "invalid signature index");
    return nullptr;
  }

  uint32_t UnsignedLEB128Operand(const byte* pc, int* length) {
    uint32_t result = 0;
    ReadUnsignedLEB128ErrorCode error_code =
        ReadUnsignedLEB128Operand(pc + 1, limit_, length, &result);
    if (error_code == kInvalidLEB128) error(pc, "invalid LEB128 varint");
    if (error_code == kMissingLEB128) error(pc, "expected LEB128 varint");
    (*length)++;
    return result;
  }

  void MemoryAccessOperand(const byte* pc, int* length, uint32_t* offset) {
    byte bitfield = Operand<uint8_t>(pc);
    if (MemoryAccess::OffsetField::decode(bitfield)) {
      *offset = UnsignedLEB128Operand(pc + 1, length);
      (*length)++;  // to account for the memory access byte
    } else {
      *offset = 0;
      *length = 2;
    }
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
  *result = 0;
  const byte* ptr = pc;
  const byte* end = pc + 5;  // maximum 5 bytes.
  if (end > limit) end = limit;
  int shift = 0;
  byte b = 0;
  while (ptr < end) {
    b = *ptr++;
    *result = *result | ((b & 0x7F) << shift);
    if ((b & 0x80) == 0) break;
    shift += 7;
  }
  DCHECK_LE(ptr - pc, 5);
  *length = static_cast<int>(ptr - pc);
  if (ptr == end && (b & 0x80)) {
    return kInvalidLEB128;
  } else if (*length == 0) {
    return kMissingLEB128;
  } else {
    return kNoError;
  }
}


int OpcodeLength(const byte* pc) {
  switch (static_cast<WasmOpcode>(*pc)) {
#define DECLARE_OPCODE_CASE(name, opcode, sig) case kExpr##name:
    FOREACH_LOAD_MEM_OPCODE(DECLARE_OPCODE_CASE)
    FOREACH_STORE_MEM_OPCODE(DECLARE_OPCODE_CASE)
#undef DECLARE_OPCODE_CASE

    case kExprI8Const:
    case kExprBlock:
    case kExprLoop:
    case kExprBr:
    case kExprBrIf:
      return 2;
    case kExprI32Const:
    case kExprF32Const:
      return 5;
    case kExprI64Const:
    case kExprF64Const:
      return 9;
    case kExprStoreGlobal:
    case kExprSetLocal:
    case kExprLoadGlobal:
    case kExprCallFunction:
    case kExprCallIndirect:
    case kExprGetLocal: {
      int length;
      uint32_t result = 0;
      ReadUnsignedLEB128Operand(pc + 1, pc + 6, &length, &result);
      return 1 + length;
    }
    case kExprTableSwitch: {
      uint16_t table_count = *reinterpret_cast<const uint16_t*>(pc + 3);
      return 5 + table_count * 2;
    }

    default:
      return 1;
  }
}


int OpcodeArity(FunctionEnv* env, const byte* pc) {
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
    case kExprLoop:
      return *(pc + 1);

    case kExprCallFunction: {
      int index = *(pc + 1);
      return static_cast<int>(
          env->module->GetFunctionSignature(index)->parameter_count());
    }
    case kExprCallIndirect: {
      int index = *(pc + 1);
      return 1 + static_cast<int>(
                     env->module->GetSignature(index)->parameter_count());
    }
    case kExprReturn:
      return static_cast<int>(env->sig->return_count());
    case kExprTableSwitch: {
      uint16_t case_count = *reinterpret_cast<const uint16_t*>(pc + 1);
      return 1 + case_count;
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
}  // namespace wasm
}  // namespace internal
}  // namespace v8
