// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/wasm/asm-wasm-builder.h"
#include "src/wasm/wasm-macro-gen.h"
#include "src/wasm/wasm-opcodes.h"

#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/codegen.h"
#include "src/type-cache.h"

namespace v8 {
namespace internal {
namespace wasm {

#define RECURSE(call)               \
  do {                              \
    DCHECK(!HasStackOverflow());    \
    call;                           \
    if (HasStackOverflow()) return; \
  } while (false)


class AsmWasmBuilderImpl : public AstVisitor {
 public:
  AsmWasmBuilderImpl(Isolate* isolate, Zone* zone, FunctionLiteral* literal)
      : local_variables_(HashMap::PointersMatch,
                         ZoneHashMap::kDefaultHashMapCapacity,
                         ZoneAllocationPolicy(zone)),
        functions_(HashMap::PointersMatch, ZoneHashMap::kDefaultHashMapCapacity,
                   ZoneAllocationPolicy(zone)),
        global_variables_(HashMap::PointersMatch,
                          ZoneHashMap::kDefaultHashMapCapacity,
                          ZoneAllocationPolicy(zone)),
        in_function_(false),
        is_set_op_(false),
        marking_exported(false),
        builder_(new (zone) WasmModuleBuilder(zone)),
        current_function_builder_(nullptr),
        literal_(literal),
        isolate_(isolate),
        zone_(zone),
        cache_(TypeCache::Get()),
        breakable_blocks_(zone),
        block_size_(0),
        init_function_index(0) {
    InitializeAstVisitor(isolate);
  }

  void InitializeInitFunction() {
    unsigned char init[] = "__init__";
    init_function_index = builder_->AddFunction();
    current_function_builder_ = builder_->FunctionAt(init_function_index);
    current_function_builder_->SetName(init, 8);
    current_function_builder_->ReturnType(kAstStmt);
    current_function_builder_->Exported(1);
    current_function_builder_ = nullptr;
  }

  void Compile() {
    InitializeInitFunction();
    RECURSE(VisitFunctionLiteral(literal_));
  }

  void VisitVariableDeclaration(VariableDeclaration* decl) {}

  void VisitFunctionDeclaration(FunctionDeclaration* decl) {
    DCHECK(!in_function_);
    DCHECK(current_function_builder_ == nullptr);
    uint16_t index = LookupOrInsertFunction(decl->proxy()->var());
    current_function_builder_ = builder_->FunctionAt(index);
    in_function_ = true;
    RECURSE(Visit(decl->fun()));
    in_function_ = false;
    current_function_builder_ = nullptr;
    local_variables_.Clear();
  }

  void VisitImportDeclaration(ImportDeclaration* decl) {}

  void VisitExportDeclaration(ExportDeclaration* decl) {}

  void VisitStatements(ZoneList<Statement*>* stmts) {
    for (int i = 0; i < stmts->length(); ++i) {
      Statement* stmt = stmts->at(i);
      RECURSE(Visit(stmt));
      if (stmt->IsJump()) break;
    }
  }

  void VisitBlock(Block* stmt) {
    if (stmt->statements()->length() == 1) {
      ExpressionStatement* expr =
          stmt->statements()->at(0)->AsExpressionStatement();
      if (expr != nullptr) {
        if (expr->expression()->IsAssignment()) {
          RECURSE(VisitExpressionStatement(expr));
          return;
        }
      }
    }
    DCHECK(in_function_);
    BlockVisitor visitor(this, stmt->AsBreakableStatement(), kExprBlock, false,
                         static_cast<byte>(stmt->statements()->length()));
    RECURSE(VisitStatements(stmt->statements()));
    DCHECK(block_size_ >= 0);
  }

  class BlockVisitor {
   private:
    int prev_block_size_;
    uint32_t index_;
    AsmWasmBuilderImpl* builder_;

   public:
    BlockVisitor(AsmWasmBuilderImpl* builder, BreakableStatement* stmt,
                 WasmOpcode opcode, bool is_loop, int initial_block_size)
        : builder_(builder) {
      builder_->breakable_blocks_.push_back(std::make_pair(stmt, is_loop));
      builder_->current_function_builder_->Emit(opcode);
      index_ = builder_->current_function_builder_->EmitEditableImmediate(0);
      prev_block_size_ = builder_->block_size_;
      builder_->block_size_ = initial_block_size;
    }
    ~BlockVisitor() {
      builder_->current_function_builder_->EditImmediate(index_,
                                                         builder_->block_size_);
      builder_->block_size_ = prev_block_size_;
      builder_->breakable_blocks_.pop_back();
    }
  };

  void VisitExpressionStatement(ExpressionStatement* stmt) {
    RECURSE(Visit(stmt->expression()));
  }

  void VisitEmptyStatement(EmptyStatement* stmt) {}

