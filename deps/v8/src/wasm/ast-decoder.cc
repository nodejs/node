// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/signature.h"

#include "src/bit-vector.h"
#include "src/flags.h"
#include "src/handles.h"
#include "src/zone-containers.h"

#include "src/wasm/ast-decoder.h"
#include "src/wasm/decoder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes.h"

#include "src/ostreams.h"

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

#define CHECK_PROTOTYPE_OPCODE(flag)                   \
  if (!FLAG_##flag) {                                  \
    error("Invalid opcode (enable with --" #flag ")"); \
    break;                                             \
  }

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
  void SetNotMerged() {
    if (state == kMerged) state = kReached;
  }
};

// An entry on the value stack.
struct Value {
  const byte* pc;
  TFNode* node;
  LocalType type;
};

struct Control;

// IncomingBranch is used by exception handling code for managing finally's.
struct IncomingBranch {
  int32_t token_value;
  Control* target;
  Value val;
};

// Auxiliary data for exception handling. Most scopes don't need any of this so
// we group everything into a separate struct.
struct TryInfo : public ZoneObject {
  SsaEnv* catch_env;       // catch environment (only for try with catch).
  SsaEnv* finish_try_env;  // the environment where a try with finally lives.
  ZoneVector<IncomingBranch> incoming_branches;
  TFNode* token;
  bool has_handled_finally;

  TryInfo(Zone* zone, SsaEnv* c, SsaEnv* f)
      : catch_env(c),
        finish_try_env(f),
        incoming_branches(zone),
        token(nullptr),
        has_handled_finally(false) {}
};

// An entry on the control stack (i.e. if, block, loop).
struct Control {
  const byte* pc;
  int stack_depth;       // stack height at the beginning of the construct.
  SsaEnv* end_env;       // end environment for the construct.
  SsaEnv* false_env;     // false environment (only for if).
  TryInfo* try_info;     // exception handling stuff. See TryInfo above.
  int32_t prev_finally;  // previous control (on stack) that has a finally.
  TFNode* node;          // result node for the construct.
  LocalType type;        // result type for the construct.
  bool is_loop;          // true if this is the inner label of a loop.

  bool is_if() const { return *pc == kExprIf; }

  bool is_try() const {
    return *pc == kExprTryCatch || *pc == kExprTryCatchFinally ||
           *pc == kExprTryFinally;
  }

  bool has_catch() const {
    return *pc == kExprTryCatch || *pc == kExprTryCatchFinally;
  }

  bool has_finally() const {
    return *pc == kExprTryCatchFinally || *pc == kExprTryFinally;
  }

  // Named constructors.
  static Control Block(const byte* pc, int stack_depth,
                       int32_t most_recent_finally, SsaEnv* end_env) {
    return {pc,      stack_depth, end_env,
            nullptr, nullptr,     most_recent_finally,
            nullptr, kAstEnd,     false};
  }

  static Control If(const byte* pc, int stack_depth,
                    int32_t most_recent_finally, SsaEnv* end_env,
                    SsaEnv* false_env) {
    return {pc,        stack_depth, end_env,
            false_env, nullptr,     most_recent_finally,
            nullptr,   kAstStmt,    false};
  }

  static Control Loop(const byte* pc, int stack_depth,
                      int32_t most_recent_finally, SsaEnv* end_env) {
    return {pc,      stack_depth, end_env,
            nullptr, nullptr,     most_recent_finally,
            nullptr, kAstEnd,     true};
  }

  static Control Try(const byte* pc, int stack_depth,
                     int32_t most_recent_finally, Zone* zone, SsaEnv* end_env,
                     SsaEnv* catch_env, SsaEnv* finish_try_env) {
    return {pc,
            stack_depth,
            end_env,
            nullptr,
            new (zone) TryInfo(zone, catch_env, finish_try_env),
            most_recent_finally,
            nullptr,
            kAstEnd,
            false};
  }
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
  WasmDecoder(ModuleEnv* module, FunctionSig* sig, const byte* start,
              const byte* end)
      : Decoder(start, end),
        module_(module),
        sig_(sig),
        total_locals_(0),
        local_types_(nullptr) {}
  ModuleEnv* module_;
  FunctionSig* sig_;
  size_t total_locals_;
  ZoneVector<LocalType>* local_types_;

  inline bool Validate(const byte* pc, LocalIndexOperand& operand) {
    if (operand.index < total_locals_) {
      if (local_types_) {
        operand.type = local_types_->at(operand.index);
      } else {
        operand.type = kAstStmt;
      }
      return true;
    }
    error(pc, pc + 1, "invalid local index");
    return false;
  }

  inline bool Validate(const byte* pc, GlobalIndexOperand& operand) {
    ModuleEnv* m = module_;
    if (m && m->module && operand.index < m->module->globals.size()) {
      operand.type = m->module->globals[operand.index].type;
      return true;
    }
    error(pc, pc + 1, "invalid global index");
    return false;
  }

  inline bool Complete(const byte* pc, CallFunctionOperand& operand) {
    ModuleEnv* m = module_;
    if (m && m->module && operand.index < m->module->functions.size()) {
      operand.sig = m->module->functions[operand.index].sig;
      return true;
    }
    return false;
  }

  inline bool Validate(const byte* pc, CallFunctionOperand& operand) {
    if (Complete(pc, operand)) {
      uint32_t expected = static_cast<uint32_t>(operand.sig->parameter_count());
      if (operand.arity != expected) {
        error(pc, pc + 1,
              "arity mismatch in direct function call (expected %u, got %u)",
              expected, operand.arity);
        return false;
      }
      return true;
    }
    error(pc, pc + 1, "invalid function index");
    return false;
  }

  inline bool Complete(const byte* pc, CallIndirectOperand& operand) {
    ModuleEnv* m = module_;
    if (m && m->module && operand.index < m->module->signatures.size()) {
      operand.sig = m->module->signatures[operand.index];
      return true;
    }
    return false;
  }

  inline bool Validate(const byte* pc, CallIndirectOperand& operand) {
    if (Complete(pc, operand)) {
      uint32_t expected = static_cast<uint32_t>(operand.sig->parameter_count());
      if (operand.arity != expected) {
        error(pc, pc + 1,
              "arity mismatch in indirect function call (expected %u, got %u)",
              expected, operand.arity);
        return false;
      }
      return true;
    }
    error(pc, pc + 1, "invalid signature index");
    return false;
  }

  inline bool Complete(const byte* pc, CallImportOperand& operand) {
    ModuleEnv* m = module_;
    if (m && m->module && operand.index < m->module->import_table.size()) {
      operand.sig = m->module->import_table[operand.index].sig;
      return true;
    }
    return false;
  }

  inline bool Validate(const byte* pc, CallImportOperand& operand) {
    if (Complete(pc, operand)) {
      uint32_t expected = static_cast<uint32_t>(operand.sig->parameter_count());
      if (operand.arity != expected) {
        error(pc, pc + 1, "arity mismatch in import call (expected %u, got %u)",
              expected, operand.arity);
        return false;
      }
      return true;
    }
    error(pc, pc + 1, "invalid signature index");
    return false;
  }

  inline bool Validate(const byte* pc, BreakDepthOperand& operand,
                       ZoneVector<Control>& control) {
    if (operand.arity > 1) {
      error(pc, pc + 1, "invalid arity for br or br_if");
      return false;
    }
    if (operand.depth < control.size()) {
      operand.target = &control[control.size() - operand.depth - 1];
      return true;
    }
    error(pc, pc + 1, "invalid break depth");
    return false;
  }

  bool Validate(const byte* pc, BranchTableOperand& operand,
                size_t block_depth) {
    if (operand.arity > 1) {
      error(pc, pc + 1, "invalid arity for break");
      return false;
    }
    // Verify table.
    for (uint32_t i = 0; i < operand.table_count + 1; ++i) {
      uint32_t target = operand.read_entry(this, i);
      if (target >= block_depth) {
        error(operand.table + i * 2, "improper branch in br_table");
        return false;
      }
    }
    return true;
  }

