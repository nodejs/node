// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/signature.h"

#include "src/base/platform/elapsed-timer.h"
#include "src/bit-vector.h"
#include "src/flags.h"
#include "src/handles.h"
#include "src/objects-inl.h"
#include "src/zone/zone-containers.h"

#include "src/wasm/decoder.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/wasm-limits.h"
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
  if (module_ != nullptr && module_->is_asm_js()) {    \
    error("Opcode not supported for asmjs modules");   \
  }                                                    \
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
  ValueType type;
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

  Value& operator[](size_t i) {
    DCHECK_GT(arity, i);
    return arity == 1 ? vals.first : vals.array[i];
  }
};

static Value* NO_VALUE = nullptr;

enum ControlKind { kControlIf, kControlBlock, kControlLoop, kControlTry };

// An entry on the control stack (i.e. if, block, loop).
struct Control {
  const byte* pc;
  ControlKind kind;
  size_t stack_depth;      // stack height at the beginning of the construct.
  SsaEnv* end_env;         // end environment for the construct.
  SsaEnv* false_env;       // false environment (only for if).
  TryInfo* try_info;       // Information used for compiling try statements.
  int32_t previous_catch;  // The previous Control (on the stack) with a catch.
  bool unreachable;        // The current block has been ended.

  // Values merged into the end of this control construct.
  MergeValues merge;

  inline bool is_if() const { return kind == kControlIf; }
  inline bool is_block() const { return kind == kControlBlock; }
  inline bool is_loop() const { return kind == kControlLoop; }
  inline bool is_try() const { return kind == kControlTry; }

  // Named constructors.
  static Control Block(const byte* pc, size_t stack_depth, SsaEnv* end_env,
                       int32_t previous_catch) {
    return {pc,      kControlBlock,  stack_depth, end_env,        nullptr,
            nullptr, previous_catch, false,       {0, {NO_VALUE}}};
  }

  static Control If(const byte* pc, size_t stack_depth, SsaEnv* end_env,
                    SsaEnv* false_env, int32_t previous_catch) {
    return {pc,      kControlIf,     stack_depth, end_env,        false_env,
            nullptr, previous_catch, false,       {0, {NO_VALUE}}};
  }

  static Control Loop(const byte* pc, size_t stack_depth, SsaEnv* end_env,
                      int32_t previous_catch) {
    return {pc,      kControlLoop,   stack_depth, end_env,        nullptr,
            nullptr, previous_catch, false,       {0, {NO_VALUE}}};
  }

  static Control Try(const byte* pc, size_t stack_depth, SsaEnv* end_env,
                     Zone* zone, SsaEnv* catch_env, int32_t previous_catch) {
    DCHECK_NOT_NULL(catch_env);
    TryInfo* try_info = new (zone) TryInfo(catch_env);
    return {pc,       kControlTry,    stack_depth, end_env,        nullptr,
            try_info, previous_catch, false,       {0, {NO_VALUE}}};
  }
};