  void VisitEmptyParentheses(EmptyParentheses* paren) { UNREACHABLE(); }

  void VisitIfStatement(IfStatement* stmt) {
    DCHECK(in_function_);
    if (stmt->HasElseStatement()) {
      current_function_builder_->Emit(kExprIfElse);
    } else {
      current_function_builder_->Emit(kExprIf);
    }
    RECURSE(Visit(stmt->condition()));
    if (stmt->HasThenStatement()) {
      RECURSE(Visit(stmt->then_statement()));
    } else {
      current_function_builder_->Emit(kExprNop);
    }
    if (stmt->HasElseStatement()) {
      RECURSE(Visit(stmt->else_statement()));
    }
  }

  void VisitContinueStatement(ContinueStatement* stmt) {
    DCHECK(in_function_);
    DCHECK(stmt->target() != NULL);
    int i = static_cast<int>(breakable_blocks_.size()) - 1;
    int block_distance = 0;
    for (; i >= 0; i--) {
      auto elem = breakable_blocks_.at(i);
      if (elem.first == stmt->target()) {
        DCHECK(elem.second);
        break;
      } else if (elem.second) {
        block_distance += 2;
      } else {
        block_distance += 1;
      }
    }
    DCHECK(i >= 0);
    current_function_builder_->EmitWithU8(kExprBr, block_distance);
    current_function_builder_->Emit(kExprNop);
  }

  void VisitBreakStatement(BreakStatement* stmt) {
    DCHECK(in_function_);
    DCHECK(stmt->target() != NULL);
    int i = static_cast<int>(breakable_blocks_.size()) - 1;
    int block_distance = 0;
    for (; i >= 0; i--) {
      auto elem = breakable_blocks_.at(i);
      if (elem.first == stmt->target()) {
        if (elem.second) {
          block_distance++;
        }
        break;
      } else if (elem.second) {
        block_distance += 2;
      } else {
        block_distance += 1;
      }
    }
    DCHECK(i >= 0);
    current_function_builder_->EmitWithU8(kExprBr, block_distance);
    current_function_builder_->Emit(kExprNop);
  }

  void VisitReturnStatement(ReturnStatement* stmt) {
    if (in_function_) {
      current_function_builder_->Emit(kExprReturn);
    } else {
      marking_exported = true;
    }
    RECURSE(Visit(stmt->expression()));
    if (!in_function_) {
      marking_exported = false;
    }
  }

  void VisitWithStatement(WithStatement* stmt) { UNREACHABLE(); }

  void SetLocalTo(uint16_t index, int value) {
    current_function_builder_->Emit(kExprSetLocal);
    AddLeb128(index, true);
    byte code[] = {WASM_I32(value)};
    current_function_builder_->EmitCode(code, sizeof(code));
    block_size_++;
  }

  void CompileCase(CaseClause* clause, uint16_t fall_through,
                   VariableProxy* tag) {
    Literal* label = clause->label()->AsLiteral();
    DCHECK(label != nullptr);
    block_size_++;
    current_function_builder_->Emit(kExprIf);
    current_function_builder_->Emit(kExprI32Ior);
    current_function_builder_->Emit(kExprI32Eq);
    VisitVariableProxy(tag);
    VisitLiteral(label);
    current_function_builder_->Emit(kExprGetLocal);
    AddLeb128(fall_through, true);
    BlockVisitor visitor(this, nullptr, kExprBlock, false, 0);
    SetLocalTo(fall_through, 1);
    ZoneList<Statement*>* stmts = clause->statements();
    block_size_ += stmts->length();
    RECURSE(VisitStatements(stmts));
  }

  void VisitSwitchStatement(SwitchStatement* stmt) {
    VariableProxy* tag = stmt->tag()->AsVariableProxy();
    DCHECK(tag != NULL);
    BlockVisitor visitor(this, stmt->AsBreakableStatement(), kExprBlock, false,
                         0);
    uint16_t fall_through = current_function_builder_->AddLocal(kAstI32);
    SetLocalTo(fall_through, 0);

    ZoneList<CaseClause*>* clauses = stmt->cases();
    for (int i = 0; i < clauses->length(); ++i) {
      CaseClause* clause = clauses->at(i);
      if (!clause->is_default()) {
        CompileCase(clause, fall_through, tag);
      } else {
        ZoneList<Statement*>* stmts = clause->statements();
        block_size_ += stmts->length();
        RECURSE(VisitStatements(stmts));
      }
    }
  }

  void VisitCaseClause(CaseClause* clause) { UNREACHABLE(); }

  void VisitDoWhileStatement(DoWhileStatement* stmt) {
    DCHECK(in_function_);
    BlockVisitor visitor(this, stmt->AsBreakableStatement(), kExprLoop, true,
                         2);
    RECURSE(Visit(stmt->body()));
    current_function_builder_->Emit(kExprIf);
    RECURSE(Visit(stmt->cond()));
    current_function_builder_->EmitWithU8(kExprBr, 0);
    current_function_builder_->Emit(kExprNop);
  }