  unsigned OpcodeArity(const byte* pc) {
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
      case kExprGetGlobal:
      case kExprNop:
      case kExprUnreachable:
      case kExprEnd:
      case kExprBlock:
      case kExprThrow:
      case kExprTryCatch:
      case kExprTryCatchFinally:
      case kExprTryFinally:
      case kExprFinally:
      case kExprLoop:
        return 0;

      case kExprSetGlobal:
      case kExprSetLocal:
      case kExprElse:
      case kExprCatch:
        return 1;

      case kExprBr: {
        BreakDepthOperand operand(this, pc);
        return operand.arity;
      }
      case kExprBrIf: {
        BreakDepthOperand operand(this, pc);
        return 1 + operand.arity;
      }
      case kExprBrTable: {
        BranchTableOperand operand(this, pc);
        return 1 + operand.arity;
      }

      case kExprIf:
        return 1;
      case kExprSelect:
        return 3;

      case kExprCallFunction: {
        CallFunctionOperand operand(this, pc);
        return operand.arity;
      }
      case kExprCallIndirect: {
        CallIndirectOperand operand(this, pc);
        return 1 + operand.arity;
      }
      case kExprCallImport: {
        CallImportOperand operand(this, pc);
        return operand.arity;
      }
      case kExprReturn: {
        ReturnArityOperand operand(this, pc);
        return operand.arity;
      }

#define DECLARE_OPCODE_CASE(name, opcode, sig) \
  case kExpr##name:                            \
    return kArity_##sig;

        FOREACH_LOAD_MEM_OPCODE(DECLARE_OPCODE_CASE)
        FOREACH_STORE_MEM_OPCODE(DECLARE_OPCODE_CASE)
        FOREACH_MISC_MEM_OPCODE(DECLARE_OPCODE_CASE)
        FOREACH_SIMPLE_OPCODE(DECLARE_OPCODE_CASE)
        FOREACH_SIMPLE_MEM_OPCODE(DECLARE_OPCODE_CASE)
        FOREACH_ASMJS_COMPAT_OPCODE(DECLARE_OPCODE_CASE)
        FOREACH_SIMD_OPCODE(DECLARE_OPCODE_CASE)
#undef DECLARE_OPCODE_CASE
      default:
        UNREACHABLE();
        return 0;
    }
  }

  unsigned OpcodeLength(const byte* pc) {
    switch (static_cast<WasmOpcode>(*pc)) {
#define DECLARE_OPCODE_CASE(name, opcode, sig) case kExpr##name:
      FOREACH_LOAD_MEM_OPCODE(DECLARE_OPCODE_CASE)
      FOREACH_STORE_MEM_OPCODE(DECLARE_OPCODE_CASE)
#undef DECLARE_OPCODE_CASE
      {
        MemoryAccessOperand operand(this, pc);
        return 1 + operand.length;
      }
      case kExprBr:
      case kExprBrIf: {
        BreakDepthOperand operand(this, pc);
        return 1 + operand.length;
      }
      case kExprSetGlobal:
      case kExprGetGlobal: {
        GlobalIndexOperand operand(this, pc);
        return 1 + operand.length;
      }

      case kExprCallFunction: {
        CallFunctionOperand operand(this, pc);
        return 1 + operand.length;
      }
      case kExprCallIndirect: {
        CallIndirectOperand operand(this, pc);
        return 1 + operand.length;
      }
      case kExprCallImport: {
        CallImportOperand operand(this, pc);
        return 1 + operand.length;
      }

      case kExprSetLocal:
      case kExprGetLocal:
      case kExprCatch: {
        LocalIndexOperand operand(this, pc);
        return 1 + operand.length;
      }
      case kExprBrTable: {
        BranchTableOperand operand(this, pc);
        return 1 + operand.length;
      }
      case kExprI32Const: {
        ImmI32Operand operand(this, pc);
        return 1 + operand.length;
      }
      case kExprI64Const: {
        ImmI64Operand operand(this, pc);
        return 1 + operand.length;
      }
      case kExprI8Const:
        return 2;
      case kExprF32Const:
        return 5;
      case kExprF64Const:
        return 9;
      case kExprReturn: {
        ReturnArityOperand operand(this, pc);
        return 1 + operand.length;
      }

      default:
        return 1;
    }
  }
};

static const int32_t kFirstFinallyToken = 1;
static const int32_t kFallthroughToken = 0;
static const int32_t kNullFinallyToken = -1;

// The full WASM decoder for bytecode. Both verifies bytecode and generates
// a TurboFan IR graph.
class WasmFullDecoder : public WasmDecoder {
 public:
  WasmFullDecoder(Zone* zone, TFBuilder* builder, const FunctionBody& body)
      : WasmDecoder(body.module, body.sig, body.start, body.end),
        zone_(zone),
        builder_(builder),
        base_(body.base),
        local_type_vec_(zone),
        stack_(zone),
        control_(zone),
        most_recent_finally_(-1),
        finally_token_val_(kFirstFinallyToken) {
    local_types_ = &local_type_vec_;
  }

  bool Decode() {
    base::ElapsedTimer decode_timer;
    if (FLAG_trace_wasm_decode_time) {
      decode_timer.Start();
    }
    stack_.clear();
    control_.clear();

    if (end_ < pc_) {
      error(pc_, "function body end < start");
      return false;
    }

    DecodeLocalDecls();
    InitSsaEnv();
    DecodeFunctionBody();

    if (failed()) return TraceFailed();

    if (!control_.empty()) {
      error(pc_, control_.back().pc, "unterminated control structure");
      return TraceFailed();
    }

    if (ssa_env_->go()) {
      TRACE("  @%-6d #xx:%-20s|", startrel(pc_), "ImplicitReturn");
      DoReturn();
      if (failed()) return TraceFailed();
      TRACE("\n");
    }

    if (FLAG_trace_wasm_decode_time) {
      double ms = decode_timer.Elapsed().InMillisecondsF();
      PrintF("wasm-decode ok (%0.3f ms)\n\n", ms);
    } else {
      TRACE("wasm-decode ok\n\n");
    }

    return true;
  }

  bool TraceFailed() {
    TRACE("wasm-error module+%-6d func+%d: %s\n\n", baserel(error_pc_),
          startrel(error_pc_), error_msg_.get());
    return false;
  }

  bool DecodeLocalDecls(AstLocalDecls& decls) {
    DecodeLocalDecls();
    if (failed()) return false;
    decls.decls_encoded_size = pc_offset();
    decls.local_types.reserve(local_type_vec_.size());
    for (size_t pos = 0; pos < local_type_vec_.size();) {
      uint32_t count = 0;
      LocalType type = local_type_vec_[pos];
      while (pos < local_type_vec_.size() && local_type_vec_[pos] == type) {
        pos++;
        count++;
      }
      decls.local_types.push_back(std::pair<LocalType, uint32_t>(type, count));
    }
    decls.total_local_count = static_cast<uint32_t>(local_type_vec_.size());
    return true;
  }

  BitVector* AnalyzeLoopAssignmentForTesting(const byte* pc,
                                             size_t num_locals) {
    total_locals_ = num_locals;
    local_type_vec_.reserve(num_locals);
    if (num_locals > local_type_vec_.size()) {
      local_type_vec_.insert(local_type_vec_.end(),
                             num_locals - local_type_vec_.size(), kAstI32);
    }
    return AnalyzeLoopAssignment(pc);
  }

 private:
  static const size_t kErrorMsgSize = 128;

  Zone* zone_;
  TFBuilder* builder_;
  const byte* base_;

  SsaEnv* ssa_env_;

  ZoneVector<LocalType> local_type_vec_;  // types of local variables.
  ZoneVector<Value> stack_;               // stack of values.
  ZoneVector<Control> control_;           // stack of blocks, loops, and ifs.

