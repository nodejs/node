// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/signature.h"

#include "src/bit-vector.h"
#include "src/flags.h"
#include "src/handles.h"
#include "src/zone/zone-containers.h"

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
// TODO(titzer): this is only for intermediate migration.
#define IMPLICIT_FUNCTION_END 1

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

struct TryInfo : public ZoneObject {
  SsaEnv* catch_env;
  TFNode* exception;

  explicit TryInfo(SsaEnv* c) : catch_env(c), exception(nullptr) {}
};

struct MergeValues {
  uint32_t arity;
  union {
    Value* array;
    Value first;
  } vals;  // Either multiple values or a single value.

  Value& first() {
    DCHECK_GT(arity, 0u);
    return arity == 1 ? vals.first : vals.array[0];
  }
};

static Value* NO_VALUE = nullptr;

enum ControlKind { kControlIf, kControlBlock, kControlLoop, kControlTry };

// An entry on the control stack (i.e. if, block, loop).
struct Control {
  const byte* pc;
  ControlKind kind;
  int stack_depth;    // stack height at the beginning of the construct.
  SsaEnv* end_env;    // end environment for the construct.
  SsaEnv* false_env;  // false environment (only for if).
  TryInfo* try_info;  // Information used for compiling try statements.
  int32_t previous_catch;  // The previous Control (on the stack) with a catch.

  // Values merged into the end of this control construct.
  MergeValues merge;

  inline bool is_if() const { return kind == kControlIf; }
  inline bool is_block() const { return kind == kControlBlock; }
  inline bool is_loop() const { return kind == kControlLoop; }
  inline bool is_try() const { return kind == kControlTry; }

  // Named constructors.
  static Control Block(const byte* pc, int stack_depth, SsaEnv* end_env,
                       int32_t previous_catch) {
    return {pc,      kControlBlock, stack_depth,    end_env,
            nullptr, nullptr,       previous_catch, {0, {NO_VALUE}}};
  }

  static Control If(const byte* pc, int stack_depth, SsaEnv* end_env,
                    SsaEnv* false_env, int32_t previous_catch) {
    return {pc,        kControlIf, stack_depth,    end_env,
            false_env, nullptr,    previous_catch, {0, {NO_VALUE}}};
  }

  static Control Loop(const byte* pc, int stack_depth, SsaEnv* end_env,
                      int32_t previous_catch) {
    return {pc,      kControlLoop, stack_depth,    end_env,
            nullptr, nullptr,      previous_catch, {0, {NO_VALUE}}};
  }

  static Control Try(const byte* pc, int stack_depth, SsaEnv* end_env,
                     Zone* zone, SsaEnv* catch_env, int32_t previous_catch) {
    DCHECK_NOT_NULL(catch_env);
    TryInfo* try_info = new (zone) TryInfo(catch_env);
    return {pc,      kControlTry, stack_depth,    end_env,
            nullptr, try_info,    previous_catch, {0, {NO_VALUE}}};
  }
};

// Macros that build nodes only if there is a graph and the current SSA
// environment is reachable from start. This avoids problems with malformed
// TF graphs when decoding inputs that have unreachable code.
#define BUILD(func, ...) \
  (build() ? CheckForException(builder_->func(__VA_ARGS__)) : nullptr)