  void VisitWhileStatement(WhileStatement* stmt) {
    DCHECK(in_function_);
    BlockVisitor visitor(this, stmt->AsBreakableStatement(), kExprLoop, true,
                         1);
    current_function_builder_->Emit(kExprIf);
    RECURSE(Visit(stmt->cond()));
    current_function_builder_->EmitWithU8(kExprBr, 0);
    RECURSE(Visit(stmt->body()));
  }

  void VisitForStatement(ForStatement* stmt) {
    DCHECK(in_function_);
    if (stmt->init() != nullptr) {
      block_size_++;
      RECURSE(Visit(stmt->init()));
    }
    BlockVisitor visitor(this, stmt->AsBreakableStatement(), kExprLoop, true,
                         0);
    if (stmt->cond() != nullptr) {
      block_size_++;
      current_function_builder_->Emit(kExprIf);
      current_function_builder_->Emit(kExprBoolNot);
      RECURSE(Visit(stmt->cond()));
      current_function_builder_->EmitWithU8(kExprBr, 1);
      current_function_builder_->Emit(kExprNop);
    }
    if (stmt->body() != nullptr) {
      block_size_++;
      RECURSE(Visit(stmt->body()));
    }
    if (stmt->next() != nullptr) {
      block_size_++;
      RECURSE(Visit(stmt->next()));
    }
    block_size_++;
    current_function_builder_->EmitWithU8(kExprBr, 0);
    current_function_builder_->Emit(kExprNop);
  }

  void VisitForInStatement(ForInStatement* stmt) { UNREACHABLE(); }

  void VisitForOfStatement(ForOfStatement* stmt) { UNREACHABLE(); }

  void VisitTryCatchStatement(TryCatchStatement* stmt) { UNREACHABLE(); }

  void VisitTryFinallyStatement(TryFinallyStatement* stmt) { UNREACHABLE(); }

  void VisitDebuggerStatement(DebuggerStatement* stmt) { UNREACHABLE(); }

  void VisitFunctionLiteral(FunctionLiteral* expr) {
    Scope* scope = expr->scope();
    if (in_function_) {
      if (expr->bounds().lower->IsFunction()) {
        Type::FunctionType* func_type = expr->bounds().lower->AsFunction();
        LocalType return_type = TypeFrom(func_type->Result());
        current_function_builder_->ReturnType(return_type);
        for (int i = 0; i < expr->parameter_count(); i++) {
          LocalType type = TypeFrom(func_type->Parameter(i));
          DCHECK(type != kAstStmt);
          LookupOrInsertLocal(scope->parameter(i), type);
        }
      } else {
        UNREACHABLE();
      }
    }
    RECURSE(VisitDeclarations(scope->declarations()));
    RECURSE(VisitStatements(expr->body()));
  }

  void VisitNativeFunctionLiteral(NativeFunctionLiteral* expr) {
    UNREACHABLE();
  }

  void VisitConditional(Conditional* expr) {
    DCHECK(in_function_);
    current_function_builder_->Emit(kExprIfElse);
    RECURSE(Visit(expr->condition()));
    RECURSE(Visit(expr->then_expression()));
    RECURSE(Visit(expr->else_expression()));
  }

  void VisitVariableProxy(VariableProxy* expr) {
    if (in_function_) {
      Variable* var = expr->var();
      if (var->is_function()) {
        DCHECK(!is_set_op_);
        std::vector<uint8_t> index =
            UnsignedLEB128From(LookupOrInsertFunction(var));
        current_function_builder_->EmitCode(
            &index[0], static_cast<uint32_t>(index.size()));
      } else {
        if (is_set_op_) {
          if (var->IsContextSlot()) {
            current_function_builder_->Emit(kExprStoreGlobal);
          } else {
            current_function_builder_->Emit(kExprSetLocal);
          }
          is_set_op_ = false;
        } else {
          if (var->IsContextSlot()) {
            current_function_builder_->Emit(kExprLoadGlobal);
          } else {
            current_function_builder_->Emit(kExprGetLocal);
          }
        }
        LocalType var_type = TypeOf(expr);
        DCHECK(var_type != kAstStmt);
        if (var->IsContextSlot()) {
          AddLeb128(LookupOrInsertGlobal(var, var_type), false);
        } else {
          AddLeb128(LookupOrInsertLocal(var, var_type), true);
        }
      }
    }
  }