  int32_t most_recent_finally_;
  int32_t finally_token_val_;

  int32_t FallthroughTokenForFinally() {
    // Any number < kFirstFinallyToken would work.
    return kFallthroughToken;
  }

  int32_t NewTokenForFinally() { return finally_token_val_++; }

  inline bool build() { return builder_ && ssa_env_->go(); }

  void InitSsaEnv() {
    TFNode* start = nullptr;
    SsaEnv* ssa_env = reinterpret_cast<SsaEnv*>(zone_->New(sizeof(SsaEnv)));
    size_t size = sizeof(TFNode*) * EnvironmentCount();
    ssa_env->state = SsaEnv::kReached;
    ssa_env->locals =
        size > 0 ? reinterpret_cast<TFNode**>(zone_->New(size)) : nullptr;

    if (builder_) {
      start = builder_->Start(static_cast<int>(sig_->parameter_count() + 1));
      // Initialize local variables.
      uint32_t index = 0;
      while (index < sig_->parameter_count()) {
        ssa_env->locals[index] = builder_->Param(index, local_type_vec_[index]);
        index++;
      }
      while (index < local_type_vec_.size()) {
        LocalType type = local_type_vec_[index];
        TFNode* node = DefaultValue(type);
        while (index < local_type_vec_.size() &&
               local_type_vec_[index] == type) {
          // Do a whole run of like-typed locals at a time.
          ssa_env->locals[index++] = node;
        }
      }
      builder_->set_module(module_);
    }
    ssa_env->control = start;
    ssa_env->effect = start;
    SetEnv("initial", ssa_env);
    if (builder_) {
      builder_->StackCheck(position());
    }
  }

  TFNode* DefaultValue(LocalType type) {
    switch (type) {
      case kAstI32:
        return builder_->Int32Constant(0);
      case kAstI64:
        return builder_->Int64Constant(0);
      case kAstF32:
        return builder_->Float32Constant(0);
      case kAstF64:
        return builder_->Float64Constant(0);
      case kAstS128:
        return builder_->DefaultS128Value();
      default:
        UNREACHABLE();
        return nullptr;
    }
  }

  char* indentation() {
    static const int kMaxIndent = 64;
    static char bytes[kMaxIndent + 1];
    for (int i = 0; i < kMaxIndent; ++i) bytes[i] = ' ';
    bytes[kMaxIndent] = 0;
    if (stack_.size() < kMaxIndent / 2) {
      bytes[stack_.size() * 2] = 0;
    }
    return bytes;
  }

  // Decodes the locals declarations, if any, populating {local_type_vec_}.
  void DecodeLocalDecls() {
    DCHECK_EQ(0, local_type_vec_.size());
    // Initialize {local_type_vec} from signature.
    if (sig_) {
      local_type_vec_.reserve(sig_->parameter_count());
      for (size_t i = 0; i < sig_->parameter_count(); ++i) {
        local_type_vec_.push_back(sig_->GetParam(i));
      }
    }
    // Decode local declarations, if any.
    uint32_t entries = consume_u32v("local decls count");
    while (entries-- > 0 && pc_ < limit_) {
      uint32_t count = consume_u32v("local count");
      byte code = consume_u8("local type");
      LocalType type;
      switch (code) {
        case kLocalI32:
          type = kAstI32;
          break;
        case kLocalI64:
          type = kAstI64;
          break;
        case kLocalF32:
          type = kAstF32;
          break;
        case kLocalF64:
          type = kAstF64;
          break;
        case kLocalS128:
          type = kAstS128;
          break;
        default:
          error(pc_ - 1, "invalid local type");
          return;
      }
      local_type_vec_.insert(local_type_vec_.end(), count, type);
    }
    total_locals_ = local_type_vec_.size();
  }