namespace {
inline unsigned GetShuffleMaskSize(WasmOpcode opcode) {
  switch (opcode) {
    case kExprS32x4Shuffle:
      return 4;
    case kExprS16x8Shuffle:
      return 8;
    case kExprS8x16Shuffle:
      return 16;
    default:
      UNREACHABLE();
      return 0;
  }
}
}  // namespace

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
  WasmDecoder(const WasmModule* module, FunctionSig* sig, const byte* start,
              const byte* end)
      : Decoder(start, end),
        module_(module),
        sig_(sig),
        local_types_(nullptr) {}
  const WasmModule* module_;
  FunctionSig* sig_;

  ZoneVector<ValueType>* local_types_;

  size_t total_locals() const {
    return local_types_ == nullptr ? 0 : local_types_->size();
  }

  static bool DecodeLocals(Decoder* decoder, const FunctionSig* sig,
                           ZoneVector<ValueType>* type_list) {
    DCHECK_NOT_NULL(type_list);
    DCHECK_EQ(0, type_list->size());
    // Initialize from signature.
    if (sig != nullptr) {
      type_list->assign(sig->parameters().begin(), sig->parameters().end());
    }
    // Decode local declarations, if any.
    uint32_t entries = decoder->consume_u32v("local decls count");
    if (decoder->failed()) return false;

    TRACE("local decls count: %u\n", entries);
    while (entries-- > 0 && decoder->ok() && decoder->more()) {
      uint32_t count = decoder->consume_u32v("local count");
      if (decoder->failed()) return false;

      if ((count + type_list->size()) > kV8MaxWasmFunctionLocals) {
        decoder->error(decoder->pc() - 1, "local count too large");
        return false;
      }
      byte code = decoder->consume_u8("local type");
      if (decoder->failed()) return false;

      ValueType type;
      switch (code) {
        case kLocalI32:
          type = kWasmI32;
          break;
        case kLocalI64:
          type = kWasmI64;
          break;
        case kLocalF32:
          type = kWasmF32;
          break;
        case kLocalF64:
          type = kWasmF64;
          break;
        case kLocalS128:
          type = kWasmS128;
          break;
        case kLocalS1x4:
          type = kWasmS1x4;
          break;
        case kLocalS1x8:
          type = kWasmS1x8;
          break;
        case kLocalS1x16:
          type = kWasmS1x16;
          break;
        default:
          decoder->error(decoder->pc() - 1, "invalid local type");
          return false;
      }
      type_list->insert(type_list->end(), count, type);
    }
    DCHECK(decoder->ok());
    return true;
  }

  static BitVector* AnalyzeLoopAssignment(Decoder* decoder, const byte* pc,
                                          int locals_count, Zone* zone) {
    if (pc >= decoder->end()) return nullptr;
    if (*pc != kExprLoop) return nullptr;

    BitVector* assigned = new (zone) BitVector(locals_count, zone);
    int depth = 0;
    // Iteratively process all AST nodes nested inside the loop.
    while (pc < decoder->end() && decoder->ok()) {
      WasmOpcode opcode = static_cast<WasmOpcode>(*pc);
      unsigned length = 1;
      switch (opcode) {
        case kExprLoop:
        case kExprIf:
        case kExprBlock:
        case kExprTry:
          length = OpcodeLength(decoder, pc);
          depth++;
          break;
        case kExprSetLocal:  // fallthru
        case kExprTeeLocal: {
          LocalIndexOperand<true> operand(decoder, pc);
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
          length = OpcodeLength(decoder, pc);
          break;
      }
      if (depth <= 0) break;
      pc += length;
    }
    return decoder->ok() ? assigned : nullptr;
  }

  inline bool Validate(const byte* pc, LocalIndexOperand<true>& operand) {
    if (operand.index < total_locals()) {
      if (local_types_) {
        operand.type = local_types_->at(operand.index);
      } else {
        operand.type = kWasmStmt;
      }
      return true;
    }
    errorf(pc + 1, "invalid local index: %u", operand.index);
    return false;
  }

  inline bool Validate(const byte* pc, GlobalIndexOperand<true>& operand) {
    if (module_ != nullptr && operand.index < module_->globals.size()) {
      operand.global = &module_->globals[operand.index];
      operand.type = operand.global->type;
      return true;
    }
    errorf(pc + 1, "invalid global index: %u", operand.index);
    return false;
  }

  inline bool Complete(const byte* pc, CallFunctionOperand<true>& operand) {
    if (module_ != nullptr && operand.index < module_->functions.size()) {
      operand.sig = module_->functions[operand.index].sig;
      return true;
    }
    return false;
  }

  inline bool Validate(const byte* pc, CallFunctionOperand<true>& operand) {
    if (Complete(pc, operand)) {
      return true;
    }
    errorf(pc + 1, "invalid function index: %u", operand.index);
    return false;
  }

  inline bool Complete(const byte* pc, CallIndirectOperand<true>& operand) {
    if (module_ != nullptr && operand.index < module_->signatures.size()) {
      operand.sig = module_->signatures[operand.index];
      return true;
    }
    return false;
  }

  inline bool Validate(const byte* pc, CallIndirectOperand<true>& operand) {
    if (module_ == nullptr || module_->function_tables.empty()) {
      error("function table has to exist to execute call_indirect");
      return false;
    }
    if (Complete(pc, operand)) {
      return true;
    }
    errorf(pc + 1, "invalid signature index: #%u", operand.index);
    return false;
  }

  inline bool Validate(const byte* pc, BreakDepthOperand<true>& operand,
                       ZoneVector<Control>& control) {
    if (operand.depth < control.size()) {
      operand.target = &control[control.size() - operand.depth - 1];
      return true;
    }
    errorf(pc + 1, "invalid break depth: %u", operand.depth);
    return false;
  }

  bool Validate(const byte* pc, BranchTableOperand<true>& operand,
                size_t block_depth) {
    if (operand.table_count >= kV8MaxWasmFunctionSize) {
      errorf(pc + 1, "invalid table count (> max function size): %u",
             operand.table_count);
      return false;
    }
    return checkAvailable(operand.table_count);
  }

  inline bool Validate(const byte* pc, WasmOpcode opcode,
                       SimdLaneOperand<true>& operand) {
    uint8_t num_lanes = 0;
    switch (opcode) {
      case kExprF32x4ExtractLane:
      case kExprF32x4ReplaceLane:
      case kExprI32x4ExtractLane:
      case kExprI32x4ReplaceLane:
        num_lanes = 4;
        break;
      case kExprI16x8ExtractLane:
      case kExprI16x8ReplaceLane:
        num_lanes = 8;
        break;
      case kExprI8x16ExtractLane:
      case kExprI8x16ReplaceLane:
        num_lanes = 16;
        break;
      default:
        UNREACHABLE();
        break;
    }
    if (operand.lane < 0 || operand.lane >= num_lanes) {
      error(pc_ + 2, "invalid lane index");
      return false;
    } else {
      return true;
    }
  }

  inline bool Validate(const byte* pc, WasmOpcode opcode,
                       SimdShiftOperand<true>& operand) {
    uint8_t max_shift = 0;
    switch (opcode) {
      case kExprI32x4Shl:
      case kExprI32x4ShrS:
      case kExprI32x4ShrU:
        max_shift = 32;
        break;
      case kExprI16x8Shl:
      case kExprI16x8ShrS:
      case kExprI16x8ShrU:
        max_shift = 16;
        break;
      case kExprI8x16Shl:
      case kExprI8x16ShrS:
      case kExprI8x16ShrU:
        max_shift = 8;
        break;
      default:
        UNREACHABLE();
        break;
    }
    if (operand.shift < 0 || operand.shift >= max_shift) {
      error(pc_ + 2, "invalid shift amount");
      return false;
    } else {
      return true;
    }
  }

  inline bool Validate(const byte* pc, WasmOpcode opcode,
                       SimdShuffleOperand<true>& operand) {
    unsigned lanes = GetShuffleMaskSize(opcode);
    uint8_t max_lane = 0;
    for (unsigned i = 0; i < lanes; i++)
      max_lane = std::max(max_lane, operand.shuffle[i]);
    if (operand.lanes != lanes || max_lane > 2 * lanes) {
      error(pc_ + 2, "invalid shuffle mask");
      return false;
    } else {
      return true;
    }
  }

  static unsigned OpcodeLength(Decoder* decoder, const byte* pc) {
    WasmOpcode opcode = static_cast<WasmOpcode>(*pc);
    switch (opcode) {
#define DECLARE_OPCODE_CASE(name, opcode, sig) case kExpr##name:
      FOREACH_LOAD_MEM_OPCODE(DECLARE_OPCODE_CASE)
      FOREACH_STORE_MEM_OPCODE(DECLARE_OPCODE_CASE)
#undef DECLARE_OPCODE_CASE
      {
        MemoryAccessOperand<true> operand(decoder, pc, UINT32_MAX);
        return 1 + operand.length;
      }
      case kExprBr:
      case kExprBrIf: {
        BreakDepthOperand<true> operand(decoder, pc);
        return 1 + operand.length;
      }
      case kExprSetGlobal:
      case kExprGetGlobal: {
        GlobalIndexOperand<true> operand(decoder, pc);
        return 1 + operand.length;
      }

      case kExprCallFunction: {
        CallFunctionOperand<true> operand(decoder, pc);
        return 1 + operand.length;
      }
      case kExprCallIndirect: {
        CallIndirectOperand<true> operand(decoder, pc);
        return 1 + operand.length;
      }

      case kExprTry:
      case kExprIf:  // fall thru
      case kExprLoop:
      case kExprBlock: {
        BlockTypeOperand<true> operand(decoder, pc);
        return 1 + operand.length;
      }

      case kExprSetLocal:
      case kExprTeeLocal:
      case kExprGetLocal:
      case kExprCatch: {
        LocalIndexOperand<true> operand(decoder, pc);
        return 1 + operand.length;
      }
      case kExprBrTable: {
        BranchTableOperand<true> operand(decoder, pc);
        BranchTableIterator<true> iterator(decoder, operand);
        return 1 + iterator.length();
      }
      case kExprI32Const: {
        ImmI32Operand<true> operand(decoder, pc);
        return 1 + operand.length;
      }
      case kExprI64Const: {
        ImmI64Operand<true> operand(decoder, pc);
        return 1 + operand.length;
      }
      case kExprGrowMemory:
      case kExprMemorySize: {
        MemoryIndexOperand<true> operand(decoder, pc);
        return 1 + operand.length;
      }
      case kExprF32Const:
        return 5;
      case kExprF64Const:
        return 9;
      case kSimdPrefix: {
        byte simd_index = decoder->read_u8<true>(pc + 1, "simd_index");
        WasmOpcode opcode =
            static_cast<WasmOpcode>(kSimdPrefix << 8 | simd_index);
        switch (opcode) {
#define DECLARE_OPCODE_CASE(name, opcode, sig) case kExpr##name:
          FOREACH_SIMD_0_OPERAND_OPCODE(DECLARE_OPCODE_CASE)
#undef DECLARE_OPCODE_CASE
          {
            return 2;
          }
#define DECLARE_OPCODE_CASE(name, opcode, sig) case kExpr##name:
          FOREACH_SIMD_1_OPERAND_OPCODE(DECLARE_OPCODE_CASE)
#undef DECLARE_OPCODE_CASE
          {
            return 3;
          }
          // Shuffles contain a byte array to determine the shuffle.
          case kExprS32x4Shuffle:
          case kExprS16x8Shuffle:
          case kExprS8x16Shuffle:
            return 2 + GetShuffleMaskSize(opcode);
          default:
            decoder->error(pc, "invalid SIMD opcode");
            return 2;
        }
      }
      default:
        return 1;
    }
  }

  std::pair<uint32_t, uint32_t> StackEffect(const byte* pc) {
    WasmOpcode opcode = static_cast<WasmOpcode>(*pc);
    // Handle "simple" opcodes with a fixed signature first.
    FunctionSig* sig = WasmOpcodes::Signature(opcode);
    if (!sig) sig = WasmOpcodes::AsmjsSignature(opcode);
    if (sig) return {sig->parameter_count(), sig->return_count()};

#define DECLARE_OPCODE_CASE(name, opcode, sig) case kExpr##name:
    // clang-format off
    switch (opcode) {
      case kExprSelect:
        return {3, 1};
      FOREACH_STORE_MEM_OPCODE(DECLARE_OPCODE_CASE)
        return {2, 0};
      FOREACH_LOAD_MEM_OPCODE(DECLARE_OPCODE_CASE)
      case kExprTeeLocal:
      case kExprGrowMemory:
        return {1, 1};
      case kExprSetLocal:
      case kExprSetGlobal:
      case kExprDrop:
      case kExprBrIf:
      case kExprBrTable:
      case kExprIf:
        return {1, 0};
      case kExprGetLocal:
      case kExprGetGlobal:
      case kExprI32Const:
      case kExprI64Const:
      case kExprF32Const:
      case kExprF64Const:
      case kExprMemorySize:
        return {0, 1};
      case kExprCallFunction: {
        CallFunctionOperand<true> operand(this, pc);
        CHECK(Complete(pc, operand));
        return {operand.sig->parameter_count(), operand.sig->return_count()};
      }
      case kExprCallIndirect: {
        CallIndirectOperand<true> operand(this, pc);
        CHECK(Complete(pc, operand));
        // Indirect calls pop an additional argument for the table index.
        return {operand.sig->parameter_count() + 1,
                operand.sig->return_count()};
      }
      case kExprBr:
      case kExprBlock:
      case kExprLoop:
      case kExprEnd:
      case kExprElse:
      case kExprNop:
      case kExprReturn:
      case kExprUnreachable:
        return {0, 0};
      default:
        V8_Fatal(__FILE__, __LINE__, "unimplemented opcode: %x", opcode);
        return {0, 0};
    }
#undef DECLARE_OPCODE_CASE
    // clang-format on
  }
};