  void VisitLiteral(Literal* expr) {
    if (in_function_) {
      if (expr->raw_value()->IsNumber()) {
        LocalType type = TypeOf(expr);
        switch (type) {
          case kAstI32: {
            int val = static_cast<int>(expr->raw_value()->AsNumber());
            byte code[] = {WASM_I32(val)};
            current_function_builder_->EmitCode(code, sizeof(code));
            break;
          }
          case kAstF32: {
            float val = static_cast<float>(expr->raw_value()->AsNumber());
            byte code[] = {WASM_F32(val)};
            current_function_builder_->EmitCode(code, sizeof(code));
            break;
          }
          case kAstF64: {
            double val = static_cast<double>(expr->raw_value()->AsNumber());
            byte code[] = {WASM_F64(val)};
            current_function_builder_->EmitCode(code, sizeof(code));
            break;
          }
          default:
            UNREACHABLE();
        }
      }
    }
  }

  void VisitRegExpLiteral(RegExpLiteral* expr) { UNREACHABLE(); }

  void VisitObjectLiteral(ObjectLiteral* expr) {
    ZoneList<ObjectLiteralProperty*>* props = expr->properties();
    for (int i = 0; i < props->length(); ++i) {
      ObjectLiteralProperty* prop = props->at(i);
      DCHECK(marking_exported);
      VariableProxy* expr = prop->value()->AsVariableProxy();
      DCHECK(expr != nullptr);
      Variable* var = expr->var();
      Literal* name = prop->key()->AsLiteral();
      DCHECK(name != nullptr);
      DCHECK(name->IsPropertyName());
      const AstRawString* raw_name = name->AsRawPropertyName();
      if (var->is_function()) {
        uint16_t index = LookupOrInsertFunction(var);
        builder_->FunctionAt(index)->Exported(1);
        builder_->FunctionAt(index)
            ->SetName(raw_name->raw_data(), raw_name->length());
      }
    }
  }

  void VisitArrayLiteral(ArrayLiteral* expr) { UNREACHABLE(); }

  void LoadInitFunction() {
    current_function_builder_ = builder_->FunctionAt(init_function_index);
    in_function_ = true;
  }

  void UnLoadInitFunction() {
    in_function_ = false;
    current_function_builder_ = nullptr;
  }

  void VisitAssignment(Assignment* expr) {
    bool in_init = false;
    if (!in_function_) {
      // TODO(bradnelson): Get rid of this.
      if (TypeOf(expr->value()) == kAstStmt) {
        return;
      }
      in_init = true;
      LoadInitFunction();
    }
    BinaryOperation* value_op = expr->value()->AsBinaryOperation();
    if (value_op != nullptr && MatchBinaryOperation(value_op) == kAsIs) {
      VariableProxy* target_var = expr->target()->AsVariableProxy();
      VariableProxy* effective_value_var = GetLeft(value_op)->AsVariableProxy();
      if (target_var != nullptr && effective_value_var != nullptr &&
          target_var->var() == effective_value_var->var()) {
        block_size_--;
        return;
      }
    }
    is_set_op_ = true;
    RECURSE(Visit(expr->target()));
    DCHECK(!is_set_op_);
    RECURSE(Visit(expr->value()));
    if (in_init) {
      UnLoadInitFunction();
    }
  }

  void VisitYield(Yield* expr) { UNREACHABLE(); }

  void VisitThrow(Throw* expr) { UNREACHABLE(); }

  void VisitProperty(Property* expr) {
    Expression* obj = expr->obj();
    DCHECK(obj->bounds().lower == obj->bounds().upper);
    TypeImpl<ZoneTypeConfig>* type = obj->bounds().lower;
    MachineType mtype;
    int size;
    if (type->Is(cache_.kUint8Array)) {
      mtype = MachineType::Uint8();
      size = 1;
    } else if (type->Is(cache_.kInt8Array)) {
      mtype = MachineType::Int8();
      size = 1;
    } else if (type->Is(cache_.kUint16Array)) {
      mtype = MachineType::Uint16();
      size = 2;
    } else if (type->Is(cache_.kInt16Array)) {
      mtype = MachineType::Int16();
      size = 2;
    } else if (type->Is(cache_.kUint32Array)) {
      mtype = MachineType::Uint32();
      size = 4;
    } else if (type->Is(cache_.kInt32Array)) {
      mtype = MachineType::Int32();
      size = 4;
    } else if (type->Is(cache_.kUint32Array)) {
      mtype = MachineType::Uint32();
      size = 4;
    } else if (type->Is(cache_.kFloat32Array)) {
      mtype = MachineType::Float32();
      size = 4;
    } else if (type->Is(cache_.kFloat64Array)) {
      mtype = MachineType::Float64();
      size = 8;
    } else {
      UNREACHABLE();
    }
    current_function_builder_->EmitWithU8(
        WasmOpcodes::LoadStoreOpcodeOf(mtype, is_set_op_),
        WasmOpcodes::LoadStoreAccessOf(false));
    is_set_op_ = false;
    Literal* value = expr->key()->AsLiteral();
    if (value) {
      DCHECK(value->raw_value()->IsNumber());
      DCHECK(kAstI32 == TypeOf(value));
      int val = static_cast<int>(value->raw_value()->AsNumber());
      byte code[] = {WASM_I32(val * size)};
      current_function_builder_->EmitCode(code, sizeof(code));
      return;
    }
    BinaryOperation* binop = expr->key()->AsBinaryOperation();
    if (binop) {
      DCHECK(Token::SAR == binop->op());
      DCHECK(binop->right()->AsLiteral()->raw_value()->IsNumber());
      DCHECK(kAstI32 == TypeOf(binop->right()->AsLiteral()));
      DCHECK(size ==
             1 << static_cast<int>(
                 binop->right()->AsLiteral()->raw_value()->AsNumber()));
      // Mask bottom bits to match asm.js behavior.
      current_function_builder_->Emit(kExprI32And);
      byte code[] = {WASM_I8(~(size - 1))};
      current_function_builder_->EmitCode(code, sizeof(code));
      RECURSE(Visit(binop->left()));
      return;
    }
    UNREACHABLE();
  }