  // Decodes the body of a function.
  void DecodeFunctionBody() {
    TRACE("wasm-decode %p...%p (module+%d, %d bytes) %s\n",
          reinterpret_cast<const void*>(start_),
          reinterpret_cast<const void*>(limit_), baserel(pc_),
          static_cast<int>(limit_ - start_), builder_ ? "graph building" : "");

    if (pc_ >= limit_) return;  // Nothing to do.

    while (true) {  // decoding loop.
      unsigned len = 1;
      WasmOpcode opcode = static_cast<WasmOpcode>(*pc_);
      TRACE("  @%-6d #%02x:%-20s|", startrel(pc_), opcode,
            WasmOpcodes::ShortOpcodeName(opcode));

      FunctionSig* sig = WasmOpcodes::Signature(opcode);
      if (sig) {
        // Fast case of a simple operator.
        TFNode* node;
        switch (sig->parameter_count()) {
          case 1: {
            Value val = Pop(0, sig->GetParam(0));
            node = BUILD(Unop, opcode, val.node, position());
            break;
          }
          case 2: {
            Value rval = Pop(1, sig->GetParam(1));
            Value lval = Pop(0, sig->GetParam(0));
            node = BUILD(Binop, opcode, lval.node, rval.node, position());
            break;
          }
          default:
            UNREACHABLE();
            node = nullptr;
            break;
        }
        Push(GetReturnType(sig), node);
      } else {
        // Complex bytecode.
        switch (opcode) {
          case kExprNop:
            Push(kAstStmt, nullptr);
            break;
          case kExprBlock: {
            // The break environment is the outer environment.
            SsaEnv* break_env = ssa_env_;
            PushBlock(break_env);
            SetEnv("block:start", Steal(break_env));
            break;
          }
          case kExprThrow: {
            CHECK_PROTOTYPE_OPCODE(wasm_eh_prototype);
            Pop(0, kAstI32);

            // TODO(jpp): start exception propagation.
            break;
          }
          case kExprTryCatch: {
            CHECK_PROTOTYPE_OPCODE(wasm_eh_prototype);
            SsaEnv* outer_env = ssa_env_;
            SsaEnv* try_env = Steal(outer_env);
            SsaEnv* catch_env = Split(try_env);
            PushTry(outer_env, catch_env, nullptr);
            SetEnv("try_catch:start", try_env);
            break;
          }
          case kExprTryCatchFinally: {
            CHECK_PROTOTYPE_OPCODE(wasm_eh_prototype);
            SsaEnv* outer_env = ssa_env_;
            SsaEnv* try_env = Steal(outer_env);
            SsaEnv* catch_env = Split(try_env);
            SsaEnv* finally_env = Split(try_env);
            PushTry(finally_env, catch_env, outer_env);
            SetEnv("try_catch_finally:start", try_env);
            break;
          }
          case kExprTryFinally: {
            CHECK_PROTOTYPE_OPCODE(wasm_eh_prototype);
            SsaEnv* outer_env = ssa_env_;
            SsaEnv* try_env = Steal(outer_env);
            SsaEnv* finally_env = Split(outer_env);
            PushTry(finally_env, nullptr, outer_env);
            SetEnv("try_finally:start", try_env);
            break;
          }
          case kExprCatch: {
            CHECK_PROTOTYPE_OPCODE(wasm_eh_prototype);
            LocalIndexOperand operand(this, pc_);
            len = 1 + operand.length;

            if (control_.empty()) {
              error(pc_, "catch does not match a any try");
              break;
            }

            Control* c = &control_.back();
            if (!c->has_catch()) {
              error(pc_, "catch does not match a try with catch");
              break;
            }

            if (c->try_info->catch_env == nullptr) {
              error(pc_, "catch already present for try with catch");
              break;
            }

            Goto(ssa_env_, c->end_env);

            SsaEnv* catch_env = c->try_info->catch_env;
            c->try_info->catch_env = nullptr;
            SetEnv("catch:begin", catch_env);

            if (Validate(pc_, operand)) {
              // TODO(jpp): figure out how thrown value is propagated. It is
              // unlikely to be a value on the stack.
              if (ssa_env_->locals) {
                ssa_env_->locals[operand.index] = nullptr;
              }
            }

            PopUpTo(c->stack_depth);

            break;
          }
          case kExprFinally: {
            CHECK_PROTOTYPE_OPCODE(wasm_eh_prototype);
            if (control_.empty()) {
              error(pc_, "finally does not match a any try");
              break;
            }

            Control* c = &control_.back();
            if (c->has_catch() && c->try_info->catch_env != nullptr) {
              error(pc_, "missing catch for try with catch and finally");
              break;
            }

            if (!c->has_finally()) {
              error(pc_, "finally does not match a try with finally");
              break;
            }

            if (c->try_info->finish_try_env == nullptr) {
              error(pc_, "finally already present for try with finally");
              break;
            }

            // ssa_env_ is either the env for either the try or the catch, but
            // it does not matter: either way we need to direct the control flow
            // to the end_env, which is the env for the finally.
            // c->try_info->finish_try_env is the the environment enclosing the
            // try block.
            const bool has_fallthrough = ssa_env_->go();
            Value val = PopUpTo(c->stack_depth);
            if (has_fallthrough) {
              MergeInto(c->end_env, &c->node, &c->type, val);
              MergeFinallyToken(ssa_env_, c, FallthroughTokenForFinally());
            }

            // The current environment becomes end_env, and finish_try_env
            // becomes the new end_env. This ensures that any control flow
            // leaving a try block up to now will do so by branching to the
            // finally block. Setting the end_env to be finish_try_env ensures
            // that kExprEnd below can handle the try block as it would any
            // other block construct.
            SsaEnv* finally_env = c->end_env;
            c->end_env = c->try_info->finish_try_env;
            SetEnv("finally:begin", finally_env);
            c->try_info->finish_try_env = nullptr;

            if (has_fallthrough) {
              LocalType c_type = c->type;
              if (c->node == nullptr) {
                c_type = kAstStmt;
              }
              Push(c_type, c->node);
            }

            c->try_info->has_handled_finally = true;
            // There's no more need to keep the current control scope in the
            // finally chain as no more predecessors will be added to c.
            most_recent_finally_ = c->prev_finally;
            break;
          }
          case kExprLoop: {
            // The break environment is the outer environment.
            SsaEnv* break_env = ssa_env_;
            PushBlock(break_env);
            SsaEnv* finish_try_env = Steal(break_env);
            // The continue environment is the inner environment.
            PrepareForLoop(pc_, finish_try_env);
            SetEnv("loop:start", Split(finish_try_env));
            ssa_env_->SetNotMerged();
            PushLoop(finish_try_env);
            break;
          }
          case kExprIf: {
            // Condition on top of stack. Split environments for branches.
            Value cond = Pop(0, kAstI32);
            TFNode* if_true = nullptr;
            TFNode* if_false = nullptr;
            BUILD(Branch, cond.node, &if_true, &if_false);
            SsaEnv* end_env = ssa_env_;
            SsaEnv* false_env = Split(ssa_env_);
            false_env->control = if_false;
            SsaEnv* true_env = Steal(ssa_env_);
            true_env->control = if_true;
            PushIf(end_env, false_env);
            SetEnv("if:true", true_env);
            break;
          }
          case kExprElse: {
            if (control_.empty()) {
              error(pc_, "else does not match any if");
              break;
            }
            Control* c = &control_.back();
            if (!c->is_if()) {
              error(pc_, c->pc, "else does not match an if");
              break;
            }
            if (c->false_env == nullptr) {
              error(pc_, c->pc, "else already present for if");
              break;
            }
            Value val = PopUpTo(c->stack_depth);
            MergeInto(c->end_env, &c->node, &c->type, val);
            // Switch to environment for false branch.
            SetEnv("if_else:false", c->false_env);
            c->false_env = nullptr;  // record that an else is already seen
            break;
          }
          case kExprEnd: {
            if (control_.empty()) {
              error(pc_, "end does not match any if, try, or block");
              break;
            }
            const char* name = "block:end";
            Control* c = &control_.back();
            Value val = PopUpTo(c->stack_depth);
            if (c->is_loop) {
              // Loops always push control in pairs.
              PopControl();
              c = &control_.back();
              name = "loop:end";
            } else if (c->is_if()) {
              if (c->false_env != nullptr) {
                // End the true branch of a one-armed if.
                Goto(c->false_env, c->end_env);
                val = {val.pc, nullptr, kAstStmt};
                name = "if:merge";
              } else {
                // End the false branch of a two-armed if.
                name = "if_else:merge";
              }
            } else if (c->is_try()) {
              name = "try:end";

              // validate that catch/finally were seen.
              if (c->try_info->catch_env != nullptr) {
                error(pc_, "missing catch in try with catch");
                break;
              }

              if (c->try_info->finish_try_env != nullptr) {
                error(pc_, "missing finally in try with finally");
                break;
              }

              if (c->has_finally() && ssa_env_->go()) {
                DispatchToTargets(c, val);
              }
            }

            if (ssa_env_->go()) {
              // Adds a fallthrough edge to the next control block.
              MergeInto(c->end_env, &c->node, &c->type, val);
            }
            SetEnv(name, c->end_env);
            stack_.resize(c->stack_depth);
            Push(c->type, c->node);
            PopControl();
            break;
          }
          case kExprSelect: {
            Value cond = Pop(2, kAstI32);
            Value fval = Pop();
            Value tval = Pop();
            if (tval.type == kAstStmt || tval.type != fval.type) {
              if (tval.type != kAstEnd && fval.type != kAstEnd) {
                error(pc_, "type mismatch in select");
                break;
              }
            }
            if (build()) {
              DCHECK(tval.type != kAstEnd);
              DCHECK(fval.type != kAstEnd);
              DCHECK(cond.type != kAstEnd);
              TFNode* controls[2];
              builder_->Branch(cond.node, &controls[0], &controls[1]);
              TFNode* merge = builder_->Merge(2, controls);
              TFNode* vals[2] = {tval.node, fval.node};
              TFNode* phi = builder_->Phi(tval.type, 2, vals, merge);
              Push(tval.type, phi);
              ssa_env_->control = merge;
            } else {
              Push(tval.type, nullptr);
            }
            break;
          }
          case kExprBr: {
            BreakDepthOperand operand(this, pc_);
            Value val = {pc_, nullptr, kAstStmt};
            if (operand.arity) val = Pop();
            if (Validate(pc_, operand, control_)) {
              BreakTo(operand, val);
            }
            len = 1 + operand.length;
            Push(kAstEnd, nullptr);
            break;
          }
          case kExprBrIf: {
            BreakDepthOperand operand(this, pc_);
            Value cond = Pop(operand.arity, kAstI32);
            Value val = {pc_, nullptr, kAstStmt};
            if (operand.arity == 1) val = Pop();
            if (Validate(pc_, operand, control_)) {
              SsaEnv* fenv = ssa_env_;
              SsaEnv* tenv = Split(fenv);
              fenv->SetNotMerged();
              BUILD(Branch, cond.node, &tenv->control, &fenv->control);
              ssa_env_ = tenv;
              BreakTo(operand, val);
              ssa_env_ = fenv;
            }
            len = 1 + operand.length;
            Push(kAstStmt, nullptr);
            break;
          }
          case kExprBrTable: {
            BranchTableOperand operand(this, pc_);
            if (Validate(pc_, operand, control_.size())) {
              Value key = Pop(operand.arity, kAstI32);
              Value val = {pc_, nullptr, kAstStmt};
              if (operand.arity == 1) val = Pop();
              if (failed()) break;

              SsaEnv* break_env = ssa_env_;
              if (operand.table_count > 0) {
                // Build branches to the various blocks based on the table.
                TFNode* sw = BUILD(Switch, operand.table_count + 1, key.node);

                SsaEnv* copy = Steal(break_env);
                ssa_env_ = copy;
                for (uint32_t i = 0; i < operand.table_count + 1; ++i) {
                  uint16_t target = operand.read_entry(this, i);
                  ssa_env_ = Split(copy);
                  ssa_env_->control = (i == operand.table_count)
                                          ? BUILD(IfDefault, sw)
                                          : BUILD(IfValue, i, sw);
                  int depth = target;
                  Control* c = &control_[control_.size() - depth - 1];
                  MergeInto(c->end_env, &c->node, &c->type, val);
                }
              } else {
                // Only a default target. Do the equivalent of br.
                uint16_t target = operand.read_entry(this, 0);
                int depth = target;
                Control* c = &control_[control_.size() - depth - 1];
                MergeInto(c->end_env, &c->node, &c->type, val);
              }
              // br_table ends the control flow like br.
              ssa_env_ = break_env;
              Push(kAstStmt, nullptr);
            }
            len = 1 + operand.length;
            break;
          }
          case kExprReturn: {
            ReturnArityOperand operand(this, pc_);
            if (operand.arity != sig_->return_count()) {
              error(pc_, pc_ + 1, "arity mismatch in return");
            }
            DoReturn();
            len = 1 + operand.length;
            break;
          }
          case kExprUnreachable: {
            Push(kAstEnd, BUILD(Unreachable, position()));
            ssa_env_->Kill(SsaEnv::kControlEnd);
            break;
          }
          case kExprI8Const: {
            ImmI8Operand operand(this, pc_);
            Push(kAstI32, BUILD(Int32Constant, operand.value));
            len = 1 + operand.length;
            break;
          }
          case kExprI32Const: {
            ImmI32Operand operand(this, pc_);
            Push(kAstI32, BUILD(Int32Constant, operand.value));
            len = 1 + operand.length;
            break;
          }
          case kExprI64Const: {
            ImmI64Operand operand(this, pc_);
            Push(kAstI64, BUILD(Int64Constant, operand.value));
            len = 1 + operand.length;
            break;
          }
          case kExprF32Const: {
            ImmF32Operand operand(this, pc_);
            Push(kAstF32, BUILD(Float32Constant, operand.value));
            len = 1 + operand.length;
            break;
          }
          case kExprF64Const: {
            ImmF64Operand operand(this, pc_);
            Push(kAstF64, BUILD(Float64Constant, operand.value));
            len = 1 + operand.length;
            break;
          }
          case kExprGetLocal: {
            LocalIndexOperand operand(this, pc_);
            if (Validate(pc_, operand)) {
              if (build()) {
                Push(operand.type, ssa_env_->locals[operand.index]);
              } else {
                Push(operand.type, nullptr);
              }
            }
            len = 1 + operand.length;
            break;
          }
          case kExprSetLocal: {
            LocalIndexOperand operand(this, pc_);
            if (Validate(pc_, operand)) {
              Value val = Pop(0, local_type_vec_[operand.index]);
              if (ssa_env_->locals) ssa_env_->locals[operand.index] = val.node;
              Push(val.type, val.node);
            }
            len = 1 + operand.length;
            break;
          }
          case kExprGetGlobal: {
            GlobalIndexOperand operand(this, pc_);
            if (Validate(pc_, operand)) {
              Push(operand.type, BUILD(GetGlobal, operand.index));
            }
            len = 1 + operand.length;
            break;
          }
          case kExprSetGlobal: {
            GlobalIndexOperand operand(this, pc_);
            if (Validate(pc_, operand)) {
              Value val = Pop(0, operand.type);
              BUILD(SetGlobal, operand.index, val.node);
              Push(val.type, val.node);
            }
            len = 1 + operand.length;
            break;
          }
          case kExprI32LoadMem8S:
            len = DecodeLoadMem(kAstI32, MachineType::Int8());
            break;
          case kExprI32LoadMem8U:
            len = DecodeLoadMem(kAstI32, MachineType::Uint8());
            break;
          case kExprI32LoadMem16S:
            len = DecodeLoadMem(kAstI32, MachineType::Int16());
            break;
          case kExprI32LoadMem16U:
            len = DecodeLoadMem(kAstI32, MachineType::Uint16());
            break;
          case kExprI32LoadMem:
            len = DecodeLoadMem(kAstI32, MachineType::Int32());
            break;

          case kExprI64LoadMem8S:
            len = DecodeLoadMem(kAstI64, MachineType::Int8());
            break;
          case kExprI64LoadMem8U:
            len = DecodeLoadMem(kAstI64, MachineType::Uint8());
            break;
          case kExprI64LoadMem16S:
            len = DecodeLoadMem(kAstI64, MachineType::Int16());
            break;
          case kExprI64LoadMem16U:
            len = DecodeLoadMem(kAstI64, MachineType::Uint16());
            break;
          case kExprI64LoadMem32S:
            len = DecodeLoadMem(kAstI64, MachineType::Int32());
            break;
          case kExprI64LoadMem32U:
            len = DecodeLoadMem(kAstI64, MachineType::Uint32());
            break;
          case kExprI64LoadMem:
            len = DecodeLoadMem(kAstI64, MachineType::Int64());
            break;
          case kExprF32LoadMem:
            len = DecodeLoadMem(kAstF32, MachineType::Float32());
            break;
          case kExprF64LoadMem:
            len = DecodeLoadMem(kAstF64, MachineType::Float64());
            break;
          case kExprI32StoreMem8:
            len = DecodeStoreMem(kAstI32, MachineType::Int8());
            break;
          case kExprI32StoreMem16:
            len = DecodeStoreMem(kAstI32, MachineType::Int16());
            break;
          case kExprI32StoreMem:
            len = DecodeStoreMem(kAstI32, MachineType::Int32());
            break;
          case kExprI64StoreMem8:
            len = DecodeStoreMem(kAstI64, MachineType::Int8());
            break;
          case kExprI64StoreMem16:
            len = DecodeStoreMem(kAstI64, MachineType::Int16());
            break;
          case kExprI64StoreMem32:
            len = DecodeStoreMem(kAstI64, MachineType::Int32());
            break;
          case kExprI64StoreMem:
            len = DecodeStoreMem(kAstI64, MachineType::Int64());
            break;
          case kExprF32StoreMem:
            len = DecodeStoreMem(kAstF32, MachineType::Float32());
            break;
          case kExprF64StoreMem:
            len = DecodeStoreMem(kAstF64, MachineType::Float64());
            break;

          case kExprMemorySize:
            Push(kAstI32, BUILD(MemSize, 0));
            break;
          case kExprCallFunction: {
            CallFunctionOperand operand(this, pc_);
            if (Validate(pc_, operand)) {
              TFNode** buffer = PopArgs(operand.sig);
              TFNode* call =
                  BUILD(CallDirect, operand.index, buffer, position());
              Push(GetReturnType(operand.sig), call);
            }
            len = 1 + operand.length;
            break;
          }
          case kExprCallIndirect: {
            CallIndirectOperand operand(this, pc_);
            if (Validate(pc_, operand)) {
              TFNode** buffer = PopArgs(operand.sig);
              Value index = Pop(0, kAstI32);
              if (buffer) buffer[0] = index.node;
              TFNode* call =
                  BUILD(CallIndirect, operand.index, buffer, position());
              Push(GetReturnType(operand.sig), call);
            }
            len = 1 + operand.length;
            break;
          }
          case kExprCallImport: {
            CallImportOperand operand(this, pc_);
            if (Validate(pc_, operand)) {
              TFNode** buffer = PopArgs(operand.sig);
              TFNode* call =
                  BUILD(CallImport, operand.index, buffer, position());
              Push(GetReturnType(operand.sig), call);
            }
            len = 1 + operand.length;
            break;
          }
          case kSimdPrefix: {
            CHECK_PROTOTYPE_OPCODE(wasm_simd_prototype);
            len++;
            byte simd_index = *(pc_ + 1);
            opcode = static_cast<WasmOpcode>(opcode << 8 | simd_index);
            DecodeSimdOpcode(opcode);
            break;
          }
          default:
            error("Invalid opcode");
            return;
        }
      }  // end complex bytecode

#if DEBUG
      if (FLAG_trace_wasm_decoder) {
        for (size_t i = 0; i < stack_.size(); ++i) {
          Value& val = stack_[i];
          WasmOpcode opcode = static_cast<WasmOpcode>(*val.pc);
          PrintF(" %c@%d:%s", WasmOpcodes::ShortNameOf(val.type),
                 static_cast<int>(val.pc - start_),
                 WasmOpcodes::ShortOpcodeName(opcode));
          switch (opcode) {
            case kExprI32Const: {
              ImmI32Operand operand(this, val.pc);
              PrintF("[%d]", operand.value);
              break;
            }
            case kExprGetLocal: {
              LocalIndexOperand operand(this, val.pc);
              PrintF("[%u]", operand.index);
              break;
            }
            case kExprSetLocal: {
              LocalIndexOperand operand(this, val.pc);
              PrintF("[%u]", operand.index);
              break;
            }
            default:
              break;
          }
        }
        PrintF("\n");
      }
#endif
      pc_ += len;
      if (pc_ >= limit_) {
        // End of code reached or exceeded.
        if (pc_ > limit_ && ok()) error("Beyond end of code");
        return;
      }
    }  // end decode loop
  }    // end DecodeFunctionBody()

