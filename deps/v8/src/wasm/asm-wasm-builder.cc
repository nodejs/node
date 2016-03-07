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
  AsmWasmBuilderImpl(Isolate* isolate, Zone* zone, FunctionLiteral* literal,
                     Handle<Object> foreign)
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
        foreign_(foreign),
        cache_(TypeCache::Get()),
        breakable_blocks_(zone),
        block_size_(0),
        init_function_index_(0),
        next_table_index_(0),
        function_tables_(HashMap::PointersMatch,
                         ZoneHashMap::kDefaultHashMapCapacity,
                         ZoneAllocationPolicy(zone)),
        imported_function_table_(this) {
    InitializeAstVisitor(isolate);
  }

  void InitializeInitFunction() {
    unsigned char init[] = "__init__";
    init_function_index_ = builder_->AddFunction();
    current_function_builder_ = builder_->FunctionAt(init_function_index_);
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
    DCHECK_NULL(current_function_builder_);
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
    if (in_function_) {
      BlockVisitor visitor(this, stmt->AsBreakableStatement(), kExprBlock,
                           false,
                           static_cast<byte>(stmt->statements()->length()));
      RECURSE(VisitStatements(stmt->statements()));
      DCHECK(block_size_ >= 0);
    } else {
      RECURSE(VisitStatements(stmt->statements()));
    }
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
    DCHECK_NOT_NULL(stmt->target());
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
    DCHECK_NOT_NULL(stmt->target());
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
    DCHECK_NOT_NULL(label);
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
    DCHECK_NOT_NULL(tag);
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
        FunctionType* func_type = expr->bounds().lower->AsFunction();
        LocalType return_type = TypeFrom(func_type->Result());
        current_function_builder_->ReturnType(return_type);
        for (int i = 0; i < expr->parameter_count(); i++) {
          LocalType type = TypeFrom(func_type->Parameter(i));
          DCHECK_NE(kAstStmt, type);
          LookupOrInsertLocal(scope->parameter(i), type);
        }
      } else {
        UNREACHABLE();
      }
    }
    RECURSE(VisitStatements(expr->body()));
    RECURSE(VisitDeclarations(scope->declarations()));
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
      DCHECK_NE(kAstStmt, var_type);
      if (var->IsContextSlot()) {
        AddLeb128(LookupOrInsertGlobal(var, var_type), false);
      } else {
        AddLeb128(LookupOrInsertLocal(var, var_type), true);
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
      DCHECK_NOT_NULL(expr);
      Variable* var = expr->var();
      Literal* name = prop->key()->AsLiteral();
      DCHECK_NOT_NULL(name);
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
    current_function_builder_ = builder_->FunctionAt(init_function_index_);
    in_function_ = true;
  }

  void UnLoadInitFunction() {
    in_function_ = false;
    current_function_builder_ = nullptr;
  }

  void AddFunctionTable(VariableProxy* table, ArrayLiteral* funcs) {
    FunctionType* func_type =
        funcs->bounds().lower->AsArray()->Element()->AsFunction();
    LocalType return_type = TypeFrom(func_type->Result());
    FunctionSig::Builder sig(zone(), return_type == kAstStmt ? 0 : 1,
                             func_type->Arity());
    if (return_type != kAstStmt) {
      sig.AddReturn(static_cast<LocalType>(return_type));
    }
    for (int i = 0; i < func_type->Arity(); i++) {
      sig.AddParam(TypeFrom(func_type->Parameter(i)));
    }
    uint16_t signature_index = builder_->AddSignature(sig.Build());
    InsertFunctionTable(table->var(), next_table_index_, signature_index);
    next_table_index_ += funcs->values()->length();
    for (int i = 0; i < funcs->values()->length(); i++) {
      VariableProxy* func = funcs->values()->at(i)->AsVariableProxy();
      DCHECK_NOT_NULL(func);
      builder_->AddIndirectFunction(LookupOrInsertFunction(func->var()));
    }
  }

  struct FunctionTableIndices : public ZoneObject {
    uint32_t start_index;
    uint16_t signature_index;
  };

  void InsertFunctionTable(Variable* v, uint32_t start_index,
                           uint16_t signature_index) {
    FunctionTableIndices* container = new (zone()) FunctionTableIndices();
    container->start_index = start_index;
    container->signature_index = signature_index;
    ZoneHashMap::Entry* entry = function_tables_.LookupOrInsert(
        v, ComputePointerHash(v), ZoneAllocationPolicy(zone()));
    entry->value = container;
  }

  FunctionTableIndices* LookupFunctionTable(Variable* v) {
    ZoneHashMap::Entry* entry =
        function_tables_.Lookup(v, ComputePointerHash(v));
    DCHECK_NOT_NULL(entry);
    return reinterpret_cast<FunctionTableIndices*>(entry->value);
  }

  class ImportedFunctionTable {
   private:
    class ImportedFunctionIndices : public ZoneObject {
     public:
      const unsigned char* name_;
      int name_length_;
      WasmModuleBuilder::SignatureMap signature_to_index_;

      ImportedFunctionIndices(const unsigned char* name, int name_length,
                              Zone* zone)
          : name_(name), name_length_(name_length), signature_to_index_(zone) {}
    };
    ZoneHashMap table_;
    AsmWasmBuilderImpl* builder_;

   public:
    explicit ImportedFunctionTable(AsmWasmBuilderImpl* builder)
        : table_(HashMap::PointersMatch, ZoneHashMap::kDefaultHashMapCapacity,
                 ZoneAllocationPolicy(builder->zone())),
          builder_(builder) {}

    void AddImport(Variable* v, const unsigned char* name, int name_length) {
      ImportedFunctionIndices* indices = new (builder_->zone())
          ImportedFunctionIndices(name, name_length, builder_->zone());
      ZoneHashMap::Entry* entry = table_.LookupOrInsert(
          v, ComputePointerHash(v), ZoneAllocationPolicy(builder_->zone()));
      entry->value = indices;
    }

    uint16_t GetFunctionIndex(Variable* v, FunctionSig* sig) {
      ZoneHashMap::Entry* entry = table_.Lookup(v, ComputePointerHash(v));
      DCHECK_NOT_NULL(entry);
      ImportedFunctionIndices* indices =
          reinterpret_cast<ImportedFunctionIndices*>(entry->value);
      WasmModuleBuilder::SignatureMap::iterator pos =
          indices->signature_to_index_.find(sig);
      if (pos != indices->signature_to_index_.end()) {
        return pos->second;
      } else {
        uint16_t index = builder_->builder_->AddFunction();
        indices->signature_to_index_[sig] = index;
        WasmFunctionBuilder* function = builder_->builder_->FunctionAt(index);
        function->External(1);
        function->SetName(indices->name_, indices->name_length_);
        if (sig->return_count() > 0) {
          function->ReturnType(sig->GetReturn());
        }
        for (size_t i = 0; i < sig->parameter_count(); i++) {
          function->AddParam(sig->GetParam(i));
        }
        return index;
      }
    }
  };

  void VisitAssignment(Assignment* expr) {
    bool in_init = false;
    if (!in_function_) {
      BinaryOperation* binop = expr->value()->AsBinaryOperation();
      if (binop != nullptr) {
        Property* prop = binop->left()->AsProperty();
        DCHECK_NOT_NULL(prop);
        LoadInitFunction();
        is_set_op_ = true;
        RECURSE(Visit(expr->target()));
        DCHECK(!is_set_op_);
        if (binop->op() == Token::MUL) {
          DCHECK(binop->right()->IsLiteral());
          DCHECK_EQ(1.0, binop->right()->AsLiteral()->raw_value()->AsNumber());
          DCHECK(binop->right()->AsLiteral()->raw_value()->ContainsDot());
          VisitForeignVariable(true, prop);
        } else if (binop->op() == Token::BIT_OR) {
          DCHECK(binop->right()->IsLiteral());
          DCHECK_EQ(0.0, binop->right()->AsLiteral()->raw_value()->AsNumber());
          DCHECK(!binop->right()->AsLiteral()->raw_value()->ContainsDot());
          VisitForeignVariable(false, prop);
        } else {
          UNREACHABLE();
        }
        UnLoadInitFunction();
        return;
      }
      // TODO(bradnelson): Get rid of this.
      if (TypeOf(expr->value()) == kAstStmt) {
        Property* prop = expr->value()->AsProperty();
        if (prop != nullptr) {
          VariableProxy* vp = prop->obj()->AsVariableProxy();
          if (vp != nullptr && vp->var()->IsParameter() &&
              vp->var()->index() == 1) {
            VariableProxy* target = expr->target()->AsVariableProxy();
            if (target->bounds().lower->Is(Type::Function())) {
              const AstRawString* name =
                  prop->key()->AsLiteral()->AsRawPropertyName();
              imported_function_table_.AddImport(
                  target->var(), name->raw_data(), name->length());
            }
          }
        }
        ArrayLiteral* funcs = expr->value()->AsArrayLiteral();
        if (funcs != nullptr &&
            funcs->bounds().lower->AsArray()->Element()->IsFunction()) {
          VariableProxy* target = expr->target()->AsVariableProxy();
          DCHECK_NOT_NULL(target);
          AddFunctionTable(target, funcs);
        }
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

  void VisitForeignVariable(bool is_float, Property* expr) {
    DCHECK(expr->obj()->AsVariableProxy());
    DCHECK(VariableLocation::PARAMETER ==
           expr->obj()->AsVariableProxy()->var()->location());
    DCHECK_EQ(1, expr->obj()->AsVariableProxy()->var()->index());
    Literal* key_literal = expr->key()->AsLiteral();
    DCHECK_NOT_NULL(key_literal);
    if (!key_literal->value().is_null() && !foreign_.is_null() &&
        foreign_->IsObject()) {
      Handle<Name> name =
          i::Object::ToName(isolate_, key_literal->value()).ToHandleChecked();
      MaybeHandle<Object> maybe_value = i::Object::GetProperty(foreign_, name);
      if (!maybe_value.is_null()) {
        Handle<Object> value = maybe_value.ToHandleChecked();
        if (is_float) {
          MaybeHandle<Object> maybe_nvalue = i::Object::ToNumber(value);
          if (!maybe_nvalue.is_null()) {
            Handle<Object> nvalue = maybe_nvalue.ToHandleChecked();
            if (nvalue->IsNumber()) {
              double val = nvalue->Number();
              byte code[] = {WASM_F64(val)};
              current_function_builder_->EmitCode(code, sizeof(code));
              return;
            }
          }
        } else {
          MaybeHandle<Object> maybe_nvalue =
              i::Object::ToInt32(isolate_, value);
          if (!maybe_nvalue.is_null()) {
            Handle<Object> nvalue = maybe_nvalue.ToHandleChecked();
            if (nvalue->IsNumber()) {
              int32_t val = static_cast<int32_t>(nvalue->Number());
              byte code[] = {WASM_I32(val)};
              current_function_builder_->EmitCode(code, sizeof(code));
              return;
            }
          }
        }
      }
    }
    if (is_float) {
      byte code[] = {WASM_F64(std::numeric_limits<double>::quiet_NaN())};
      current_function_builder_->EmitCode(code, sizeof(code));
    } else {
      byte code[] = {WASM_I32(0)};
      current_function_builder_->EmitCode(code, sizeof(code));
    }
  }

  void VisitProperty(Property* expr) {
    Expression* obj = expr->obj();
    DCHECK_EQ(obj->bounds().lower, obj->bounds().upper);
    Type* type = obj->bounds().lower;
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
    if (size == 1) {
      // Allow more general expression in byte arrays than the spec
      // strictly permits.
      // Early versions of Emscripten emit HEAP8[HEAP32[..]|0] in
      // places that strictly should be HEAP8[HEAP32[..]>>0].
      RECURSE(Visit(expr->key()));
      return;
    } else {
      Literal* value = expr->key()->AsLiteral();
      if (value) {
        DCHECK(value->raw_value()->IsNumber());
        DCHECK_EQ(kAstI32, TypeOf(value));
        int val = static_cast<int>(value->raw_value()->AsNumber());
        byte code[] = {WASM_I32(val * size)};
        current_function_builder_->EmitCode(code, sizeof(code));
        return;
      }
      BinaryOperation* binop = expr->key()->AsBinaryOperation();
      if (binop) {
        DCHECK_EQ(Token::SAR, binop->op());
        DCHECK(binop->right()->AsLiteral()->raw_value()->IsNumber());
        DCHECK(kAstI32 == TypeOf(binop->right()->AsLiteral()));
        DCHECK_EQ(size,
                  1 << static_cast<int>(
                      binop->right()->AsLiteral()->raw_value()->AsNumber()));
        // Mask bottom bits to match asm.js behavior.
        current_function_builder_->Emit(kExprI32And);
        byte code[] = {WASM_I8(~(size - 1))};
        current_function_builder_->EmitCode(code, sizeof(code));
        RECURSE(Visit(binop->left()));
        return;
      }
    }
    UNREACHABLE();
  }

  void VisitCall(Call* expr) {
    Call::CallType call_type = expr->GetCallType(isolate_);
    switch (call_type) {
      case Call::OTHER_CALL: {
        DCHECK(in_function_);
        uint16_t index;
        VariableProxy* vp = expr->expression()->AsVariableProxy();
        if (vp != nullptr &&
            Type::Any()->Is(vp->bounds().lower->AsFunction()->Result())) {
          LocalType return_type = TypeOf(expr);
          ZoneList<Expression*>* args = expr->arguments();
          FunctionSig::Builder sig(zone(), return_type == kAstStmt ? 0 : 1,
                                   args->length());
          if (return_type != kAstStmt) {
            sig.AddReturn(return_type);
          }
          for (int i = 0; i < args->length(); i++) {
            sig.AddParam(TypeOf(args->at(i)));
          }
          index =
              imported_function_table_.GetFunctionIndex(vp->var(), sig.Build());
        } else {
          index = LookupOrInsertFunction(vp->var());
        }
        current_function_builder_->Emit(kExprCallFunction);
        std::vector<uint8_t> index_arr = UnsignedLEB128From(index);
        current_function_builder_->EmitCode(
            &index_arr[0], static_cast<uint32_t>(index_arr.size()));
        break;
      }
      case Call::KEYED_PROPERTY_CALL: {
        DCHECK(in_function_);
        Property* p = expr->expression()->AsProperty();
        DCHECK_NOT_NULL(p);
        VariableProxy* var = p->obj()->AsVariableProxy();
        DCHECK_NOT_NULL(var);
        FunctionTableIndices* indices = LookupFunctionTable(var->var());
        current_function_builder_->EmitWithU8(kExprCallIndirect,
                                              indices->signature_index);
        current_function_builder_->Emit(kExprI32Add);
        byte code[] = {WASM_I32(indices->start_index)};
        current_function_builder_->EmitCode(code, sizeof(code));
        RECURSE(Visit(p->key()));
        break;
      }
      default:
        UNREACHABLE();
    }
    ZoneList<Expression*>* args = expr->arguments();
    for (int i = 0; i < args->length(); ++i) {
      Expression* arg = args->at(i);
      RECURSE(Visit(arg));
    }
  }

  void VisitCallNew(CallNew* expr) { UNREACHABLE(); }

  void VisitCallRuntime(CallRuntime* expr) { UNREACHABLE(); }

  void VisitUnaryOperation(UnaryOperation* expr) {
    switch (expr->op()) {
      case Token::NOT: {
        DCHECK_EQ(kAstI32, TypeOf(expr->expression()));
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
    DCHECK_NOT_NULL(expr->right());
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
    DCHECK_NOT_NULL(expr->right());
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
    if (MatchIntBinaryOperation(expr, Token::BIT_OR, 0) &&
        (TypeOf(expr->left()) == kAstI32)) {
      return kAsIs;
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
      DCHECK_EQ(kAstI32, TypeOf(expr->left()));
      DCHECK_EQ(kAstI32, TypeOf(expr->right()));
      BinaryOperation* op = expr->left()->AsBinaryOperation();
      if (op != nullptr) {
        if (MatchIntBinaryOperation(op, Token::BIT_XOR, 0xffffffff)) {
          DCHECK_EQ(kAstI32, TypeOf(op->right()));
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
      DCHECK_EQ(kAstF64, TypeOf(expr->right()));
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
        BINOP_CASE(Token::BIT_AND, And, NON_SIGNED_INT_BINOP, true);
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
        case Token::COMMA: {
          current_function_builder_->EmitWithU8(kExprBlock, 2);
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
    DCHECK_EQ(expr->bounds().lower, expr->bounds().upper);
    Type* type = expr->bounds().lower;
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

  void VisitRewritableExpression(RewritableExpression* expr) { UNREACHABLE(); }

  struct IndexContainer : public ZoneObject {
    uint16_t index;
  };

  uint16_t LookupOrInsertLocal(Variable* v, LocalType type) {
    DCHECK_NOT_NULL(current_function_builder_);
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
    DCHECK_NOT_NULL(builder_);
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
    DCHECK_EQ(expr->bounds().lower, expr->bounds().upper);
    return TypeFrom(expr->bounds().lower);
  }

  LocalType TypeFrom(Type* type) {
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
  Handle<Object> foreign_;
  TypeCache const& cache_;
  ZoneVector<std::pair<BreakableStatement*, bool>> breakable_blocks_;
  int block_size_;
  uint16_t init_function_index_;
  uint32_t next_table_index_;
  ZoneHashMap function_tables_;
  ImportedFunctionTable imported_function_table_;

  DEFINE_AST_VISITOR_SUBCLASS_MEMBERS();

 private:
  DISALLOW_COPY_AND_ASSIGN(AsmWasmBuilderImpl);
};

AsmWasmBuilder::AsmWasmBuilder(Isolate* isolate, Zone* zone,
                               FunctionLiteral* literal, Handle<Object> foreign)
    : isolate_(isolate), zone_(zone), literal_(literal), foreign_(foreign) {}

// TODO(aseemgarg): probably should take zone (to write wasm to) as input so
// that zone in constructor may be thrown away once wasm module is written.
WasmModuleIndex* AsmWasmBuilder::Run() {
  AsmWasmBuilderImpl impl(isolate_, zone_, literal_, foreign_);
  impl.Compile();
  WasmModuleWriter* writer = impl.builder_->Build(zone_);
  return writer->WriteTo(zone_);
}
}  // namespace wasm
}  // namespace internal
}  // namespace v8