  void VisitCall(Call* expr) {
    Call::CallType call_type = expr->GetCallType(isolate_);
    switch (call_type) {
      case Call::OTHER_CALL: {
        DCHECK(in_function_);
        current_function_builder_->Emit(kExprCallFunction);
        RECURSE(Visit(expr->expression()));
        ZoneList<Expression*>* args = expr->arguments();
        for (int i = 0; i < args->length(); ++i) {
          Expression* arg = args->at(i);
          RECURSE(Visit(arg));
        }
        break;
      }
      default:
        UNREACHABLE();
    }
  }

  void VisitCallNew(CallNew* expr) { UNREACHABLE(); }

  void VisitCallRuntime(CallRuntime* expr) { UNREACHABLE(); }

  void VisitUnaryOperation(UnaryOperation* expr) {
    switch (expr->op()) {
      case Token::NOT: {
        DCHECK(TypeOf(expr->expression()) == kAstI32);
        current_function_builder_->Emit(kExprBoolNot);
        break;
      }
      default:
        UNREACHABLE();
    }
    RECURSE(Visit(expr->expression()));
  }

  void VisitCountOperation(CountOperation* expr) { UNREACHABLE(); }

  bool MatchIntBinaryOperation(BinaryOperation* expr, Token::Value op,
                               int32_t val) {
    DCHECK(expr->right() != nullptr);
    if (expr->op() == op && expr->right()->IsLiteral() &&
        TypeOf(expr) == kAstI32) {
      Literal* right = expr->right()->AsLiteral();
      DCHECK(right->raw_value()->IsNumber());
      if (static_cast<int32_t>(right->raw_value()->AsNumber()) == val) {
        return true;
      }
    }
    return false;
  }

  bool MatchDoubleBinaryOperation(BinaryOperation* expr, Token::Value op,
                                  double val) {
    DCHECK(expr->right() != nullptr);
    if (expr->op() == op && expr->right()->IsLiteral() &&
        TypeOf(expr) == kAstF64) {
      Literal* right = expr->right()->AsLiteral();
      DCHECK(right->raw_value()->IsNumber());
      if (right->raw_value()->AsNumber() == val) {
        return true;
      }
    }
    return false;
  }

  enum ConvertOperation { kNone, kAsIs, kToInt, kToDouble };

  ConvertOperation MatchOr(BinaryOperation* expr) {
    if (MatchIntBinaryOperation(expr, Token::BIT_OR, 0)) {
      return (TypeOf(expr->left()) == kAstI32) ? kAsIs : kToInt;
    } else {
      return kNone;
    }
  }

  ConvertOperation MatchShr(BinaryOperation* expr) {
    if (MatchIntBinaryOperation(expr, Token::SHR, 0)) {
      // TODO(titzer): this probably needs to be kToUint
      return (TypeOf(expr->left()) == kAstI32) ? kAsIs : kToInt;
    } else {
      return kNone;
    }
  }

  ConvertOperation MatchXor(BinaryOperation* expr) {
    if (MatchIntBinaryOperation(expr, Token::BIT_XOR, 0xffffffff)) {
      DCHECK(TypeOf(expr->left()) == kAstI32);
      DCHECK(TypeOf(expr->right()) == kAstI32);
      BinaryOperation* op = expr->left()->AsBinaryOperation();
      if (op != nullptr) {
        if (MatchIntBinaryOperation(op, Token::BIT_XOR, 0xffffffff)) {
          DCHECK(TypeOf(op->right()) == kAstI32);
          if (TypeOf(op->left()) != kAstI32) {
            return kToInt;
          } else {
            return kAsIs;
          }
        }
      }
    }
    return kNone;
  }