  TFNode** PopArgs(FunctionSig* sig) {
    if (build()) {
      int count = static_cast<int>(sig->parameter_count());
      TFNode** buffer = builder_->Buffer(count + 1);
      buffer[0] = nullptr;  // reserved for code object or function index.
      for (int i = count - 1; i >= 0; i--) {
        buffer[i + 1] = Pop(i, sig->GetParam(i)).node;
      }
      return buffer;
    } else {
      int count = static_cast<int>(sig->parameter_count());
      for (int i = count - 1; i >= 0; i--) {
        Pop(i, sig->GetParam(i));
      }
      return nullptr;
    }
  }

  LocalType GetReturnType(FunctionSig* sig) {
    return sig->return_count() == 0 ? kAstStmt : sig->GetReturn();
  }

  void PushBlock(SsaEnv* end_env) {
    const int stack_depth = static_cast<int>(stack_.size());
    control_.emplace_back(
        Control::Block(pc_, stack_depth, most_recent_finally_, end_env));
  }

  void PushLoop(SsaEnv* end_env) {
    const int stack_depth = static_cast<int>(stack_.size());
    control_.emplace_back(
        Control::Loop(pc_, stack_depth, most_recent_finally_, end_env));
  }

  void PushIf(SsaEnv* end_env, SsaEnv* false_env) {
    const int stack_depth = static_cast<int>(stack_.size());
    control_.emplace_back(Control::If(pc_, stack_depth, most_recent_finally_,
                                      end_env, false_env));
  }