static const int32_t kNullCatch = -1;

// The full WASM decoder for bytecode. Verifies bytecode and, optionally,
// generates a TurboFan IR graph.
class WasmFullDecoder : public WasmDecoder {
 public:
  WasmFullDecoder(Zone* zone, const wasm::WasmModule* module,
                  const FunctionBody& body)
      : WasmFullDecoder(zone, module, nullptr, body) {}

  WasmFullDecoder(Zone* zone, TFBuilder* builder, const FunctionBody& body)
      : WasmFullDecoder(zone, builder->module_env() == nullptr
                                  ? nullptr
                                  : builder->module_env()->module,
                        builder, body) {}

  bool Decode() {
    if (FLAG_wasm_code_fuzzer_gen_test) {
      PrintRawWasmCode(start_, end_);
    }
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

    DCHECK_EQ(0, local_types_->size());
    WasmDecoder::DecodeLocals(this, sig_, local_types_);
    InitSsaEnv();
    DecodeFunctionBody();

    if (failed()) return TraceFailed();

    if (!control_.empty()) {
      // Generate a better error message whether the unterminated control
      // structure is the function body block or an innner structure.
      if (control_.size() > 1) {
        error(control_.back().pc, "unterminated control structure");
      } else {
        error("function body must end with \"end\" opcode.");
      }
      return TraceFailed();
    }

    if (!last_end_found_) {
      error("function body must end with \"end\" opcode.");
      return false;
    }

    if (FLAG_trace_wasm_decode_time) {
      double ms = decode_timer.Elapsed().InMillisecondsF();
      PrintF("wasm-decode %s (%0.3f ms)\n\n", ok() ? "ok" : "failed", ms);
    } else {
      TRACE("wasm-decode %s\n\n", ok() ? "ok" : "failed");
    }

    return true;
  }

  bool TraceFailed() {
    TRACE("wasm-error module+%-6d func+%d: %s\n\n",
          baserel(start_ + error_offset_), error_offset_, error_msg_.c_str());
    return false;
  }

 private:
  WasmFullDecoder(Zone* zone, const wasm::WasmModule* module,
                  TFBuilder* builder, const FunctionBody& body)
      : WasmDecoder(module, body.sig, body.start, body.end),
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

  static const size_t kErrorMsgSize = 128;

  Zone* zone_;
  TFBuilder* builder_;
  const byte* base_;

  SsaEnv* ssa_env_;