#define BUILD0(func) (build() ? CheckForException(builder_->func()) : nullptr)

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
    error(pc, pc + 1, "invalid local index: %u", operand.index);
    return false;
  }

  inline bool Validate(const byte* pc, GlobalIndexOperand& operand) {
    ModuleEnv* m = module_;
    if (m && m->module && operand.index < m->module->globals.size()) {
      operand.global = &m->module->globals[operand.index];
      operand.type = operand.global->type;
      return true;
    }
    error(pc, pc + 1, "invalid global index: %u", operand.index);
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
      return true;
    }
    error(pc, pc + 1, "invalid function index: %u", operand.index);
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
      return true;
    }
    error(pc, pc + 1, "invalid signature index: #%u", operand.index);
    return false;
  }

  inline bool Validate(const byte* pc, BreakDepthOperand& operand,
                       ZoneVector<Control>& control) {
    if (operand.depth < control.size()) {
      operand.target = &control[control.size() - operand.depth - 1];
      return true;
    }
    error(pc, pc + 1, "invalid break depth: %u", operand.depth);
    return false;
  }

  bool Validate(const byte* pc, BranchTableOperand& operand,
                size_t block_depth) {
    // TODO(titzer): add extra redundant validation for br_table here?
    return true;
  }

  unsigned OpcodeLength(const byte* pc) {
    switch (static_cast<WasmOpcode>(*pc)) {
#define DECLARE_OPCODE_CASE(name, opcode, sig) case kExpr##name:
      FOREACH_LOAD_MEM_OPCODE(DECLARE_OPCODE_CASE)
      FOREACH_STORE_MEM_OPCODE(DECLARE_OPCODE_CASE)
#undef DECLARE_OPCODE_CASE
      {
        MemoryAccessOperand operand(this, pc, UINT32_MAX);
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

      case kExprTry:
      case kExprIf:  // fall thru
      case kExprLoop:
      case kExprBlock: {
        BlockTypeOperand operand(this, pc);
        return 1 + operand.length;
      }

      case kExprSetLocal:
      case kExprTeeLocal:
      case kExprGetLocal:
      case kExprCatch: {
        LocalIndexOperand operand(this, pc);
        return 1 + operand.length;
      }
      case kExprBrTable: {
        BranchTableOperand operand(this, pc);
        BranchTableIterator iterator(this, operand);
        return 1 + iterator.length();
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
      default:
        return 1;
    }
  }
};

static const int32_t kNullCatch = -1;

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
        last_end_found_(false),
        current_catch_(kNullCatch) {
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
      error("function body end < start");
      return false;
    }

    DecodeLocalDecls();
    InitSsaEnv();
    DecodeFunctionBody();

    if (failed()) return TraceFailed();

#if IMPLICIT_FUNCTION_END
    // With implicit end support (old style), the function block
    // remains on the stack. Other control blocks are an error.
    if (control_.size() > 1) {
      error(pc_, control_.back().pc, "unterminated control structure");
      return TraceFailed();
    }

    // Assume an implicit end to the function body block.
    if (control_.size() == 1) {
      Control* c = &control_.back();
      if (ssa_env_->go()) {
        FallThruTo(c);
      }

      if (c->end_env->go()) {
        // Push the end values onto the stack.
        stack_.resize(c->stack_depth);
        if (c->merge.arity == 1) {
          stack_.push_back(c->merge.vals.first);
        } else {
          for (unsigned i = 0; i < c->merge.arity; i++) {
            stack_.push_back(c->merge.vals.array[i]);
          }
        }

        TRACE("  @%-8d #xx:%-20s|", startrel(pc_), "ImplicitReturn");
        SetEnv("function:end", c->end_env);
        DoReturn();
        TRACE("\n");
      }
    }
#else
    if (!control_.empty()) {
      error(pc_, control_.back().pc, "unterminated control structure");
      return TraceFailed();
    }

    if (!last_end_found_) {
      error("function body must end with \"end\" opcode.");
      return false;
    }
#endif

    if (FLAG_trace_wasm_decode_time) {
      double ms = decode_timer.Elapsed().InMillisecondsF();
      PrintF("wasm-decode %s (%0.3f ms)\n\n", ok() ? "ok" : "failed", ms);
    } else {
      TRACE("wasm-decode %s\n\n", ok() ? "ok" : "failed");
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
  bool last_end_found_;

  int32_t current_catch_;

  TryInfo* current_try_info() { return control_[current_catch_].try_info; }

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
    TRACE("local decls count: %u\n", entries);
    while (entries-- > 0 && pc_ < limit_) {
      uint32_t count = consume_u32v("local count");
      if (count > kMaxNumWasmLocals) {
        error(pc_ - 1, "local count too large");
        return;
      }
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

    {
      // Set up initial function block.
      SsaEnv* break_env = ssa_env_;
      SetEnv("initial env", Steal(break_env));
      PushBlock(break_env);
      Control* c = &control_.back();
      c->merge.arity = static_cast<uint32_t>(sig_->return_count());

      if (c->merge.arity == 1) {
        c->merge.vals.first = {pc_, nullptr, sig_->GetReturn(0)};
      } else if (c->merge.arity > 1) {
        c->merge.vals.array = zone_->NewArray<Value>(c->merge.arity);
        for (unsigned i = 0; i < c->merge.arity; i++) {
          c->merge.vals.array[i] = {pc_, nullptr, sig_->GetReturn(i)};
        }
      }
    }

    if (pc_ >= limit_) return;  // Nothing to do.

    while (true) {  // decoding loop.
      unsigned len = 1;
      WasmOpcode opcode = static_cast<WasmOpcode>(*pc_);
      if (!WasmOpcodes::IsPrefixOpcode(opcode)) {
        TRACE("  @%-8d #%02x:%-20s|", startrel(pc_), opcode,
              WasmOpcodes::ShortOpcodeName(opcode));
      }

      FunctionSig* sig = WasmOpcodes::Signature(opcode);
      if (sig) {
        BuildSimpleOperator(opcode, sig);
      } else {
        // Complex bytecode.
        switch (opcode) {
          case kExprNop:
            break;
          case kExprBlock: {
            // The break environment is the outer environment.
            BlockTypeOperand operand(this, pc_);
            SsaEnv* break_env = ssa_env_;
            PushBlock(break_env);
            SetEnv("block:start", Steal(break_env));
            SetBlockType(&control_.back(), operand);
            len = 1 + operand.length;
            break;
          }
          case kExprThrow: {
            CHECK_PROTOTYPE_OPCODE(wasm_eh_prototype);
            Value value = Pop(0, kAstI32);
            BUILD(Throw, value.node);
            break;
          }
          case kExprTry: {
            CHECK_PROTOTYPE_OPCODE(wasm_eh_prototype);
            BlockTypeOperand operand(this, pc_);
            SsaEnv* outer_env = ssa_env_;
            SsaEnv* try_env = Steal(outer_env);
            SsaEnv* catch_env = UnreachableEnv();
            PushTry(outer_env, catch_env);
            SetEnv("try_catch:start", try_env);
            SetBlockType(&control_.back(), operand);
            len = 1 + operand.length;
            break;
          }
          case kExprCatch: {
            CHECK_PROTOTYPE_OPCODE(wasm_eh_prototype);
            LocalIndexOperand operand(this, pc_);
            len = 1 + operand.length;

            if (control_.empty()) {
              error("catch does not match any try");
              break;
            }

            Control* c = &control_.back();
            if (!c->is_try()) {
              error("catch does not match any try");
              break;
            }

            if (c->try_info->catch_env == nullptr) {
              error(pc_, "catch already present for try with catch");
              break;
            }

            if (ssa_env_->go()) {
              MergeValuesInto(c);
            }
            stack_.resize(c->stack_depth);

            DCHECK_NOT_NULL(c->try_info);
            SsaEnv* catch_env = c->try_info->catch_env;
            c->try_info->catch_env = nullptr;
            SetEnv("catch:begin", catch_env);
            current_catch_ = c->previous_catch;

            if (Validate(pc_, operand)) {
              if (ssa_env_->locals) {
                TFNode* exception_as_i32 =
                    BUILD(Catch, c->try_info->exception, position());
                ssa_env_->locals[operand.index] = exception_as_i32;
              }
            }

            break;
          }
          case kExprLoop: {
            BlockTypeOperand operand(this, pc_);
            SsaEnv* finish_try_env = Steal(ssa_env_);
            // The continue environment is the inner environment.
            PrepareForLoop(pc_, finish_try_env);
            SetEnv("loop:start", Split(finish_try_env));
            ssa_env_->SetNotMerged();
            PushLoop(finish_try_env);
            SetBlockType(&control_.back(), operand);
            len = 1 + operand.length;
            break;
          }
          case kExprIf: {
            // Condition on top of stack. Split environments for branches.
            BlockTypeOperand operand(this, pc_);
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
            SetBlockType(&control_.back(), operand);
            len = 1 + operand.length;
            break;
          }
          case kExprElse: {
            if (control_.empty()) {
              error("else does not match any if");
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
            FallThruTo(c);
            // Switch to environment for false branch.
            stack_.resize(c->stack_depth);
            SetEnv("if_else:false", c->false_env);
            c->false_env = nullptr;  // record that an else is already seen
            break;
          }
          case kExprEnd: {
            if (control_.empty()) {
              error("end does not match any if, try, or block");
              return;
            }
            const char* name = "block:end";
            Control* c = &control_.back();
            if (c->is_loop()) {
              // A loop just leaves the values on the stack.
              TypeCheckLoopFallThru(c);
              PopControl();
              SetEnv("loop:end", ssa_env_);
              break;
            }
            if (c->is_if()) {
              if (c->false_env != nullptr) {
                // End the true branch of a one-armed if.
                Goto(c->false_env, c->end_env);
                if (ssa_env_->go() && stack_.size() != c->stack_depth) {
                  error("end of if expected empty stack");
                  stack_.resize(c->stack_depth);
                }
                if (c->merge.arity > 0) {
                  error("non-void one-armed if");
                }
                name = "if:merge";
              } else {
                // End the false branch of a two-armed if.
                name = "if_else:merge";
              }
            } else if (c->is_try()) {
              name = "try:end";

              // validate that catch was seen.
              if (c->try_info->catch_env != nullptr) {
                error(pc_, "missing catch in try");
                break;
              }
            }
            FallThruTo(c);
            SetEnv(name, c->end_env);

            // Push the end values onto the stack.
            stack_.resize(c->stack_depth);
            if (c->merge.arity == 1) {
              stack_.push_back(c->merge.vals.first);
            } else {
              for (unsigned i = 0; i < c->merge.arity; i++) {
                stack_.push_back(c->merge.vals.array[i]);
              }
            }

            PopControl();

            if (control_.empty()) {
              // If the last (implicit) control was popped, check we are at end.
              if (pc_ + 1 != end_) {
                error(pc_, pc_ + 1, "trailing code after function end");
              }
              last_end_found_ = true;
              if (ssa_env_->go()) {
                // The result of the block is the return value.
                TRACE("  @%-8d #xx:%-20s|", startrel(pc_), "ImplicitReturn");
                DoReturn();
                TRACE("\n");
              }
              return;
            }
            break;
          }
          case kExprSelect: {
            Value cond = Pop(2, kAstI32);
            Value fval = Pop();
            Value tval = Pop();
            if (tval.type == kAstStmt || tval.type != fval.type) {
              if (tval.type != kAstEnd && fval.type != kAstEnd) {
                error("type mismatch in select");
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
            if (Validate(pc_, operand, control_)) {
              BreakTo(operand.depth);
            }
            len = 1 + operand.length;
            EndControl();
            break;
          }
          case kExprBrIf: {
            BreakDepthOperand operand(this, pc_);
            Value cond = Pop(0, kAstI32);
            if (ok() && Validate(pc_, operand, control_)) {
              SsaEnv* fenv = ssa_env_;
              SsaEnv* tenv = Split(fenv);
              fenv->SetNotMerged();
              BUILD(Branch, cond.node, &tenv->control, &fenv->control);
              ssa_env_ = tenv;
              BreakTo(operand.depth);
              ssa_env_ = fenv;
            }
            len = 1 + operand.length;
            break;
          }
          case kExprBrTable: {
            BranchTableOperand operand(this, pc_);
            BranchTableIterator iterator(this, operand);
            if (Validate(pc_, operand, control_.size())) {
              Value key = Pop(0, kAstI32);
              if (failed()) break;

              SsaEnv* break_env = ssa_env_;
              if (operand.table_count > 0) {
                // Build branches to the various blocks based on the table.
                TFNode* sw = BUILD(Switch, operand.table_count + 1, key.node);

                SsaEnv* copy = Steal(break_env);
                ssa_env_ = copy;
                while (iterator.has_next()) {
                  uint32_t i = iterator.cur_index();
                  const byte* pos = iterator.pc();
                  uint32_t target = iterator.next();
                  if (target >= control_.size()) {
                    error(pos, "improper branch in br_table");
                    break;
                  }
                  ssa_env_ = Split(copy);
                  ssa_env_->control = (i == operand.table_count)
                                          ? BUILD(IfDefault, sw)
                                          : BUILD(IfValue, i, sw);
                  BreakTo(target);
                }
              } else {
                // Only a default target. Do the equivalent of br.
                const byte* pos = iterator.pc();
                uint32_t target = iterator.next();
                if (target >= control_.size()) {
                  error(pos, "improper branch in br_table");
                  break;
                }
                BreakTo(target);
              }
              // br_table ends the control flow like br.
              ssa_env_ = break_env;
            }
            len = 1 + iterator.length();
            break;
          }
          case kExprReturn: {
            DoReturn();
            break;
          }
          case kExprUnreachable: {
            BUILD(Unreachable, position());
            EndControl();
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
            }
            len = 1 + operand.length;
            break;
          }
          case kExprTeeLocal: {
            LocalIndexOperand operand(this, pc_);
            if (Validate(pc_, operand)) {
              Value val = Pop(0, local_type_vec_[operand.index]);
              if (ssa_env_->locals) ssa_env_->locals[operand.index] = val.node;
              Push(val.type, val.node);
            }
            len = 1 + operand.length;
            break;
          }
          case kExprDrop: {
            Pop();
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
              if (operand.global->mutability) {
                Value val = Pop(0, operand.type);
                BUILD(SetGlobal, operand.index, val.node);
              } else {
                error(pc_, pc_ + 1, "immutable global #%u cannot be assigned",
                      operand.index);
              }
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
          case kExprGrowMemory:
            if (module_->origin != kAsmJsOrigin) {
              Value val = Pop(0, kAstI32);
              Push(kAstI32, BUILD(GrowMemory, val.node));
            } else {
              error("grow_memory is not supported for asmjs modules");
            }
            break;
          case kExprMemorySize:
            Push(kAstI32, BUILD(CurrentMemoryPages));
            break;
          case kExprCallFunction: {
            CallFunctionOperand operand(this, pc_);
            if (Validate(pc_, operand)) {
              TFNode** buffer = PopArgs(operand.sig);
              TFNode** rets = nullptr;
              BUILD(CallDirect, operand.index, buffer, &rets, position());
              PushReturns(operand.sig, rets);
            }
            len = 1 + operand.length;
            break;
          }
          case kExprCallIndirect: {
            CallIndirectOperand operand(this, pc_);
            if (Validate(pc_, operand)) {
              Value index = Pop(0, kAstI32);
              TFNode** buffer = PopArgs(operand.sig);
              if (buffer) buffer[0] = index.node;
              TFNode** rets = nullptr;
              BUILD(CallIndirect, operand.index, buffer, &rets, position());
              PushReturns(operand.sig, rets);
            }
            len = 1 + operand.length;
            break;
          }
          case kSimdPrefix: {
            CHECK_PROTOTYPE_OPCODE(wasm_simd_prototype);
            len++;
            byte simd_index = *(pc_ + 1);
            opcode = static_cast<WasmOpcode>(opcode << 8 | simd_index);
            TRACE("  @%-4d #%02x #%02x:%-20s|", startrel(pc_), kSimdPrefix,
                  simd_index, WasmOpcodes::ShortOpcodeName(opcode));
            len += DecodeSimdOpcode(opcode);
            break;
          }
          default: {
            // Deal with special asmjs opcodes.
            if (module_ && module_->origin == kAsmJsOrigin) {
              sig = WasmOpcodes::AsmjsSignature(opcode);
              if (sig) {
                BuildSimpleOperator(opcode, sig);
              }
            } else {
              error("Invalid opcode");
              return;
            }
          }
        }
      }

#if DEBUG
      if (FLAG_trace_wasm_decoder) {
        for (size_t i = 0; i < stack_.size(); ++i) {
          Value& val = stack_[i];
          WasmOpcode opcode = static_cast<WasmOpcode>(*val.pc);
          if (WasmOpcodes::IsPrefixOpcode(opcode)) {
            opcode = static_cast<WasmOpcode>(opcode << 8 | *(val.pc + 1));
          }
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
            case kExprSetLocal:  // fallthru
            case kExprTeeLocal: {
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
  }

  void EndControl() { ssa_env_->Kill(SsaEnv::kControlEnd); }

  void SetBlockType(Control* c, BlockTypeOperand& operand) {
    c->merge.arity = operand.arity;
    if (c->merge.arity == 1) {
      c->merge.vals.first = {pc_, nullptr, operand.read_entry(0)};
    } else if (c->merge.arity > 1) {
      c->merge.vals.array = zone_->NewArray<Value>(c->merge.arity);
      for (unsigned i = 0; i < c->merge.arity; i++) {
        c->merge.vals.array[i] = {pc_, nullptr, operand.read_entry(i)};
      }
    }
  }

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
        Control::Block(pc_, stack_depth, end_env, current_catch_));
  }

  void PushLoop(SsaEnv* end_env) {
    const int stack_depth = static_cast<int>(stack_.size());
    control_.emplace_back(
        Control::Loop(pc_, stack_depth, end_env, current_catch_));
  }

  void PushIf(SsaEnv* end_env, SsaEnv* false_env) {
    const int stack_depth = static_cast<int>(stack_.size());
    control_.emplace_back(
        Control::If(pc_, stack_depth, end_env, false_env, current_catch_));
  }

  void PushTry(SsaEnv* end_env, SsaEnv* catch_env) {
    const int stack_depth = static_cast<int>(stack_.size());
    control_.emplace_back(Control::Try(pc_, stack_depth, end_env, zone_,
                                       catch_env, current_catch_));
    current_catch_ = static_cast<int32_t>(control_.size() - 1);
  }

  void PopControl() { control_.pop_back(); }

  int DecodeLoadMem(LocalType type, MachineType mem_type) {
    MemoryAccessOperand operand(this, pc_,
                                ElementSizeLog2Of(mem_type.representation()));

    Value index = Pop(0, kAstI32);
    TFNode* node = BUILD(LoadMem, type, mem_type, index.node, operand.offset,
                         operand.alignment, position());
    Push(type, node);
    return 1 + operand.length;
  }

  int DecodeStoreMem(LocalType type, MachineType mem_type) {
    MemoryAccessOperand operand(this, pc_,
                                ElementSizeLog2Of(mem_type.representation()));
    Value val = Pop(1, type);
    Value index = Pop(0, kAstI32);
    BUILD(StoreMem, mem_type, index.node, operand.offset, operand.alignment,
          val.node, position());
    return 1 + operand.length;
  }

  unsigned DecodeSimdOpcode(WasmOpcode opcode) {
    unsigned len = 0;
    switch (opcode) {
      case kExprI32x4ExtractLane: {
        uint8_t lane = this->checked_read_u8(pc_, 2, "lane number");
        if (lane < 0 || lane > 3) {
          error(pc_, pc_ + 2, "invalid extract lane value");
        }
        TFNode* input = Pop(0, LocalType::kSimd128).node;
        TFNode* node = BUILD(SimdExtractLane, opcode, lane, input);
        Push(LocalType::kWord32, node);
        len++;
        break;
      }
      default: {
        FunctionSig* sig = WasmOpcodes::Signature(opcode);
        if (sig != nullptr) {
          compiler::NodeVector inputs(sig->parameter_count(), zone_);
          for (size_t i = sig->parameter_count(); i > 0; i--) {
            Value val = Pop(static_cast<int>(i - 1), sig->GetParam(i - 1));
            inputs[i - 1] = val.node;
          }
          TFNode* node = BUILD(SimdOp, opcode, inputs);
          Push(GetReturnType(sig), node);
        } else {
          error("invalid simd opcode");
        }
      }
    }
    return len;
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

    BUILD(Return, count, buffer);
    EndControl();
  }

  void Push(LocalType type, TFNode* node) {
    if (type != kAstStmt && type != kAstEnd) {
      stack_.push_back({pc_, node, type});
    }
  }

  void PushReturns(FunctionSig* sig, TFNode** rets) {
    for (size_t i = 0; i < sig->return_count(); i++) {
      // When verifying only, then {rets} will be null, so push null.
      Push(sig->GetReturn(i), rets ? rets[i] : nullptr);
    }
  }

  const char* SafeOpcodeNameAt(const byte* pc) {
    if (pc >= end_) return "<end>";
    return WasmOpcodes::ShortOpcodeName(static_cast<WasmOpcode>(*pc));
  }

  Value Pop(int index, LocalType expected) {
    if (!ssa_env_->go()) {
      // Unreachable code is essentially not typechecked.
      return {pc_, nullptr, expected};
    }
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
    if (!ssa_env_->go()) {
      // Unreachable code is essentially not typechecked.
      return {pc_, nullptr, kAstEnd};
    }
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
    if (!ssa_env_->go()) {
      // Unreachable code is essentially not typechecked.
      return {pc_, nullptr, kAstEnd};
    }
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

  void BreakTo(unsigned depth) {
    if (!ssa_env_->go()) return;
    Control* c = &control_[control_.size() - depth - 1];
    if (c->is_loop()) {
      // This is the inner loop block, which does not have a value.
      Goto(ssa_env_, c->end_env);
    } else {
      // Merge the value(s) into the end of the block.
      if (static_cast<size_t>(c->stack_depth + c->merge.arity) >
          stack_.size()) {
        error(
            pc_, pc_,
            "expected at least %d values on the stack for br to @%d, found %d",
            c->merge.arity, startrel(c->pc),
            static_cast<int>(stack_.size() - c->stack_depth));
        return;
      }
      MergeValuesInto(c);
    }
  }

  void FallThruTo(Control* c) {
    if (!ssa_env_->go()) return;
    // Merge the value(s) into the end of the block.
    int arity = static_cast<int>(c->merge.arity);
    if (c->stack_depth + arity != stack_.size()) {
      error(pc_, pc_, "expected %d elements on the stack for fallthru to @%d",
            arity, startrel(c->pc));
      return;
    }
    MergeValuesInto(c);
  }

  inline Value& GetMergeValueFromStack(Control* c, int i) {
    return stack_[stack_.size() - c->merge.arity + i];
  }

  void TypeCheckLoopFallThru(Control* c) {
    if (!ssa_env_->go()) return;
    // Fallthru must match arity exactly.
    int arity = static_cast<int>(c->merge.arity);
    if (c->stack_depth + arity != stack_.size()) {
      error(pc_, pc_, "expected %d elements on the stack for fallthru to @%d",
            arity, startrel(c->pc));
      return;
    }
    // Typecheck the values left on the stack.
    for (unsigned i = 0; i < c->merge.arity; i++) {
      Value& val = GetMergeValueFromStack(c, i);
      Value& old =
          c->merge.arity == 1 ? c->merge.vals.first : c->merge.vals.array[i];
      if (val.type != old.type) {
        error(pc_, pc_, "type error in merge[%d] (expected %s, got %s)", i,
              WasmOpcodes::TypeName(old.type), WasmOpcodes::TypeName(val.type));
        return;
      }
    }
  }

  void MergeValuesInto(Control* c) {
    SsaEnv* target = c->end_env;
    bool first = target->state == SsaEnv::kUnreachable;
    Goto(ssa_env_, target);

    for (unsigned i = 0; i < c->merge.arity; i++) {
      Value& val = GetMergeValueFromStack(c, i);
      Value& old =
          c->merge.arity == 1 ? c->merge.vals.first : c->merge.vals.array[i];
      if (val.type != old.type) {
        error(pc_, pc_, "type error in merge[%d] (expected %s, got %s)", i,
              WasmOpcodes::TypeName(old.type), WasmOpcodes::TypeName(val.type));
        return;
      }
      old.node =
          first ? val.node : CreateOrMergeIntoPhi(old.type, target->control,
                                                  old.node, val.node);
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

  TFNode* CheckForException(TFNode* node) {
    if (node == nullptr) {
      return nullptr;
    }

    const bool inside_try_scope = current_catch_ != kNullCatch;

    if (!inside_try_scope) {
      return node;
    }

    TFNode* if_success = nullptr;
    TFNode* if_exception = nullptr;
    if (!builder_->ThrowsException(node, &if_success, &if_exception)) {
      return node;
    }

    SsaEnv* success_env = Steal(ssa_env_);
    success_env->control = if_success;

    SsaEnv* exception_env = Split(success_env);
    exception_env->control = if_exception;
    TryInfo* try_info = current_try_info();
    Goto(exception_env, try_info->catch_env);
    TFNode* exception = try_info->exception;
    if (exception == nullptr) {
      DCHECK_EQ(SsaEnv::kReached, try_info->catch_env->state);
      try_info->exception = if_exception;
    } else {
      DCHECK_EQ(SsaEnv::kMerged, try_info->catch_env->state);
      try_info->exception =
          CreateOrMergeIntoPhi(kAstI32, try_info->catch_env->control,
                               try_info->exception, if_exception);
    }

    SetEnv("if_success", success_env);
    return node;
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
        case kExprTry:
          length = OpcodeLength(pc);
          depth++;
          break;
        case kExprSetLocal:  // fallthru
        case kExprTeeLocal: {
          LocalIndexOperand operand(this, pc);
          if (assigned->length() > 0 &&
              operand.index < static_cast<uint32_t>(assigned->length())) {
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

  inline void BuildSimpleOperator(WasmOpcode opcode, FunctionSig* sig) {
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
  }
};

bool DecodeLocalDecls(AstLocalDecls& decls, const byte* start,
                      const byte* end) {
  AccountingAllocator allocator;
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

DecodeResult VerifyWasmCode(AccountingAllocator* allocator,
                            FunctionBody& body) {
  Zone zone(allocator);
  WasmFullDecoder decoder(&zone, nullptr, body);
  decoder.Decode();
  return decoder.toResult<DecodeStruct*>(nullptr);
}

DecodeResult BuildTFGraph(AccountingAllocator* allocator, TFBuilder* builder,
                          FunctionBody& body) {
  Zone zone(allocator);
  WasmFullDecoder decoder(&zone, builder, body);
  decoder.Decode();
  return decoder.toResult<DecodeStruct*>(nullptr);
}

unsigned OpcodeLength(const byte* pc, const byte* end) {
  WasmDecoder decoder(nullptr, nullptr, pc, end);
  return decoder.OpcodeLength(pc);
}

void PrintAstForDebugging(const byte* start, const byte* end) {
  AccountingAllocator allocator;
  OFStream os(stdout);
  PrintAst(&allocator, FunctionBodyForTesting(start, end), os, nullptr);
}

bool PrintAst(AccountingAllocator* allocator, const FunctionBody& body,
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
      case kExprElse:
        os << "   // @" << i.pc_offset();
        control_depth++;
        break;
      case kExprLoop:
      case kExprIf:
      case kExprBlock:
      case kExprTry: {
        BlockTypeOperand operand(&i, i.pc());
        os << "   // @" << i.pc_offset();
        for (unsigned i = 0; i < operand.arity; i++) {
          os << " " << WasmOpcodes::TypeName(operand.read_entry(i));
        }
        control_depth++;
        break;
      }
      case kExprEnd:
        os << "   // @" << i.pc_offset();
        control_depth--;
        break;
      case kExprBr: {
        BreakDepthOperand operand(&i, i.pc());
        os << "   // depth=" << operand.depth;
        break;
      }
      case kExprBrIf: {
        BreakDepthOperand operand(&i, i.pc());
        os << "   // depth=" << operand.depth;
        break;
      }
      case kExprBrTable: {
        BranchTableOperand operand(&i, i.pc());
        os << " // entries=" << operand.table_count;
        break;
      }
      case kExprCallIndirect: {
        CallIndirectOperand operand(&i, i.pc());
        os << "   // sig #" << operand.index;
        if (decoder.Complete(i.pc(), operand)) {
          os << ": " << *operand.sig;
        }
        break;
      }
      case kExprCallFunction: {
        CallFunctionOperand operand(&i, i.pc());
        os << " // function #" << operand.index;
        if (decoder.Complete(i.pc(), operand)) {
          os << ": " << *operand.sig;
        }
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