  void PushTry(SsaEnv* end_env, SsaEnv* catch_env, SsaEnv* finish_try_env) {
    const int stack_depth = static_cast<int>(stack_.size());
    control_.emplace_back(Control::Try(pc_, stack_depth, most_recent_finally_,
                                       zone_, end_env, catch_env,
                                       finish_try_env));
    if (control_.back().has_finally()) {
      most_recent_finally_ = static_cast<uint32_t>(control_.size() - 1);
    }
  }

  void PopControl() {
    const Control& c = control_.back();
    most_recent_finally_ = c.prev_finally;
    control_.pop_back();
    // No more accesses to (danging pointer) c
  }

  int DecodeLoadMem(LocalType type, MachineType mem_type) {
    MemoryAccessOperand operand(this, pc_);
    Value index = Pop(0, kAstI32);
    TFNode* node = BUILD(LoadMem, type, mem_type, index.node, operand.offset,
                         operand.alignment, position());
    Push(type, node);
    return 1 + operand.length;
  }

  int DecodeStoreMem(LocalType type, MachineType mem_type) {
    MemoryAccessOperand operand(this, pc_);
    Value val = Pop(1, type);
    Value index = Pop(0, kAstI32);
    BUILD(StoreMem, mem_type, index.node, operand.offset, operand.alignment,
          val.node, position());
    Push(type, val.node);
    return 1 + operand.length;
  }

  void DecodeSimdOpcode(WasmOpcode opcode) {
    FunctionSig* sig = WasmOpcodes::Signature(opcode);
    compiler::NodeVector inputs(sig->parameter_count(), zone_);
    for (size_t i = sig->parameter_count(); i > 0; i--) {
      Value val = Pop(static_cast<int>(i - 1), sig->GetParam(i - 1));
      inputs[i - 1] = val.node;
    }
    TFNode* node = BUILD(SimdOp, opcode, inputs);
    Push(GetReturnType(sig), node);
  }

  void DispatchToTargets(Control* next_block, const Value& val) {
    const ZoneVector<IncomingBranch>& incoming_branches =
        next_block->try_info->incoming_branches;
    // Counts how many successors are not current control block.
    uint32_t targets = 0;
    for (const auto& path_token : incoming_branches) {
      if (path_token.target != next_block) ++targets;
    }

    if (targets == 0) {
      // Nothing to do here: the control flow should just fall
      // through to the next control block.
      return;
    }

    TFNode* sw = BUILD(Switch, static_cast<uint32_t>(targets + 1),
                       next_block->try_info->token);

    SsaEnv* break_env = ssa_env_;
    SsaEnv* copy = Steal(break_env);
    for (uint32_t ii = 0; ii < incoming_branches.size(); ++ii) {
      Control* t = incoming_branches[ii].target;
      if (t != next_block) {
        const int32_t token_value = incoming_branches[ii].token_value;
        ssa_env_ = Split(copy);
        ssa_env_->control = BUILD(IfValue, token_value, sw);
        MergeInto(t->end_env, &t->node, &t->type, incoming_branches[ii].val);
        // We only need to merge the finally token if t is both a
        // try-with-finally and its finally hasn't yet been found
        // in the instruction stream. Otherwise we just need to
        // branch to t.
        if (t->has_finally() && !t->try_info->has_handled_finally) {
          MergeFinallyToken(ssa_env_, t, token_value);
        }
      }
    }
    ssa_env_ = Split(copy);
    ssa_env_->control = BUILD(IfDefault, sw);
    MergeInto(next_block->end_env, &next_block->node, &next_block->type, val);
    // Not a finally env; no fallthrough token.

    ssa_env_ = break_env;
  }

  void DoReturn() {
    int count = static_cast<int>(sig_->return_count());
    TFNode** buffer = nullptr;
    if (build()) buffer = builder_->Buffer(count);

    // Pop return values off the stack in reverse order.
    for (int i = count - 1; i >= 0; i--) {
      Value val = Pop(i, sig_->GetReturn(i));
      if (buffer) buffer[i] = val.node;
    }

    Push(kAstEnd, BUILD(Return, count, buffer));
    ssa_env_->Kill(SsaEnv::kControlEnd);
  }

  void Push(LocalType type, TFNode* node) {
    stack_.push_back({pc_, node, type});
  }

  const char* SafeOpcodeNameAt(const byte* pc) {
    if (pc >= end_) return "<end>";
    return WasmOpcodes::ShortOpcodeName(static_cast<WasmOpcode>(*pc));
  }

  Value Pop(int index, LocalType expected) {
    Value val = Pop();
    if (val.type != expected) {
      if (val.type != kAstEnd) {
        error(pc_, val.pc, "%s[%d] expected type %s, found %s of type %s",
              SafeOpcodeNameAt(pc_), index, WasmOpcodes::TypeName(expected),
              SafeOpcodeNameAt(val.pc), WasmOpcodes::TypeName(val.type));
      }
    }
    return val;
  }