  ConvertOperation MatchMul(BinaryOperation* expr) {
    if (MatchDoubleBinaryOperation(expr, Token::MUL, 1.0)) {
      DCHECK(TypeOf(expr->right()) == kAstF64);
      if (TypeOf(expr->left()) != kAstF64) {
        return kToDouble;
      } else {
        return kAsIs;
      }
    } else {
      return kNone;
    }
  }

  ConvertOperation MatchBinaryOperation(BinaryOperation* expr) {
    switch (expr->op()) {
      case Token::BIT_OR:
        return MatchOr(expr);
      case Token::SHR:
        return MatchShr(expr);
      case Token::BIT_XOR:
        return MatchXor(expr);
      case Token::MUL:
        return MatchMul(expr);
      default:
        return kNone;
    }
  }

// Work around Mul + Div being defined in PPC assembler.
#ifdef Mul
#undef Mul
#endif
#ifdef Div
#undef Div
#endif

#define NON_SIGNED_BINOP(op)      \
  static WasmOpcode opcodes[] = { \
    kExprI32##op,                 \
    kExprI32##op,                 \
    kExprF32##op,                 \
    kExprF64##op                  \
  }

#define SIGNED_BINOP(op)          \
  static WasmOpcode opcodes[] = { \
    kExprI32##op##S,              \
    kExprI32##op##U,              \
    kExprF32##op,                 \
    kExprF64##op                  \
  }

#define NON_SIGNED_INT_BINOP(op) \
  static WasmOpcode opcodes[] = { kExprI32##op, kExprI32##op }

#define BINOP_CASE(token, op, V, ignore_sign)                         \
  case token: {                                                       \
    V(op);                                                            \
    int type = TypeIndexOf(expr->left(), expr->right(), ignore_sign); \
    current_function_builder_->Emit(opcodes[type]);                   \
    break;                                                            \
  }

  Expression* GetLeft(BinaryOperation* expr) {
    if (expr->op() == Token::BIT_XOR) {
      return expr->left()->AsBinaryOperation()->left();
    } else {
      return expr->left();
    }
  }

  void VisitBinaryOperation(BinaryOperation* expr) {
    ConvertOperation convertOperation = MatchBinaryOperation(expr);
    if (convertOperation == kToDouble) {
      TypeIndex type = TypeIndexOf(expr->left());
      if (type == kInt32 || type == kFixnum) {
        current_function_builder_->Emit(kExprF64SConvertI32);
      } else if (type == kUint32) {
        current_function_builder_->Emit(kExprF64UConvertI32);
      } else if (type == kFloat32) {
        current_function_builder_->Emit(kExprF64ConvertF32);
      } else {
        UNREACHABLE();
      }
      RECURSE(Visit(expr->left()));
    } else if (convertOperation == kToInt) {
      TypeIndex type = TypeIndexOf(GetLeft(expr));
      if (type == kFloat32) {
        current_function_builder_->Emit(kExprI32SConvertF32);
      } else if (type == kFloat64) {
        current_function_builder_->Emit(kExprI32SConvertF64);
      } else {
        UNREACHABLE();
      }
      RECURSE(Visit(GetLeft(expr)));
    } else if (convertOperation == kAsIs) {
      RECURSE(Visit(GetLeft(expr)));
    } else {
      switch (expr->op()) {
        BINOP_CASE(Token::ADD, Add, NON_SIGNED_BINOP, true);
        BINOP_CASE(Token::SUB, Sub, NON_SIGNED_BINOP, true);
        BINOP_CASE(Token::MUL, Mul, NON_SIGNED_BINOP, true);
        BINOP_CASE(Token::DIV, Div, SIGNED_BINOP, false);
        BINOP_CASE(Token::BIT_OR, Ior, NON_SIGNED_INT_BINOP, true);
        BINOP_CASE(Token::BIT_XOR, Xor, NON_SIGNED_INT_BINOP, true);
        BINOP_CASE(Token::SHL, Shl, NON_SIGNED_INT_BINOP, true);
        BINOP_CASE(Token::SAR, ShrS, NON_SIGNED_INT_BINOP, true);
        BINOP_CASE(Token::SHR, ShrU, NON_SIGNED_INT_BINOP, true);
        case Token::MOD: {
          TypeIndex type = TypeIndexOf(expr->left(), expr->right(), false);
          if (type == kInt32) {
            current_function_builder_->Emit(kExprI32RemS);
          } else if (type == kUint32) {
            current_function_builder_->Emit(kExprI32RemU);
          } else if (type == kFloat64) {
            ModF64(expr);
            return;
          } else {
            UNREACHABLE();
          }
          break;
        }
        default:
          UNREACHABLE();
      }
      RECURSE(Visit(expr->left()));
      RECURSE(Visit(expr->right()));
    }
  }

  void ModF64(BinaryOperation* expr) {
    current_function_builder_->EmitWithU8(kExprBlock, 3);
    uint16_t index_0 = current_function_builder_->AddLocal(kAstF64);
    uint16_t index_1 = current_function_builder_->AddLocal(kAstF64);
    current_function_builder_->Emit(kExprSetLocal);
    AddLeb128(index_0, true);
    RECURSE(Visit(expr->left()));
    current_function_builder_->Emit(kExprSetLocal);
    AddLeb128(index_1, true);
    RECURSE(Visit(expr->right()));
    current_function_builder_->Emit(kExprF64Sub);
    current_function_builder_->Emit(kExprGetLocal);
    AddLeb128(index_0, true);
    current_function_builder_->Emit(kExprF64Mul);
    current_function_builder_->Emit(kExprGetLocal);
    AddLeb128(index_1, true);
    // Use trunc instead of two casts
    current_function_builder_->Emit(kExprF64SConvertI32);
    current_function_builder_->Emit(kExprI32SConvertF64);
    current_function_builder_->Emit(kExprF64Div);
    current_function_builder_->Emit(kExprGetLocal);
    AddLeb128(index_0, true);
    current_function_builder_->Emit(kExprGetLocal);
    AddLeb128(index_1, true);
  }

  void AddLeb128(uint32_t index, bool is_local) {
    std::vector<uint8_t> index_vec = UnsignedLEB128From(index);
    if (is_local) {
      uint32_t pos_of_index[1] = {0};
      current_function_builder_->EmitCode(
          &index_vec[0], static_cast<uint32_t>(index_vec.size()), pos_of_index,
          1);
    } else {
      current_function_builder_->EmitCode(
          &index_vec[0], static_cast<uint32_t>(index_vec.size()));
    }
  }

  void VisitCompareOperation(CompareOperation* expr) {
    switch (expr->op()) {
      BINOP_CASE(Token::EQ, Eq, NON_SIGNED_BINOP, false);
      BINOP_CASE(Token::LT, Lt, SIGNED_BINOP, false);
      BINOP_CASE(Token::LTE, Le, SIGNED_BINOP, false);
      BINOP_CASE(Token::GT, Gt, SIGNED_BINOP, false);
      BINOP_CASE(Token::GTE, Ge, SIGNED_BINOP, false);
      default:
        UNREACHABLE();
    }
    RECURSE(Visit(expr->left()));
    RECURSE(Visit(expr->right()));
  }

#undef BINOP_CASE
#undef NON_SIGNED_INT_BINOP
#undef SIGNED_BINOP
#undef NON_SIGNED_BINOP

  enum TypeIndex {
    kInt32 = 0,
    kUint32 = 1,
    kFloat32 = 2,
    kFloat64 = 3,
    kFixnum = 4
  };

  TypeIndex TypeIndexOf(Expression* left, Expression* right, bool ignore_sign) {
    TypeIndex left_index = TypeIndexOf(left);
    TypeIndex right_index = TypeIndexOf(right);
    if (left_index == kFixnum) {
      left_index = right_index;
    }
    if (right_index == kFixnum) {
      right_index = left_index;
    }
    if (left_index == kFixnum && right_index == kFixnum) {
      left_index = kInt32;
      right_index = kInt32;
    }
    DCHECK((left_index == right_index) ||
           (ignore_sign && (left_index <= 1) && (right_index <= 1)));
    return left_index;
  }

  TypeIndex TypeIndexOf(Expression* expr) {
    DCHECK(expr->bounds().lower == expr->bounds().upper);
    TypeImpl<ZoneTypeConfig>* type = expr->bounds().lower;
    if (type->Is(cache_.kAsmFixnum)) {
      return kFixnum;
    } else if (type->Is(cache_.kAsmSigned)) {
      return kInt32;
    } else if (type->Is(cache_.kAsmUnsigned)) {
      return kUint32;
    } else if (type->Is(cache_.kAsmInt)) {
      return kInt32;
    } else if (type->Is(cache_.kAsmFloat)) {
      return kFloat32;
    } else if (type->Is(cache_.kAsmDouble)) {
      return kFloat64;
    } else {
      UNREACHABLE();
      return kInt32;
    }
  }

#undef CASE
#undef NON_SIGNED_INT
#undef SIGNED
#undef NON_SIGNED

  void VisitThisFunction(ThisFunction* expr) { UNREACHABLE(); }

  void VisitDeclarations(ZoneList<Declaration*>* decls) {
    for (int i = 0; i < decls->length(); ++i) {
      Declaration* decl = decls->at(i);
      RECURSE(Visit(decl));
    }
  }

  void VisitClassLiteral(ClassLiteral* expr) { UNREACHABLE(); }

  void VisitSpread(Spread* expr) { UNREACHABLE(); }

  void VisitSuperPropertyReference(SuperPropertyReference* expr) {
    UNREACHABLE();
  }

  void VisitSuperCallReference(SuperCallReference* expr) { UNREACHABLE(); }

  void VisitSloppyBlockFunctionStatement(SloppyBlockFunctionStatement* expr) {
    UNREACHABLE();
  }

  void VisitDoExpression(DoExpression* expr) { UNREACHABLE(); }

  void VisitRewritableAssignmentExpression(
      RewritableAssignmentExpression* expr) {
    UNREACHABLE();
  }

  struct IndexContainer : public ZoneObject {
    uint16_t index;
  };

  uint16_t LookupOrInsertLocal(Variable* v, LocalType type) {
    DCHECK(current_function_builder_ != nullptr);
    ZoneHashMap::Entry* entry =
        local_variables_.Lookup(v, ComputePointerHash(v));
    if (entry == nullptr) {
      uint16_t index;
      if (v->IsParameter()) {
        index = current_function_builder_->AddParam(type);
      } else {
        index = current_function_builder_->AddLocal(type);
      }
      IndexContainer* container = new (zone()) IndexContainer();
      container->index = index;
      entry = local_variables_.LookupOrInsert(v, ComputePointerHash(v),
                                              ZoneAllocationPolicy(zone()));
      entry->value = container;
    }
    return (reinterpret_cast<IndexContainer*>(entry->value))->index;
  }

  uint16_t LookupOrInsertGlobal(Variable* v, LocalType type) {
    ZoneHashMap::Entry* entry =
        global_variables_.Lookup(v, ComputePointerHash(v));
    if (entry == nullptr) {
      uint16_t index =
          builder_->AddGlobal(WasmOpcodes::MachineTypeFor(type), 0);
      IndexContainer* container = new (zone()) IndexContainer();
      container->index = index;
      entry = global_variables_.LookupOrInsert(v, ComputePointerHash(v),
                                               ZoneAllocationPolicy(zone()));
      entry->value = container;
    }
    return (reinterpret_cast<IndexContainer*>(entry->value))->index;
  }

  uint16_t LookupOrInsertFunction(Variable* v) {
    DCHECK(builder_ != nullptr);
    ZoneHashMap::Entry* entry = functions_.Lookup(v, ComputePointerHash(v));
    if (entry == nullptr) {
      uint16_t index = builder_->AddFunction();
      IndexContainer* container = new (zone()) IndexContainer();
      container->index = index;
      entry = functions_.LookupOrInsert(v, ComputePointerHash(v),
                                        ZoneAllocationPolicy(zone()));
      entry->value = container;
    }
    return (reinterpret_cast<IndexContainer*>(entry->value))->index;
  }

  LocalType TypeOf(Expression* expr) {
    DCHECK(expr->bounds().lower == expr->bounds().upper);
    return TypeFrom(expr->bounds().lower);
  }

  LocalType TypeFrom(TypeImpl<ZoneTypeConfig>* type) {
    if (type->Is(cache_.kAsmInt)) {
      return kAstI32;
    } else if (type->Is(cache_.kAsmFloat)) {
      return kAstF32;
    } else if (type->Is(cache_.kAsmDouble)) {
      return kAstF64;
    } else {
      return kAstStmt;
    }
  }

  Zone* zone() { return zone_; }

  ZoneHashMap local_variables_;
  ZoneHashMap functions_;
  ZoneHashMap global_variables_;
  bool in_function_;
  bool is_set_op_;
  bool marking_exported;
  WasmModuleBuilder* builder_;
  WasmFunctionBuilder* current_function_builder_;
  FunctionLiteral* literal_;
  Isolate* isolate_;
  Zone* zone_;
  TypeCache const& cache_;
  ZoneVector<std::pair<BreakableStatement*, bool>> breakable_blocks_;
  int block_size_;
  uint16_t init_function_index;

  DEFINE_AST_VISITOR_SUBCLASS_MEMBERS();

 private:
  DISALLOW_COPY_AND_ASSIGN(AsmWasmBuilderImpl);
};

AsmWasmBuilder::AsmWasmBuilder(Isolate* isolate, Zone* zone,
                               FunctionLiteral* literal)
    : isolate_(isolate), zone_(zone), literal_(literal) {}

// TODO(aseemgarg): probably should take zone (to write wasm to) as input so
// that zone in constructor may be thrown away once wasm module is written.
WasmModuleIndex* AsmWasmBuilder::Run() {
  AsmWasmBuilderImpl impl(isolate_, zone_, literal_);
  impl.Compile();
  WasmModuleWriter* writer = impl.builder_->Build(zone_);
  return writer->WriteTo(zone_);
}
}  // namespace wasm
}  // namespace internal
}  // namespace v8