  ZoneVector<ValueType> local_type_vec_;  // types of local variables.
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
        ssa_env->locals[index] = builder_->Param(index);
        index++;
      }
      while (index < local_type_vec_.size()) {
        ValueType type = local_type_vec_[index];
        TFNode* node = DefaultValue(type);
        while (index < local_type_vec_.size() &&
               local_type_vec_[index] == type) {
          // Do a whole run of like-typed locals at a time.
          ssa_env->locals[index++] = node;
        }
      }
    }
    ssa_env->control = start;
    ssa_env->effect = start;
    SetEnv("initial", ssa_env);
    if (builder_) {
      // The function-prologue stack check is associated with position 0, which
      // is never a position of any instruction in the function.
      builder_->StackCheck(0);
    }
  }

  TFNode* DefaultValue(ValueType type) {
    switch (type) {
      case kWasmI32:
        return builder_->Int32Constant(0);
      case kWasmI64:
        return builder_->Int64Constant(0);
      case kWasmF32:
        return builder_->Float32Constant(0);
      case kWasmF64:
        return builder_->Float64Constant(0);
      case kWasmS128:
        return builder_->S128Zero();
      case kWasmS1x4:
        return builder_->S1x4Zero();
      case kWasmS1x8:
        return builder_->S1x8Zero();
      case kWasmS1x16:
        return builder_->S1x16Zero();
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

  bool CheckHasMemory() {
    if (!module_->has_memory) {
      error(pc_ - 1, "memory instruction with no memory");
    }
    return module_->has_memory;
  }

  // Decodes the body of a function.
  void DecodeFunctionBody() {
    TRACE("wasm-decode %p...%p (module+%d, %d bytes) %s\n",
          reinterpret_cast<const void*>(start_),
          reinterpret_cast<const void*>(end_), baserel(pc_),
          static_cast<int>(end_ - start_), builder_ ? "graph building" : "");

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

    while (pc_ < end_) {  // decoding loop.
      unsigned len = 1;
      WasmOpcode opcode = static_cast<WasmOpcode>(*pc_);
#if DEBUG
      if (FLAG_trace_wasm_decoder && !WasmOpcodes::IsPrefixOpcode(opcode)) {
        TRACE("  @%-8d #%-20s|", startrel(pc_),
              WasmOpcodes::OpcodeName(opcode));
      }
#endif

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
            BlockTypeOperand<true> operand(this, pc_);
            SsaEnv* break_env = ssa_env_;
            PushBlock(break_env);
            SetEnv("block:start", Steal(break_env));
            SetBlockType(&control_.back(), operand);
            len = 1 + operand.length;
            break;
          }
          case kExprThrow: {
            CHECK_PROTOTYPE_OPCODE(wasm_eh_prototype);
            Value value = Pop(0, kWasmI32);
            BUILD(Throw, value.node);
            // TODO(titzer): Throw should end control, but currently we build a
            // (reachable) runtime call instead of connecting it directly to
            // end.
            //            EndControl();
            break;
          }
          case kExprTry: {
            CHECK_PROTOTYPE_OPCODE(wasm_eh_prototype);
            BlockTypeOperand<true> operand(this, pc_);
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
            LocalIndexOperand<true> operand(this, pc_);
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

            FallThruTo(c);
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
            BlockTypeOperand<true> operand(this, pc_);
            SsaEnv* finish_try_env = Steal(ssa_env_);
            // The continue environment is the inner environment.
            SsaEnv* loop_body_env = PrepareForLoop(pc_, finish_try_env);
            SetEnv("loop:start", loop_body_env);
            ssa_env_->SetNotMerged();
            PushLoop(finish_try_env);
            SetBlockType(&control_.back(), operand);
            len = 1 + operand.length;
            break;
          }
          case kExprIf: {
            // Condition on top of stack. Split environments for branches.
            BlockTypeOperand<true> operand(this, pc_);
            Value cond = Pop(0, kWasmI32);
            TFNode* if_true = nullptr;
            TFNode* if_false = nullptr;
            BUILD(BranchNoHint, cond.node, &if_true, &if_false);
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
              error(pc_, "else does not match an if");
              break;
            }
            if (c->false_env == nullptr) {
              error(pc_, "else already present for if");
              break;
            }
            FallThruTo(c);
            stack_.resize(c->stack_depth);
            // Switch to environment for false branch.
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
              TypeCheckFallThru(c);
              if (c->unreachable) PushEndValues(c);
              PopControl();
              SetEnv("loop:end", ssa_env_);
              break;
            }
            if (c->is_if()) {
              if (c->false_env != nullptr) {
                // End the true branch of a one-armed if.
                Goto(c->false_env, c->end_env);
                if (!c->unreachable && stack_.size() != c->stack_depth) {
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
            PushEndValues(c);

            if (control_.size() == 1) {
              // If at the last (implicit) control, check we are at end.
              if (pc_ + 1 != end_) {
                error(pc_ + 1, "trailing code after function end");
                break;
              }
              last_end_found_ = true;
              if (ssa_env_->go()) {
                // The result of the block is the return value.
                TRACE("  @%-8d #xx:%-20s|", startrel(pc_), "(implicit) return");
                DoReturn();
                TRACE("\n");
              } else {
                TypeCheckFallThru(c);
              }
            }
            PopControl();
            break;
          }
          case kExprSelect: {
            Value cond = Pop(2, kWasmI32);
            Value fval = Pop();
            Value tval = Pop(0, fval.type);
            if (build()) {
              TFNode* controls[2];
              builder_->BranchNoHint(cond.node, &controls[0], &controls[1]);
              TFNode* merge = builder_->Merge(2, controls);
              TFNode* vals[2] = {tval.node, fval.node};
              TFNode* phi = builder_->Phi(tval.type, 2, vals, merge);
              Push(tval.type, phi);
              ssa_env_->control = merge;
            } else {
              Push(tval.type == kWasmVar ? fval.type : tval.type, nullptr);
            }
            break;
          }
          case kExprBr: {
            BreakDepthOperand<true> operand(this, pc_);
            if (Validate(pc_, operand, control_)) {
              BreakTo(operand.depth);
            }
            len = 1 + operand.length;
            EndControl();
            break;
          }
          case kExprBrIf: {
            BreakDepthOperand<true> operand(this, pc_);
            Value cond = Pop(0, kWasmI32);
            if (ok() && Validate(pc_, operand, control_)) {
              SsaEnv* fenv = ssa_env_;
              SsaEnv* tenv = Split(fenv);
              fenv->SetNotMerged();
              BUILD(BranchNoHint, cond.node, &tenv->control, &fenv->control);
              ssa_env_ = tenv;
              BreakTo(operand.depth);
              ssa_env_ = fenv;
            }
            len = 1 + operand.length;
            break;
          }
          case kExprBrTable: {
            BranchTableOperand<true> operand(this, pc_);
            BranchTableIterator<true> iterator(this, operand);
            if (Validate(pc_, operand, control_.size())) {
              Value key = Pop(0, kWasmI32);
              if (failed()) break;

              SsaEnv* break_env = ssa_env_;
              if (operand.table_count > 0) {
                // Build branches to the various blocks based on the table.
                TFNode* sw = BUILD(Switch, operand.table_count + 1, key.node);

                SsaEnv* copy = Steal(break_env);
                ssa_env_ = copy;
                MergeValues* merge = nullptr;
                while (ok() && iterator.has_next()) {
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

                  // Check that label types match up.
                  static MergeValues loop_dummy = {0, {nullptr}};
                  Control* c = &control_[control_.size() - target - 1];
                  MergeValues* current = c->is_loop() ? &loop_dummy : &c->merge;
                  if (i == 0) {
                    merge = current;
                  } else if (merge->arity != current->arity) {
                    errorf(pos,
                           "inconsistent arity in br_table target %d"
                           " (previous was %u, this one %u)",
                           i, merge->arity, current->arity);
                  } else if (control_.back().unreachable) {
                    for (uint32_t j = 0; ok() && j < merge->arity; ++j) {
                      if ((*merge)[j].type != (*current)[j].type) {
                        errorf(pos,
                               "type error in br_table target %d operand %d"
                               " (previous expected %s, this one %s)",
                               i, j, WasmOpcodes::TypeName((*merge)[j].type),
                               WasmOpcodes::TypeName((*current)[j].type));
                      }
                    }
                  }
                }
                if (failed()) break;
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
            EndControl();
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
          case kExprI32Const: {
            ImmI32Operand<true> operand(this, pc_);
            Push(kWasmI32, BUILD(Int32Constant, operand.value));
            len = 1 + operand.length;
            break;
          }
          case kExprI64Const: {
            ImmI64Operand<true> operand(this, pc_);
            Push(kWasmI64, BUILD(Int64Constant, operand.value));
            len = 1 + operand.length;
            break;
          }
          case kExprF32Const: {
            ImmF32Operand<true> operand(this, pc_);
            Push(kWasmF32, BUILD(Float32Constant, operand.value));
            len = 1 + operand.length;
            break;
          }
          case kExprF64Const: {
            ImmF64Operand<true> operand(this, pc_);
            Push(kWasmF64, BUILD(Float64Constant, operand.value));
            len = 1 + operand.length;
            break;
          }
          case kExprGetLocal: {
            LocalIndexOperand<true> operand(this, pc_);
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
            LocalIndexOperand<true> operand(this, pc_);
            if (Validate(pc_, operand)) {
              Value val = Pop(0, local_type_vec_[operand.index]);
              if (ssa_env_->locals) ssa_env_->locals[operand.index] = val.node;
            }
            len = 1 + operand.length;
            break;
          }
          case kExprTeeLocal: {
            LocalIndexOperand<true> operand(this, pc_);
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
            GlobalIndexOperand<true> operand(this, pc_);
            if (Validate(pc_, operand)) {
              Push(operand.type, BUILD(GetGlobal, operand.index));
            }
            len = 1 + operand.length;
            break;
          }
          case kExprSetGlobal: {
            GlobalIndexOperand<true> operand(this, pc_);
            if (Validate(pc_, operand)) {
              if (operand.global->mutability) {
                Value val = Pop(0, operand.type);
                BUILD(SetGlobal, operand.index, val.node);
              } else {
                errorf(pc_, "immutable global #%u cannot be assigned",
                       operand.index);
              }
            }
            len = 1 + operand.length;
            break;
          }
          case kExprI32LoadMem8S:
            len = DecodeLoadMem(kWasmI32, MachineType::Int8());
            break;
          case kExprI32LoadMem8U:
            len = DecodeLoadMem(kWasmI32, MachineType::Uint8());
            break;
          case kExprI32LoadMem16S:
            len = DecodeLoadMem(kWasmI32, MachineType::Int16());
            break;
          case kExprI32LoadMem16U:
            len = DecodeLoadMem(kWasmI32, MachineType::Uint16());
            break;
          case kExprI32LoadMem:
            len = DecodeLoadMem(kWasmI32, MachineType::Int32());
            break;
          case kExprI64LoadMem8S:
            len = DecodeLoadMem(kWasmI64, MachineType::Int8());
            break;
          case kExprI64LoadMem8U:
            len = DecodeLoadMem(kWasmI64, MachineType::Uint8());
            break;
          case kExprI64LoadMem16S:
            len = DecodeLoadMem(kWasmI64, MachineType::Int16());
            break;
          case kExprI64LoadMem16U:
            len = DecodeLoadMem(kWasmI64, MachineType::Uint16());
            break;
          case kExprI64LoadMem32S:
            len = DecodeLoadMem(kWasmI64, MachineType::Int32());
            break;
          case kExprI64LoadMem32U:
            len = DecodeLoadMem(kWasmI64, MachineType::Uint32());
            break;
          case kExprI64LoadMem:
            len = DecodeLoadMem(kWasmI64, MachineType::Int64());
            break;
          case kExprF32LoadMem:
            len = DecodeLoadMem(kWasmF32, MachineType::Float32());
            break;
          case kExprF64LoadMem:
            len = DecodeLoadMem(kWasmF64, MachineType::Float64());
            break;
          case kExprS128LoadMem:
            CHECK_PROTOTYPE_OPCODE(wasm_simd_prototype);
            len = DecodeLoadMem(kWasmS128, MachineType::Simd128());
            break;
          case kExprI32StoreMem8:
            len = DecodeStoreMem(kWasmI32, MachineType::Int8());
            break;
          case kExprI32StoreMem16:
            len = DecodeStoreMem(kWasmI32, MachineType::Int16());
            break;
          case kExprI32StoreMem:
            len = DecodeStoreMem(kWasmI32, MachineType::Int32());
            break;
          case kExprI64StoreMem8:
            len = DecodeStoreMem(kWasmI64, MachineType::Int8());
            break;
          case kExprI64StoreMem16:
            len = DecodeStoreMem(kWasmI64, MachineType::Int16());
            break;
          case kExprI64StoreMem32:
            len = DecodeStoreMem(kWasmI64, MachineType::Int32());
            break;
          case kExprI64StoreMem:
            len = DecodeStoreMem(kWasmI64, MachineType::Int64());
            break;
          case kExprF32StoreMem:
            len = DecodeStoreMem(kWasmF32, MachineType::Float32());
            break;
          case kExprF64StoreMem:
            len = DecodeStoreMem(kWasmF64, MachineType::Float64());
            break;
          case kExprS128StoreMem:
            CHECK_PROTOTYPE_OPCODE(wasm_simd_prototype);
            len = DecodeStoreMem(kWasmS128, MachineType::Simd128());
            break;
          case kExprGrowMemory: {
            if (!CheckHasMemory()) break;
            MemoryIndexOperand<true> operand(this, pc_);
            DCHECK_NOT_NULL(module_);
            if (module_->is_wasm()) {
              Value val = Pop(0, kWasmI32);
              Push(kWasmI32, BUILD(GrowMemory, val.node));
            } else {
              error("grow_memory is not supported for asmjs modules");
            }
            len = 1 + operand.length;
            break;
          }
          case kExprMemorySize: {
            if (!CheckHasMemory()) break;
            MemoryIndexOperand<true> operand(this, pc_);
            Push(kWasmI32, BUILD(CurrentMemoryPages));
            len = 1 + operand.length;
            break;
          }
          case kExprCallFunction: {
            CallFunctionOperand<true> operand(this, pc_);
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
            CallIndirectOperand<true> operand(this, pc_);
            if (Validate(pc_, operand)) {
              Value index = Pop(0, kWasmI32);
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
            byte simd_index = read_u8<true>(pc_ + 1, "simd index");
            opcode = static_cast<WasmOpcode>(opcode << 8 | simd_index);
            TRACE("  @%-4d #%-20s|", startrel(pc_),
                  WasmOpcodes::OpcodeName(opcode));
            len += DecodeSimdOpcode(opcode);
            break;
          }
          case kAtomicPrefix: {
            if (module_ == nullptr || !module_->is_asm_js()) {
              error("Atomics are allowed only in AsmJs modules");
              break;
            }
            if (!FLAG_wasm_atomics_prototype) {
              error("Invalid opcode (enable with --wasm_atomics_prototype)");
              break;
            }
            len = 2;
            byte atomic_opcode = read_u8<true>(pc_ + 1, "atomic index");
            opcode = static_cast<WasmOpcode>(opcode << 8 | atomic_opcode);
            sig = WasmOpcodes::AtomicSignature(opcode);
            if (sig) {
              BuildAtomicOperator(opcode);
            }
            break;
          }
          default: {
            // Deal with special asmjs opcodes.
            if (module_ != nullptr && module_->is_asm_js()) {
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
        PrintF(" ");
        for (size_t i = 0; i < control_.size(); ++i) {
          Control* c = &control_[i];
          enum ControlKind {
            kControlIf,
            kControlBlock,
            kControlLoop,
            kControlTry
          };
          switch (c->kind) {
            case kControlIf:
              PrintF("I");
              break;
            case kControlBlock:
              PrintF("B");
              break;
            case kControlLoop:
              PrintF("L");
              break;
            case kControlTry:
              PrintF("T");
              break;
            default:
              break;
          }
          PrintF("%u", c->merge.arity);
          if (c->unreachable) PrintF("*");
        }
        PrintF(" | ");
        for (size_t i = 0; i < stack_.size(); ++i) {
          Value& val = stack_[i];
          WasmOpcode opcode = static_cast<WasmOpcode>(*val.pc);
          if (WasmOpcodes::IsPrefixOpcode(opcode)) {
            opcode = static_cast<WasmOpcode>(opcode << 8 | *(val.pc + 1));
          }
          PrintF(" %c@%d:%s", WasmOpcodes::ShortNameOf(val.type),
                 static_cast<int>(val.pc - start_),
                 WasmOpcodes::OpcodeName(opcode));
          switch (opcode) {
            case kExprI32Const: {
              ImmI32Operand<true> operand(this, val.pc);
              PrintF("[%d]", operand.value);
              break;
            }
            case kExprGetLocal: {
              LocalIndexOperand<true> operand(this, val.pc);
              PrintF("[%u]", operand.index);
              break;
            }
            case kExprSetLocal:  // fallthru
            case kExprTeeLocal: {
              LocalIndexOperand<true> operand(this, val.pc);
              PrintF("[%u]", operand.index);
              break;
            }
            default:
              break;
          }
          if (val.node == nullptr) PrintF("?");
        }
        PrintF("\n");
      }
#endif
      pc_ += len;
    }  // end decode loop
    if (pc_ > end_ && ok()) error("Beyond end of code");
  }

  void EndControl() {
    ssa_env_->Kill(SsaEnv::kControlEnd);
    if (!control_.empty()) {
      stack_.resize(control_.back().stack_depth);
      control_.back().unreachable = true;
    }
  }

  void SetBlockType(Control* c, BlockTypeOperand<true>& operand) {
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

  ValueType GetReturnType(FunctionSig* sig) {
    return sig->return_count() == 0 ? kWasmStmt : sig->GetReturn();
  }

  void PushBlock(SsaEnv* end_env) {
    control_.emplace_back(
        Control::Block(pc_, stack_.size(), end_env, current_catch_));
  }

  void PushLoop(SsaEnv* end_env) {
    control_.emplace_back(
        Control::Loop(pc_, stack_.size(), end_env, current_catch_));
  }

  void PushIf(SsaEnv* end_env, SsaEnv* false_env) {
    control_.emplace_back(
        Control::If(pc_, stack_.size(), end_env, false_env, current_catch_));
  }

  void PushTry(SsaEnv* end_env, SsaEnv* catch_env) {
    control_.emplace_back(Control::Try(pc_, stack_.size(), end_env, zone_,
                                       catch_env, current_catch_));
    current_catch_ = static_cast<int32_t>(control_.size() - 1);
  }

  void PopControl() { control_.pop_back(); }

  int DecodeLoadMem(ValueType type, MachineType mem_type) {
    if (!CheckHasMemory()) return 0;
    MemoryAccessOperand<true> operand(
        this, pc_, ElementSizeLog2Of(mem_type.representation()));

    Value index = Pop(0, kWasmI32);
    TFNode* node = BUILD(LoadMem, type, mem_type, index.node, operand.offset,
                         operand.alignment, position());
    Push(type, node);
    return 1 + operand.length;
  }

  int DecodeStoreMem(ValueType type, MachineType mem_type) {
    if (!CheckHasMemory()) return 0;
    MemoryAccessOperand<true> operand(
        this, pc_, ElementSizeLog2Of(mem_type.representation()));
    Value val = Pop(1, type);
    Value index = Pop(0, kWasmI32);
    BUILD(StoreMem, mem_type, index.node, operand.offset, operand.alignment,
          val.node, position());
    return 1 + operand.length;
  }

  unsigned SimdExtractLane(WasmOpcode opcode, ValueType type) {
    SimdLaneOperand<true> operand(this, pc_);
    if (Validate(pc_, opcode, operand)) {
      compiler::NodeVector inputs(1, zone_);
      inputs[0] = Pop(0, ValueType::kSimd128).node;
      TFNode* node = BUILD(SimdLaneOp, opcode, operand.lane, inputs);
      Push(type, node);
    }
    return operand.length;
  }

  unsigned SimdReplaceLane(WasmOpcode opcode, ValueType type) {
    SimdLaneOperand<true> operand(this, pc_);
    if (Validate(pc_, opcode, operand)) {
      compiler::NodeVector inputs(2, zone_);
      inputs[1] = Pop(1, type).node;
      inputs[0] = Pop(0, ValueType::kSimd128).node;
      TFNode* node = BUILD(SimdLaneOp, opcode, operand.lane, inputs);
      Push(ValueType::kSimd128, node);
    }
    return operand.length;
  }

  unsigned SimdShiftOp(WasmOpcode opcode) {
    SimdShiftOperand<true> operand(this, pc_);
    if (Validate(pc_, opcode, operand)) {
      compiler::NodeVector inputs(1, zone_);
      inputs[0] = Pop(0, ValueType::kSimd128).node;
      TFNode* node = BUILD(SimdShiftOp, opcode, operand.shift, inputs);
      Push(ValueType::kSimd128, node);
    }
    return operand.length;
  }

  unsigned SimdShuffleOp(WasmOpcode opcode) {
    SimdShuffleOperand<true> operand(this, pc_, GetShuffleMaskSize(opcode));
    if (Validate(pc_, opcode, operand)) {
      compiler::NodeVector inputs(2, zone_);
      inputs[1] = Pop(1, ValueType::kSimd128).node;
      inputs[0] = Pop(0, ValueType::kSimd128).node;
      TFNode* node =
          BUILD(SimdShuffleOp, operand.shuffle, operand.lanes, inputs);
      Push(ValueType::kSimd128, node);
    }
    return operand.lanes;
  }

  unsigned DecodeSimdOpcode(WasmOpcode opcode) {
    unsigned len = 0;
    switch (opcode) {
      case kExprF32x4ExtractLane: {
        len = SimdExtractLane(opcode, ValueType::kFloat32);
        break;
      }
      case kExprI32x4ExtractLane:
      case kExprI16x8ExtractLane:
      case kExprI8x16ExtractLane: {
        len = SimdExtractLane(opcode, ValueType::kWord32);
        break;
      }
      case kExprF32x4ReplaceLane: {
        len = SimdReplaceLane(opcode, ValueType::kFloat32);
        break;
      }
      case kExprI32x4ReplaceLane:
      case kExprI16x8ReplaceLane:
      case kExprI8x16ReplaceLane: {
        len = SimdReplaceLane(opcode, ValueType::kWord32);
        break;
      }
      case kExprI32x4Shl:
      case kExprI32x4ShrS:
      case kExprI32x4ShrU:
      case kExprI16x8Shl:
      case kExprI16x8ShrS:
      case kExprI16x8ShrU:
      case kExprI8x16Shl:
      case kExprI8x16ShrS:
      case kExprI8x16ShrU: {
        len = SimdShiftOp(opcode);
        break;
      }
      case kExprS32x4Shuffle:
      case kExprS16x8Shuffle:
      case kExprS8x16Shuffle: {
        len = SimdShuffleOp(opcode);
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

  void BuildAtomicOperator(WasmOpcode opcode) { UNIMPLEMENTED(); }

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

  void Push(ValueType type, TFNode* node) {
    if (type != kWasmStmt) {
      stack_.push_back({pc_, node, type});
    }
  }

  void PushEndValues(Control* c) {
    DCHECK_EQ(c, &control_.back());
    stack_.resize(c->stack_depth);
    if (c->merge.arity == 1) {
      stack_.push_back(c->merge.vals.first);
    } else {
      for (unsigned i = 0; i < c->merge.arity; i++) {
        stack_.push_back(c->merge.vals.array[i]);
      }
    }
    DCHECK_EQ(c->stack_depth + c->merge.arity, stack_.size());
  }

  void PushReturns(FunctionSig* sig, TFNode** rets) {
    for (size_t i = 0; i < sig->return_count(); i++) {
      // When verifying only, then {rets} will be null, so push null.
      Push(sig->GetReturn(i), rets ? rets[i] : nullptr);
    }
  }

  const char* SafeOpcodeNameAt(const byte* pc) {
    if (pc >= end_) return "<end>";
    return WasmOpcodes::OpcodeName(static_cast<WasmOpcode>(*pc));
  }

  Value Pop(int index, ValueType expected) {
    Value val = Pop();
    if (val.type != expected && val.type != kWasmVar && expected != kWasmVar) {
      errorf(val.pc, "%s[%d] expected type %s, found %s of type %s",
             SafeOpcodeNameAt(pc_), index, WasmOpcodes::TypeName(expected),
             SafeOpcodeNameAt(val.pc), WasmOpcodes::TypeName(val.type));
    }
    return val;
  }

  Value Pop() {
    size_t limit = control_.empty() ? 0 : control_.back().stack_depth;
    if (stack_.size() <= limit) {
      // Popping past the current control start in reachable code.
      Value val = {pc_, nullptr, kWasmVar};
      if (!control_.back().unreachable) {
        errorf(pc_, "%s found empty stack", SafeOpcodeNameAt(pc_));
      }
      return val;
    }
    Value val = stack_.back();
    stack_.pop_back();
    return val;
  }

  int baserel(const byte* ptr) {
    return base_ ? static_cast<int>(ptr - base_) : 0;
  }

  int startrel(const byte* ptr) { return static_cast<int>(ptr - start_); }

  void BreakTo(unsigned depth) {
    Control* c = &control_[control_.size() - depth - 1];
    if (c->is_loop()) {
      // This is the inner loop block, which does not have a value.
      Goto(ssa_env_, c->end_env);
    } else {
      // Merge the value(s) into the end of the block.
      size_t expected = control_.back().stack_depth + c->merge.arity;
      if (stack_.size() < expected && !control_.back().unreachable) {
        errorf(
            pc_,
            "expected at least %u values on the stack for br to @%d, found %d",
            c->merge.arity, startrel(c->pc),
            static_cast<int>(stack_.size() - c->stack_depth));
        return;
      }
      MergeValuesInto(c);
    }
  }

  void FallThruTo(Control* c) {
    DCHECK_EQ(c, &control_.back());
    // Merge the value(s) into the end of the block.
    size_t expected = c->stack_depth + c->merge.arity;
    if (stack_.size() == expected ||
        (stack_.size() < expected && c->unreachable)) {
      MergeValuesInto(c);
      c->unreachable = false;
      return;
    }
    errorf(pc_, "expected %u elements on the stack for fallthru to @%d",
           c->merge.arity, startrel(c->pc));
  }

  inline Value& GetMergeValueFromStack(Control* c, size_t i) {
    return stack_[stack_.size() - c->merge.arity + i];
  }

  void TypeCheckFallThru(Control* c) {
    DCHECK_EQ(c, &control_.back());
    // Fallthru must match arity exactly.
    int arity = static_cast<int>(c->merge.arity);
    if (c->stack_depth + arity < stack_.size() ||
        (c->stack_depth + arity != stack_.size() && !c->unreachable)) {
      errorf(pc_, "expected %d elements on the stack for fallthru to @%d",
             arity, startrel(c->pc));
      return;
    }
    // Typecheck the values left on the stack.
    size_t avail = stack_.size() - c->stack_depth;
    for (size_t i = avail >= c->merge.arity ? 0 : c->merge.arity - avail;
         i < c->merge.arity; i++) {
      Value& val = GetMergeValueFromStack(c, i);
      Value& old = c->merge[i];
      if (val.type != old.type) {
        errorf(pc_, "type error in merge[%zu] (expected %s, got %s)", i,
               WasmOpcodes::TypeName(old.type),
               WasmOpcodes::TypeName(val.type));
        return;
      }
    }
  }

  void MergeValuesInto(Control* c) {
    SsaEnv* target = c->end_env;
    bool first = target->state == SsaEnv::kUnreachable;
    bool reachable = ssa_env_->go();
    Goto(ssa_env_, target);

    size_t avail = stack_.size() - control_.back().stack_depth;
    for (size_t i = avail >= c->merge.arity ? 0 : c->merge.arity - avail;
         i < c->merge.arity; i++) {
      Value& val = GetMergeValueFromStack(c, i);
      Value& old = c->merge[i];
      if (val.type != old.type && val.type != kWasmVar) {
        errorf(pc_, "type error in merge[%zu] (expected %s, got %s)", i,
               WasmOpcodes::TypeName(old.type),
               WasmOpcodes::TypeName(val.type));
        return;
      }
      if (builder_ && reachable) {
        DCHECK_NOT_NULL(val.node);
        old.node =
            first ? val.node : CreateOrMergeIntoPhi(old.type, target->control,
                                                    old.node, val.node);
      }
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
      PrintF("{set_env = %p, state = %c, reason = %s", static_cast<void*>(env),
             state, reason);
      if (env && env->control) {
        PrintF(", control = ");
        compiler::WasmGraphBuilder::PrintDebugName(env->control);
      }
      PrintF("}\n");
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
          CreateOrMergeIntoPhi(kWasmI32, try_info->catch_env->control,
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

  TFNode* CreateOrMergeIntoPhi(ValueType type, TFNode* merge, TFNode* tnode,
                               TFNode* fnode) {
    DCHECK_NOT_NULL(builder_);
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

  SsaEnv* PrepareForLoop(const byte* pc, SsaEnv* env) {
    if (!builder_) return Split(env);
    if (!env->go()) return Split(env);
    env->state = SsaEnv::kMerged;

    env->control = builder_->Loop(env->control);
    env->effect = builder_->EffectPhi(1, &env->effect, env->control);
    builder_->Terminate(env->effect, env->control);
    BitVector* assigned = AnalyzeLoopAssignment(
        this, pc, static_cast<int>(total_locals()), zone_);
    if (failed()) return env;
    if (assigned != nullptr) {
      // Only introduce phis for variables assigned in this loop.
      for (int i = EnvironmentCount() - 1; i >= 0; i--) {
        if (!assigned->Contains(i)) continue;
        env->locals[i] =
            builder_->Phi(local_type_vec_[i], 1, &env->locals[i], env->control);
      }
      SsaEnv* loop_body_env = Split(env);
      builder_->StackCheck(position(), &(loop_body_env->effect),
                           &(loop_body_env->control));
      return loop_body_env;
    }

    // Conservatively introduce phis for all local variables.
    for (int i = EnvironmentCount() - 1; i >= 0; i--) {
      env->locals[i] =
          builder_->Phi(local_type_vec_[i], 1, &env->locals[i], env->control);
    }

    SsaEnv* loop_body_env = Split(env);
    builder_->StackCheck(position(), &(loop_body_env->effect),
                         &(loop_body_env->control));
    return loop_body_env;
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
    end_ = pc_;          // Terminate decoding loop.
    builder_ = nullptr;  // Don't build any more nodes.
    TRACE(" !%s\n", error_msg_.c_str());
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

bool DecodeLocalDecls(BodyLocalDecls* decls, const byte* start,
                      const byte* end) {
  Decoder decoder(start, end);
  if (WasmDecoder::DecodeLocals(&decoder, nullptr, &decls->type_list)) {
    DCHECK(decoder.ok());
    decls->encoded_size = decoder.pc_offset();
    return true;
  }
  return false;
}

BytecodeIterator::BytecodeIterator(const byte* start, const byte* end,
                                   BodyLocalDecls* decls)
    : Decoder(start, end) {
  if (decls != nullptr) {
    if (DecodeLocalDecls(decls, start, end)) {
      pc_ += decls->encoded_size;
      if (pc_ > end_) pc_ = end_;
    }
  }
}

DecodeResult VerifyWasmCode(AccountingAllocator* allocator,
                            const wasm::WasmModule* module,
                            FunctionBody& body) {
  Zone zone(allocator, ZONE_NAME);
  WasmFullDecoder decoder(&zone, module, body);
  decoder.Decode();
  return decoder.toResult<DecodeStruct*>(nullptr);
}

DecodeResult BuildTFGraph(AccountingAllocator* allocator, TFBuilder* builder,
                          FunctionBody& body) {
  Zone zone(allocator, ZONE_NAME);
  WasmFullDecoder decoder(&zone, builder, body);
  decoder.Decode();
  return decoder.toResult<DecodeStruct*>(nullptr);
}

unsigned OpcodeLength(const byte* pc, const byte* end) {
  Decoder decoder(pc, end);
  return WasmDecoder::OpcodeLength(&decoder, pc);
}

std::pair<uint32_t, uint32_t> StackEffect(const WasmModule* module,
                                          FunctionSig* sig, const byte* pc,
                                          const byte* end) {
  WasmDecoder decoder(module, sig, pc, end);
  return decoder.StackEffect(pc);
}

void PrintRawWasmCode(const byte* start, const byte* end) {
  AccountingAllocator allocator;
  PrintRawWasmCode(&allocator, FunctionBodyForTesting(start, end), nullptr);
}

namespace {
const char* RawOpcodeName(WasmOpcode opcode) {
  switch (opcode) {
#define DECLARE_NAME_CASE(name, opcode, sig) \
  case kExpr##name:                          \
    return "kExpr" #name;
    FOREACH_OPCODE(DECLARE_NAME_CASE)
#undef DECLARE_NAME_CASE
    default:
      break;
  }
  return "Unknown";
}
}  // namespace

bool PrintRawWasmCode(AccountingAllocator* allocator, const FunctionBody& body,
                      const wasm::WasmModule* module) {
  OFStream os(stdout);
  Zone zone(allocator, ZONE_NAME);
  WasmFullDecoder decoder(&zone, module, body);
  int line_nr = 0;

  // Print the function signature.
  if (body.sig) {
    os << "// signature: " << *body.sig << std::endl;
    ++line_nr;
  }

  // Print the local declarations.
  BodyLocalDecls decls(&zone);
  BytecodeIterator i(body.start, body.end, &decls);
  if (body.start != i.pc() && !FLAG_wasm_code_fuzzer_gen_test) {
    os << "// locals: ";
    if (!decls.type_list.empty()) {
      ValueType type = decls.type_list[0];
      uint32_t count = 0;
      for (size_t pos = 0; pos < decls.type_list.size(); ++pos) {
        if (decls.type_list[pos] == type) {
          ++count;
        } else {
          os << " " << count << " " << WasmOpcodes::TypeName(type);
          type = decls.type_list[pos];
          count = 1;
        }
      }
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
    unsigned length = WasmDecoder::OpcodeLength(&decoder, i.pc());

    WasmOpcode opcode = i.current();
    if (opcode == kExprElse) control_depth--;

    int num_whitespaces = control_depth < 32 ? 2 * control_depth : 64;

    // 64 whitespaces
    const char* padding =
        "                                                                ";
    os.write(padding, num_whitespaces);

    os << RawOpcodeName(opcode) << ",";

    for (size_t j = 1; j < length; ++j) {
      os << " 0x" << AsHex(i.pc()[j], 2) << ",";
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
        BlockTypeOperand<true> operand(&i, i.pc());
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
        BreakDepthOperand<true> operand(&i, i.pc());
        os << "   // depth=" << operand.depth;
        break;
      }
      case kExprBrIf: {
        BreakDepthOperand<true> operand(&i, i.pc());
        os << "   // depth=" << operand.depth;
        break;
      }
      case kExprBrTable: {
        BranchTableOperand<true> operand(&i, i.pc());
        os << " // entries=" << operand.table_count;
        break;
      }
      case kExprCallIndirect: {
        CallIndirectOperand<true> operand(&i, i.pc());
        os << "   // sig #" << operand.index;
        if (decoder.Complete(i.pc(), operand)) {
          os << ": " << *operand.sig;
        }
        break;
      }
      case kExprCallFunction: {
        CallFunctionOperand<true> operand(&i, i.pc());
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
  Decoder decoder(start, end);
  return WasmDecoder::AnalyzeLoopAssignment(&decoder, start,
                                            static_cast<int>(num_locals), zone);
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