  Value Pop() {
    size_t limit = control_.empty() ? 0 : control_.back().stack_depth;
    if (stack_.size() <= limit) {
      Value val = {pc_, nullptr, kAstStmt};
      error(pc_, pc_, "%s found empty stack", SafeOpcodeNameAt(pc_));
      return val;
    }
    Value val = stack_.back();
    stack_.pop_back();
    return val;
  }

  Value PopUpTo(int stack_depth) {
    if (stack_depth == stack_.size()) {
      Value val = {pc_, nullptr, kAstStmt};
      return val;
    } else {
      DCHECK_LE(stack_depth, static_cast<int>(stack_.size()));
      Value val = Pop();
      stack_.resize(stack_depth);
      return val;
    }
  }

  int baserel(const byte* ptr) {
    return base_ ? static_cast<int>(ptr - base_) : 0;
  }

  int startrel(const byte* ptr) { return static_cast<int>(ptr - start_); }

  Control* BuildFinallyChain(const BreakDepthOperand& operand, const Value& val,
                             int32_t* token) {
    DCHECK_LE(operand.depth, control_.size());
    const int32_t target_index =
        static_cast<uint32_t>(control_.size() - operand.depth - 1);

    if (most_recent_finally_ == kNullFinallyToken ||  // No finallies.
        most_recent_finally_ < target_index) {  // Does not cross any finally.
      *token = kNullFinallyToken;
      return operand.target;
    }

    Control* previous_control = &control_[most_recent_finally_];
    *token = NewTokenForFinally();

    for (int32_t ii = previous_control->prev_finally; ii >= target_index;
         ii = previous_control->prev_finally) {
      Control* current_finally = &control_[ii];

      DCHECK(!current_finally->try_info->has_handled_finally);
      previous_control->try_info->incoming_branches.push_back(
          {*token, current_finally, val});
      previous_control = current_finally;
    }

    if (operand.target != previous_control) {
      DCHECK_NOT_NULL(previous_control);
      DCHECK(previous_control->has_finally());
      DCHECK_NE(*token, kNullFinallyToken);
      previous_control->try_info->incoming_branches.push_back(
          {*token, operand.target, val});
    }

    return &control_[most_recent_finally_];
  }

  void BreakTo(const BreakDepthOperand& operand, const Value& val) {
    int32_t finally_token;
    Control* block = BuildFinallyChain(operand, val, &finally_token);

    if (block->is_loop) {
      // This is the inner loop block, which does not have a value.
      Goto(ssa_env_, block->end_env);
    } else {
      // Merge the value into the production for the block.
      MergeInto(block->end_env, &block->node, &block->type, val);
    }

    if (finally_token != kNullFinallyToken) {
      MergeFinallyToken(ssa_env_, block, finally_token);
    }
  }

  void MergeInto(SsaEnv* target, TFNode** node, LocalType* type,
                 const Value& val) {
    if (!ssa_env_->go()) return;
    DCHECK_NE(kAstEnd, val.type);

    bool first = target->state == SsaEnv::kUnreachable;
    Goto(ssa_env_, target);

    if (first) {
      // first merge to this environment; set the type and the node.
      *type = val.type;
      *node = val.node;
    } else if (val.type == *type && val.type != kAstStmt) {
      // merge with the existing value for this block.
      *node = CreateOrMergeIntoPhi(*type, target->control, *node, val.node);
    } else {
      // types don't match, or block is already a stmt.
      *type = kAstStmt;
      *node = nullptr;
    }
  }

  void SetEnv(const char* reason, SsaEnv* env) {
#if DEBUG
    if (FLAG_trace_wasm_decoder) {
      char state = 'X';
      if (env) {
        switch (env->state) {
          case SsaEnv::kReached:
            state = 'R';
            break;
          case SsaEnv::kUnreachable:
            state = 'U';
            break;
          case SsaEnv::kMerged:
            state = 'M';
            break;
          case SsaEnv::kControlEnd:
            state = 'E';
            break;
        }
      }
      PrintF("  env = %p, state = %c, reason = %s", static_cast<void*>(env),
             state, reason);
      if (env && env->control) {
        PrintF(", control = ");
        compiler::WasmGraphBuilder::PrintDebugName(env->control);
      }
      PrintF("\n");
    }
#endif
    ssa_env_ = env;
    if (builder_) {
      builder_->set_control_ptr(&env->control);
      builder_->set_effect_ptr(&env->effect);
    }
  }

  void MergeFinallyToken(SsaEnv*, Control* to, int32_t new_token) {
    DCHECK(to->has_finally());
    DCHECK(!to->try_info->has_handled_finally);
    if (builder_ == nullptr) {
      return;
    }

    switch (to->end_env->state) {
      case SsaEnv::kReached:
        DCHECK(to->try_info->token == nullptr);
        to->try_info->token = builder_->Int32Constant(new_token);
        break;
      case SsaEnv::kMerged:
        DCHECK_NOT_NULL(to->try_info->token);
        to->try_info->token = CreateOrMergeIntoPhi(
            kAstI32, to->end_env->control, to->try_info->token,
            builder_->Int32Constant(new_token));
        break;
      case SsaEnv::kUnreachable:
        UNREACHABLE();
      // fallthrough intended.
      default:
        break;
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
            to->locals[i] = builder_->Phi(local_type_vec_[i], 2, vals, merge);
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
          builder_->AppendToPhi(to->effect, from->effect);
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
            builder_->AppendToPhi(tnode, fnode);
          } else if (tnode != fnode) {
            uint32_t count = builder_->InputCount(merge);
            TFNode** vals = builder_->Buffer(count);
            for (uint32_t j = 0; j < count - 1; j++) {
              vals[j] = tnode;
            }
            vals[count - 1] = fnode;
            to->locals[i] =
                builder_->Phi(local_type_vec_[i], count, vals, merge);
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
      builder_->AppendToPhi(tnode, fnode);
    } else if (tnode != fnode) {
      uint32_t count = builder_->InputCount(merge);
      TFNode** vals = builder_->Buffer(count);
      for (uint32_t j = 0; j < count - 1; j++) vals[j] = tnode;
      vals[count - 1] = fnode;
      return builder_->Phi(type, count, vals, merge);
    }
    return tnode;
  }

  void PrepareForLoop(const byte* pc, SsaEnv* env) {
    if (!env->go()) return;
    env->state = SsaEnv::kMerged;
    if (!builder_) return;

    env->control = builder_->Loop(env->control);
    env->effect = builder_->EffectPhi(1, &env->effect, env->control);
    builder_->Terminate(env->effect, env->control);
    if (FLAG_wasm_loop_assignment_analysis) {
      BitVector* assigned = AnalyzeLoopAssignment(pc);
      if (assigned != nullptr) {
        // Only introduce phis for variables assigned in this loop.
        for (int i = EnvironmentCount() - 1; i >= 0; i--) {
          if (!assigned->Contains(i)) continue;
          env->locals[i] = builder_->Phi(local_type_vec_[i], 1, &env->locals[i],
                                         env->control);
        }
        return;
      }
    }

    // Conservatively introduce phis for all local variables.
    for (int i = EnvironmentCount() - 1; i >= 0; i--) {
      env->locals[i] =
          builder_->Phi(local_type_vec_[i], 1, &env->locals[i], env->control);
    }
  }

  // Create a complete copy of the {from}.
  SsaEnv* Split(SsaEnv* from) {
    DCHECK_NOT_NULL(from);
    SsaEnv* result = reinterpret_cast<SsaEnv*>(zone_->New(sizeof(SsaEnv)));
    size_t size = sizeof(TFNode*) * EnvironmentCount();
    result->control = from->control;
    result->effect = from->effect;

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
    if (builder_) return static_cast<int>(local_type_vec_.size());
    return 0;  // if we aren't building a graph, don't bother with SSA renaming.
  }

  virtual void onFirstError() {
    limit_ = start_;     // Terminate decoding loop.
    builder_ = nullptr;  // Don't build any more nodes.
    TRACE(" !%s\n", error_msg_.get());
  }
  BitVector* AnalyzeLoopAssignment(const byte* pc) {
    if (pc >= limit_) return nullptr;
    if (*pc != kExprLoop) return nullptr;

    BitVector* assigned =
        new (zone_) BitVector(static_cast<int>(local_type_vec_.size()), zone_);
    int depth = 0;
    // Iteratively process all AST nodes nested inside the loop.
    while (pc < limit_ && ok()) {
      WasmOpcode opcode = static_cast<WasmOpcode>(*pc);
      unsigned length = 1;
      switch (opcode) {
        case kExprLoop:
        case kExprIf:
        case kExprBlock:
        case kExprTryCatch:
        case kExprTryCatchFinally:
        case kExprTryFinally:
          depth++;
          DCHECK_EQ(1, OpcodeLength(pc));
          break;
        case kExprSetLocal: {
          LocalIndexOperand operand(this, pc);
          if (assigned->length() > 0 &&
              static_cast<int>(operand.index) < assigned->length()) {
            // Unverified code might have an out-of-bounds index.
            assigned->Add(operand.index);
          }
          length = 1 + operand.length;
          break;
        }
        case kExprEnd:
          depth--;
          break;
        default:
          length = OpcodeLength(pc);
          break;
      }
      if (depth <= 0) break;
      pc += length;
    }
    return ok() ? assigned : nullptr;
  }

  inline wasm::WasmCodePosition position() {
    int offset = static_cast<int>(pc_ - start_);
    DCHECK_EQ(pc_ - start_, offset);  // overflows cannot happen
    return offset;
  }
};

bool DecodeLocalDecls(AstLocalDecls& decls, const byte* start,
                      const byte* end) {
  base::AccountingAllocator allocator;
  Zone tmp(&allocator);
  FunctionBody body = {nullptr, nullptr, nullptr, start, end};
  WasmFullDecoder decoder(&tmp, nullptr, body);
  return decoder.DecodeLocalDecls(decls);
}

BytecodeIterator::BytecodeIterator(const byte* start, const byte* end,
                                   AstLocalDecls* decls)
    : Decoder(start, end) {
  if (decls != nullptr) {
    if (DecodeLocalDecls(*decls, start, end)) {
      pc_ += decls->decls_encoded_size;
      if (pc_ > end_) pc_ = end_;
    }
  }
}

DecodeResult VerifyWasmCode(base::AccountingAllocator* allocator,
                            FunctionBody& body) {
  Zone zone(allocator);
  WasmFullDecoder decoder(&zone, nullptr, body);
  decoder.Decode();
  return decoder.toResult<DecodeStruct*>(nullptr);
}

DecodeResult BuildTFGraph(base::AccountingAllocator* allocator,
                          TFBuilder* builder, FunctionBody& body) {
  Zone zone(allocator);
  WasmFullDecoder decoder(&zone, builder, body);
  decoder.Decode();
  return decoder.toResult<DecodeStruct*>(nullptr);
}

unsigned OpcodeLength(const byte* pc, const byte* end) {
  WasmDecoder decoder(nullptr, nullptr, pc, end);
  return decoder.OpcodeLength(pc);
}

unsigned OpcodeArity(const byte* pc, const byte* end) {
  WasmDecoder decoder(nullptr, nullptr, pc, end);
  return decoder.OpcodeArity(pc);
}

void PrintAstForDebugging(const byte* start, const byte* end) {
  base::AccountingAllocator allocator;
  OFStream os(stdout);
  PrintAst(&allocator, FunctionBodyForTesting(start, end), os, nullptr);
}

bool PrintAst(base::AccountingAllocator* allocator, const FunctionBody& body,
              std::ostream& os,
              std::vector<std::tuple<uint32_t, int, int>>* offset_table) {
  Zone zone(allocator);
  WasmFullDecoder decoder(&zone, nullptr, body);
  int line_nr = 0;

  // Print the function signature.
  if (body.sig) {
    os << "// signature: " << *body.sig << std::endl;
    ++line_nr;
  }

  // Print the local declarations.
  AstLocalDecls decls(&zone);
  BytecodeIterator i(body.start, body.end, &decls);
  if (body.start != i.pc()) {
    os << "// locals: ";
    for (auto p : decls.local_types) {
      LocalType type = p.first;
      uint32_t count = p.second;
      os << " " << count << " " << WasmOpcodes::TypeName(type);
    }
    os << std::endl;
    ++line_nr;

    for (const byte* locals = body.start; locals < i.pc(); locals++) {
      os << (locals == body.start ? "0x" : " 0x") << AsHex(*locals, 2) << ",";
    }
    os << std::endl;
    ++line_nr;
  }

  os << "// body: " << std::endl;
  ++line_nr;
  unsigned control_depth = 0;
  for (; i.has_next(); i.next()) {
    unsigned length = decoder.OpcodeLength(i.pc());

    WasmOpcode opcode = i.current();
    if (opcode == kExprElse) control_depth--;

    int num_whitespaces = control_depth < 32 ? 2 * control_depth : 64;
    if (offset_table) {
      offset_table->push_back(
          std::make_tuple(i.pc_offset(), line_nr, num_whitespaces));
    }

    // 64 whitespaces
    const char* padding =
        "                                                                ";
    os.write(padding, num_whitespaces);
    os << "k" << WasmOpcodes::OpcodeName(opcode) << ",";

    for (size_t j = 1; j < length; ++j) {
      os << " " << AsHex(i.pc()[j], 2) << ",";
    }

    switch (opcode) {
      case kExprIf:
      case kExprElse:
      case kExprLoop:
      case kExprBlock:
      case kExprTryCatch:
      case kExprTryCatchFinally:
      case kExprTryFinally:
        os << "   // @" << i.pc_offset();
        control_depth++;
        break;
      case kExprEnd:
        os << "   // @" << i.pc_offset();
        control_depth--;
        break;
      case kExprBr: {
        BreakDepthOperand operand(&i, i.pc());
        os << "   // arity=" << operand.arity << " depth=" << operand.depth;
        break;
      }
      case kExprBrIf: {
        BreakDepthOperand operand(&i, i.pc());
        os << "   // arity=" << operand.arity << " depth" << operand.depth;
        break;
      }
      case kExprBrTable: {
        BranchTableOperand operand(&i, i.pc());
        os << "   // arity=" << operand.arity
           << " entries=" << operand.table_count;
        break;
      }
      case kExprCallIndirect: {
        CallIndirectOperand operand(&i, i.pc());
        if (decoder.Complete(i.pc(), operand)) {
          os << "   // sig #" << operand.index << ": " << *operand.sig;
        } else {
          os << " // arity=" << operand.arity << " sig #" << operand.index;
        }
        break;
      }
      case kExprCallImport: {
        CallImportOperand operand(&i, i.pc());
        if (decoder.Complete(i.pc(), operand)) {
          os << "   // import #" << operand.index << ": " << *operand.sig;
        } else {
          os << " // arity=" << operand.arity << " import #" << operand.index;
        }
        break;
      }
      case kExprCallFunction: {
        CallFunctionOperand operand(&i, i.pc());
        if (decoder.Complete(i.pc(), operand)) {
          os << "   // function #" << operand.index << ": " << *operand.sig;
        } else {
          os << " // arity=" << operand.arity << " function #" << operand.index;
        }
        break;
      }
      case kExprReturn: {
        ReturnArityOperand operand(&i, i.pc());
        os << "   // arity=" << operand.arity;
        break;
      }
      default:
        break;
      }
    os << std::endl;
    ++line_nr;
  }

  return decoder.ok();
}

BitVector* AnalyzeLoopAssignmentForTesting(Zone* zone, size_t num_locals,
                                           const byte* start, const byte* end) {
  FunctionBody body = {nullptr, nullptr, nullptr, start, end};
  WasmFullDecoder decoder(zone, nullptr, body);
  return decoder.AnalyzeLoopAssignmentForTesting(start, num_locals);
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
