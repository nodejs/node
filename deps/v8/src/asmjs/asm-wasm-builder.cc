// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

// Required to get M_E etc. in MSVC.
#if defined(_WIN32)
#define _USE_MATH_DEFINES
#endif
#include <math.h>

#include "src/asmjs/asm-types.h"
#include "src/asmjs/asm-wasm-builder.h"
#include "src/asmjs/switch-logic.h"

#include "src/wasm/wasm-macro-gen.h"
#include "src/wasm/wasm-opcodes.h"

#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/codegen.h"
#include "src/compilation-info.h"
#include "src/compiler.h"
#include "src/isolate.h"
#include "src/parsing/parse-info.h"

namespace v8 {
namespace internal {
namespace wasm {

#define RECURSE(call)               \
  do {                              \
    DCHECK(!HasStackOverflow());    \
    call;                           \
    if (HasStackOverflow()) return; \
  } while (false)

enum AsmScope { kModuleScope, kInitScope, kFuncScope, kExportScope };
enum ValueFate { kDrop, kLeaveOnStack };

struct ForeignVariable {
  Handle<Name> name;
  Variable* var;
  ValueType type;
};

class AsmWasmBuilderImpl final : public AstVisitor<AsmWasmBuilderImpl> {
 public:
  AsmWasmBuilderImpl(Isolate* isolate, Zone* zone, CompilationInfo* info,
                     AstValueFactory* ast_value_factory, Handle<Script> script,
                     FunctionLiteral* literal, AsmTyper* typer)
      : local_variables_(ZoneHashMap::kDefaultHashMapCapacity,
                         ZoneAllocationPolicy(zone)),
        functions_(ZoneHashMap::kDefaultHashMapCapacity,
                   ZoneAllocationPolicy(zone)),
        global_variables_(ZoneHashMap::kDefaultHashMapCapacity,
                          ZoneAllocationPolicy(zone)),
        scope_(kModuleScope),
        builder_(new (zone) WasmModuleBuilder(zone)),
        current_function_builder_(nullptr),
        literal_(literal),
        isolate_(isolate),
        zone_(zone),
        info_(info),
        ast_value_factory_(ast_value_factory),
        script_(script),
        typer_(typer),
        typer_failed_(false),
        typer_finished_(false),
        breakable_blocks_(zone),
        foreign_variables_(zone),
        init_function_(nullptr),
        foreign_init_function_(nullptr),
        function_tables_(ZoneHashMap::kDefaultHashMapCapacity,
                         ZoneAllocationPolicy(zone)),
        imported_function_table_(this),
        parent_binop_(nullptr) {
    InitializeAstVisitor(isolate);
  }

  void InitializeInitFunction() {
    FunctionSig::Builder b(zone(), 0, 0);
    init_function_ = builder_->AddFunction(b.Build());
    builder_->MarkStartFunction(init_function_);
  }

  void BuildForeignInitFunction() {
    foreign_init_function_ = builder_->AddFunction();
    FunctionSig::Builder b(zone(), 0, foreign_variables_.size());
    for (auto i = foreign_variables_.begin(); i != foreign_variables_.end();
         ++i) {
      b.AddParam(i->type);
    }
    foreign_init_function_->ExportAs(
        CStrVector(AsmWasmBuilder::foreign_init_name));
    foreign_init_function_->SetSignature(b.Build());
    for (size_t pos = 0; pos < foreign_variables_.size(); ++pos) {
      foreign_init_function_->EmitGetLocal(static_cast<uint32_t>(pos));
      ForeignVariable* fv = &foreign_variables_[pos];
      uint32_t index = LookupOrInsertGlobal(fv->var, fv->type);
      foreign_init_function_->EmitWithVarInt(kExprSetGlobal, index);
    }
    foreign_init_function_->Emit(kExprEnd);
  }

  Handle<FixedArray> GetForeignArgs() {
    Handle<FixedArray> ret = isolate_->factory()->NewFixedArray(
        static_cast<int>(foreign_variables_.size()));
    for (size_t i = 0; i < foreign_variables_.size(); ++i) {
      ForeignVariable* fv = &foreign_variables_[i];
      ret->set(static_cast<int>(i), *fv->name);
    }
    return ret;
  }

  bool Build() {
    InitializeInitFunction();
    if (!typer_->ValidateBeforeFunctionsPhase()) {
      return false;
    }
    DCHECK(!HasStackOverflow());
    VisitFunctionLiteral(literal_);
    if (HasStackOverflow()) {
      return false;
    }
    if (!typer_finished_ && !typer_failed_) {
      typer_->FailWithMessage("Module missing export section.");
      typer_failed_ = true;
    }
    if (typer_failed_) {
      return false;
    }
    BuildForeignInitFunction();
    init_function_->Emit(kExprEnd);  // finish init function.
    return true;
  }

  void VisitVariableDeclaration(VariableDeclaration* decl) {}

  void VisitFunctionDeclaration(FunctionDeclaration* decl) {
    DCHECK_EQ(kModuleScope, scope_);
    DCHECK_NULL(current_function_builder_);
    FunctionLiteral* old_func = decl->fun();
    Zone zone(isolate_->allocator(), ZONE_NAME);
    DeclarationScope* new_func_scope = nullptr;
    if (decl->fun()->body() == nullptr) {
      // TODO(titzer/bradnelson): Reuse SharedFunctionInfos used here when
      // compiling the wasm module.
      Handle<SharedFunctionInfo> shared =
          Compiler::GetSharedFunctionInfo(decl->fun(), script_, info_);
      shared->set_is_toplevel(false);
      ParseInfo info(&zone, script_);
      info.set_shared_info(shared);
      info.set_toplevel(false);
      info.set_language_mode(decl->fun()->scope()->language_mode());
      info.set_allow_lazy_parsing(false);
      info.set_function_literal_id(shared->function_literal_id());
      info.set_ast_value_factory(ast_value_factory_);
      info.set_ast_value_factory_owned(false);
      // Create fresh function scope to use to parse the function in.
      new_func_scope = new (info.zone()) DeclarationScope(
          info.zone(), decl->fun()->scope()->outer_scope(), FUNCTION_SCOPE);
      info.set_asm_function_scope(new_func_scope);
      if (!Compiler::ParseAndAnalyze(&info)) {
        typer_failed_ = true;
        return;
      }
      FunctionLiteral* func = info.literal();
      DCHECK_NOT_NULL(func);
      decl->set_fun(func);
    }
    if (!typer_->ValidateInnerFunction(decl)) {
      typer_failed_ = true;
      decl->set_fun(old_func);
      if (new_func_scope != nullptr) {
        DCHECK_EQ(new_func_scope, decl->scope()->inner_scope());
        if (!decl->scope()->RemoveInnerScope(new_func_scope)) {
          UNREACHABLE();
        }
      }
      return;
    }
    current_function_builder_ = LookupOrInsertFunction(decl->proxy()->var());
    scope_ = kFuncScope;

    // Record start of the function, used as position for the stack check.
    current_function_builder_->SetAsmFunctionStartPosition(
        decl->fun()->start_position());

    RECURSE(Visit(decl->fun()));
    decl->set_fun(old_func);
    if (new_func_scope != nullptr) {
      DCHECK_EQ(new_func_scope, decl->scope()->inner_scope());
      if (!decl->scope()->RemoveInnerScope(new_func_scope)) {
        UNREACHABLE();
      }
    }
    scope_ = kModuleScope;
    current_function_builder_ = nullptr;
    local_variables_.Clear();
    typer_->ClearFunctionNodeTypes();
  }

  void VisitStatements(ZoneList<Statement*>* stmts) {
    for (int i = 0; i < stmts->length(); ++i) {
      Statement* stmt = stmts->at(i);
      ExpressionStatement* e = stmt->AsExpressionStatement();
      if (e != nullptr && e->expression()->IsUndefinedLiteral()) {
        continue;
      }
      RECURSE(Visit(stmt));
      if (typer_failed_) break;
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
    if (scope_ == kFuncScope) {
      BlockVisitor visitor(this, stmt->AsBreakableStatement(), kExprBlock);
      RECURSE(VisitStatements(stmt->statements()));
    } else {
      RECURSE(VisitStatements(stmt->statements()));
    }
  }

  class BlockVisitor {
   private:
    AsmWasmBuilderImpl* builder_;

   public:
    BlockVisitor(AsmWasmBuilderImpl* builder, BreakableStatement* stmt,
                 WasmOpcode opcode)
        : builder_(builder) {
      builder_->breakable_blocks_.push_back(
          std::make_pair(stmt, opcode == kExprLoop));
      // block and loops have a type immediate.
      builder_->current_function_builder_->EmitWithU8(opcode, kLocalVoid);
    }
    ~BlockVisitor() {
      builder_->current_function_builder_->Emit(kExprEnd);
      builder_->breakable_blocks_.pop_back();
    }
  };

  void VisitExpressionStatement(ExpressionStatement* stmt) {
    VisitForEffect(stmt->expression());
  }

  void VisitForEffect(Expression* expr) {
    if (expr->IsAssignment()) {
      // Don't emit drops for assignments. Instead use SetLocal/GetLocal.
      VisitAssignment(expr->AsAssignment(), kDrop);
      return;
    }
    if (expr->IsCall()) {
      // Only emit a drop if the call has a non-void return value.
      if (VisitCallExpression(expr->AsCall()) && scope_ == kFuncScope) {
        current_function_builder_->Emit(kExprDrop);
      }
      return;
    }
    if (expr->IsBinaryOperation()) {
      BinaryOperation* binop = expr->AsBinaryOperation();
      if (binop->op() == Token::COMMA) {
        VisitForEffect(binop->left());
        VisitForEffect(binop->right());
        return;
      }
    }
    RECURSE(Visit(expr));
    if (scope_ == kFuncScope) current_function_builder_->Emit(kExprDrop);
  }

  void VisitEmptyStatement(EmptyStatement* stmt) {}

  void VisitEmptyParentheses(EmptyParentheses* paren) { UNREACHABLE(); }

  void VisitGetIterator(GetIterator* expr) { UNREACHABLE(); }

  void VisitIfStatement(IfStatement* stmt) {
    DCHECK_EQ(kFuncScope, scope_);
    RECURSE(Visit(stmt->condition()));
    current_function_builder_->EmitWithU8(kExprIf, kLocalVoid);
    // WASM ifs come with implement blocks for both arms.
    breakable_blocks_.push_back(std::make_pair(nullptr, false));
    if (stmt->HasThenStatement()) {
      RECURSE(Visit(stmt->then_statement()));
    }
    if (stmt->HasElseStatement()) {
      current_function_builder_->Emit(kExprElse);
      RECURSE(Visit(stmt->else_statement()));
    }
    current_function_builder_->Emit(kExprEnd);
    breakable_blocks_.pop_back();
  }

  void DoBreakOrContinue(BreakableStatement* target, bool is_continue) {
    DCHECK_EQ(kFuncScope, scope_);
    for (int i = static_cast<int>(breakable_blocks_.size()) - 1; i >= 0; --i) {
      auto elem = breakable_blocks_.at(i);
      if (elem.first == target && elem.second == is_continue) {
        int block_distance = static_cast<int>(breakable_blocks_.size() - i - 1);
        current_function_builder_->Emit(kExprBr);
        current_function_builder_->EmitVarInt(block_distance);
        return;
      }
    }
    UNREACHABLE();  // statement not found
  }

  void VisitContinueStatement(ContinueStatement* stmt) {
    DoBreakOrContinue(stmt->target(), true);
  }

  void VisitBreakStatement(BreakStatement* stmt) {
    DoBreakOrContinue(stmt->target(), false);
  }

  void VisitReturnStatement(ReturnStatement* stmt) {
    if (scope_ == kModuleScope) {
      if (typer_finished_) {
        typer_->FailWithMessage("Module has multiple returns.");
        typer_failed_ = true;
        return;
      }
      if (!typer_->ValidateAfterFunctionsPhase()) {
        typer_failed_ = true;
        return;
      }
      typer_finished_ = true;
      scope_ = kExportScope;
      RECURSE(Visit(stmt->expression()));
      scope_ = kModuleScope;
    } else if (scope_ == kFuncScope) {
      RECURSE(Visit(stmt->expression()));
      current_function_builder_->Emit(kExprReturn);
    } else {
      UNREACHABLE();
    }
  }

  void VisitWithStatement(WithStatement* stmt) { UNREACHABLE(); }

  void HandleCase(CaseNode* node,
                  ZoneMap<int, unsigned int>& case_to_block,
                  VariableProxy* tag, int default_block, int if_depth) {
    int prev_if_depth = if_depth;
    if (node->left != nullptr) {
      VisitVariableProxy(tag);
      current_function_builder_->EmitI32Const(node->begin);
      current_function_builder_->Emit(kExprI32LtS);
      current_function_builder_->EmitWithU8(kExprIf, kLocalVoid);
      if_depth++;
      breakable_blocks_.push_back(std::make_pair(nullptr, false));
      HandleCase(node->left, case_to_block, tag, default_block, if_depth);
      current_function_builder_->Emit(kExprElse);
    }
    if (node->right != nullptr) {
      VisitVariableProxy(tag);
      current_function_builder_->EmitI32Const(node->end);
      current_function_builder_->Emit(kExprI32GtS);
      current_function_builder_->EmitWithU8(kExprIf, kLocalVoid);
      if_depth++;
      breakable_blocks_.push_back(std::make_pair(nullptr, false));
      HandleCase(node->right, case_to_block, tag, default_block, if_depth);
      current_function_builder_->Emit(kExprElse);
    }
    if (node->begin == node->end) {
      VisitVariableProxy(tag);
      current_function_builder_->EmitI32Const(node->begin);
      current_function_builder_->Emit(kExprI32Eq);
      current_function_builder_->EmitWithU8(kExprIf, kLocalVoid);
      DCHECK(case_to_block.find(node->begin) != case_to_block.end());
      current_function_builder_->Emit(kExprBr);
      current_function_builder_->EmitVarInt(1 + if_depth +
                                            case_to_block[node->begin]);
      current_function_builder_->Emit(kExprEnd);
    } else {
      if (node->begin != 0) {
        VisitVariableProxy(tag);
        current_function_builder_->EmitI32Const(node->begin);
        current_function_builder_->Emit(kExprI32Sub);
      } else {
        VisitVariableProxy(tag);
      }
      current_function_builder_->Emit(kExprBrTable);
      current_function_builder_->EmitVarInt(node->end - node->begin + 1);
      for (int v = node->begin; v <= node->end; ++v) {
        if (case_to_block.find(v) != case_to_block.end()) {
          uint32_t target = if_depth + case_to_block[v];
          current_function_builder_->EmitVarInt(target);
        } else {
          uint32_t target = if_depth + default_block;
          current_function_builder_->EmitVarInt(target);
        }
        if (v == kMaxInt) {
          break;
        }
      }
      uint32_t target = if_depth + default_block;
      current_function_builder_->EmitVarInt(target);
    }

    while (if_depth-- != prev_if_depth) {
      breakable_blocks_.pop_back();
      current_function_builder_->Emit(kExprEnd);
    }
  }

  void VisitSwitchStatement(SwitchStatement* stmt) {
    VariableProxy* tag = stmt->tag()->AsVariableProxy();
    DCHECK_NOT_NULL(tag);
    ZoneList<CaseClause*>* clauses = stmt->cases();
    int case_count = clauses->length();
    if (case_count == 0) {
      return;
    }
    BlockVisitor visitor(this, stmt->AsBreakableStatement(), kExprBlock);
    ZoneVector<BlockVisitor*> blocks(zone_);
    ZoneVector<int32_t> cases(zone_);
    ZoneMap<int, unsigned int> case_to_block(zone_);
    bool has_default = false;
    for (int i = case_count - 1; i >= 0; --i) {
      CaseClause* clause = clauses->at(i);
      blocks.push_back(new BlockVisitor(this, nullptr, kExprBlock));
      if (!clause->is_default()) {
        Literal* label = clause->label()->AsLiteral();
        Handle<Object> value = label->value();
        int32_t label_value;
        bool label_is_i32 = value->ToInt32(&label_value);
        DCHECK(value->IsNumber() && label_is_i32);
        (void)label_is_i32;
        case_to_block[label_value] = i;
        cases.push_back(label_value);
      } else {
        DCHECK_EQ(i, case_count - 1);
        has_default = true;
      }
    }
    if (!has_default || case_count > 1) {
      int default_block = has_default ? case_count - 1 : case_count;
      BlockVisitor switch_logic_block(this, nullptr, kExprBlock);
      CaseNode* root = OrderCases(&cases, zone_);
      HandleCase(root, case_to_block, tag, default_block, 0);
      if (root->left != nullptr || root->right != nullptr ||
          root->begin == root->end) {
        current_function_builder_->Emit(kExprBr);
        current_function_builder_->EmitVarInt(default_block);
      }
    }
    for (int i = 0; i < case_count; ++i) {
      CaseClause* clause = clauses->at(i);
      RECURSE(VisitStatements(clause->statements()));
      BlockVisitor* v = blocks.at(case_count - i - 1);
      blocks.pop_back();
      delete v;
    }
  }

  void VisitCaseClause(CaseClause* clause) { UNREACHABLE(); }

  void VisitDoWhileStatement(DoWhileStatement* stmt) {
    DCHECK_EQ(kFuncScope, scope_);
    BlockVisitor block(this, stmt->AsBreakableStatement(), kExprBlock);
    BlockVisitor loop(this, stmt->AsBreakableStatement(), kExprLoop);
    RECURSE(Visit(stmt->body()));
    RECURSE(Visit(stmt->cond()));
    current_function_builder_->EmitWithU8(kExprIf, kLocalVoid);
    current_function_builder_->EmitWithU8(kExprBr, 1);
    current_function_builder_->Emit(kExprEnd);
  }

  void VisitWhileStatement(WhileStatement* stmt) {
    DCHECK_EQ(kFuncScope, scope_);
    BlockVisitor block(this, stmt->AsBreakableStatement(), kExprBlock);
    BlockVisitor loop(this, stmt->AsBreakableStatement(), kExprLoop);
    RECURSE(Visit(stmt->cond()));
    breakable_blocks_.push_back(std::make_pair(nullptr, false));
    current_function_builder_->EmitWithU8(kExprIf, kLocalVoid);
    RECURSE(Visit(stmt->body()));
    current_function_builder_->EmitWithU8(kExprBr, 1);
    current_function_builder_->Emit(kExprEnd);
    breakable_blocks_.pop_back();
  }

  void VisitForStatement(ForStatement* stmt) {
    DCHECK_EQ(kFuncScope, scope_);
    if (stmt->init() != nullptr) {
      RECURSE(Visit(stmt->init()));
    }
    BlockVisitor block(this, stmt->AsBreakableStatement(), kExprBlock);
    BlockVisitor loop(this, stmt->AsBreakableStatement(), kExprLoop);
    if (stmt->cond() != nullptr) {
      RECURSE(Visit(stmt->cond()));
      current_function_builder_->Emit(kExprI32Eqz);
      current_function_builder_->EmitWithU8(kExprIf, kLocalVoid);
      current_function_builder_->EmitWithU8(kExprBr, 2);
      current_function_builder_->Emit(kExprEnd);
    }
    if (stmt->body() != nullptr) {
      RECURSE(Visit(stmt->body()));
    }
    if (stmt->next() != nullptr) {
      RECURSE(Visit(stmt->next()));
    }
    current_function_builder_->EmitWithU8(kExprBr, 0);
  }

  void VisitForInStatement(ForInStatement* stmt) { UNREACHABLE(); }

  void VisitForOfStatement(ForOfStatement* stmt) { UNREACHABLE(); }

  void VisitTryCatchStatement(TryCatchStatement* stmt) { UNREACHABLE(); }

  void VisitTryFinallyStatement(TryFinallyStatement* stmt) { UNREACHABLE(); }

  void VisitDebuggerStatement(DebuggerStatement* stmt) { UNREACHABLE(); }

  void VisitFunctionLiteral(FunctionLiteral* expr) {
    DeclarationScope* scope = expr->scope();
    if (scope_ == kFuncScope) {
      if (auto* func_type = typer_->TypeOf(expr)->AsFunctionType()) {
        // Add the parameters for the function.
        const auto& arguments = func_type->Arguments();
        for (int i = 0; i < expr->parameter_count(); ++i) {
          ValueType type = TypeFrom(arguments[i]);
          DCHECK_NE(kWasmStmt, type);
          InsertParameter(scope->parameter(i), type, i);
        }
      } else {
        UNREACHABLE();
      }
    }
    RECURSE(VisitDeclarations(scope->declarations()));
    if (typer_failed_) return;
    RECURSE(VisitStatements(expr->body()));
    if (scope_ == kFuncScope) {
      // Finish the function-body scope block.
      current_function_builder_->Emit(kExprEnd);
    }
  }

  void VisitNativeFunctionLiteral(NativeFunctionLiteral* expr) {
    UNREACHABLE();
  }

  void VisitConditional(Conditional* expr) {
    DCHECK_EQ(kFuncScope, scope_);
    RECURSE(Visit(expr->condition()));
    // WASM ifs come with implicit blocks for both arms.
    breakable_blocks_.push_back(std::make_pair(nullptr, false));
    ValueTypeCode type;
    switch (TypeOf(expr)) {
      case kWasmI32:
        type = kLocalI32;
        break;
      case kWasmI64:
        type = kLocalI64;
        break;
      case kWasmF32:
        type = kLocalF32;
        break;
      case kWasmF64:
        type = kLocalF64;
        break;
      default:
        UNREACHABLE();
    }
    current_function_builder_->EmitWithU8(kExprIf, type);
    RECURSE(Visit(expr->then_expression()));
    current_function_builder_->Emit(kExprElse);
    RECURSE(Visit(expr->else_expression()));
    current_function_builder_->Emit(kExprEnd);
    breakable_blocks_.pop_back();
  }

  bool VisitStdlibConstant(Variable* var) {
    AsmTyper::StandardMember standard_object =
        typer_->VariableAsStandardMember(var);
    double value;
    switch (standard_object) {
      case AsmTyper::kInfinity: {
        value = std::numeric_limits<double>::infinity();
        break;
      }
      case AsmTyper::kNaN: {
        value = std::numeric_limits<double>::quiet_NaN();
        break;
      }
      case AsmTyper::kMathE: {
        value = M_E;
        break;
      }
      case AsmTyper::kMathLN10: {
        value = M_LN10;
        break;
      }
      case AsmTyper::kMathLN2: {
        value = M_LN2;
        break;
      }
      case AsmTyper::kMathLOG10E: {
        value = M_LOG10E;
        break;
      }
      case AsmTyper::kMathLOG2E: {
        value = M_LOG2E;
        break;
      }
      case AsmTyper::kMathPI: {
        value = M_PI;
        break;
      }
      case AsmTyper::kMathSQRT1_2: {
        value = M_SQRT1_2;
        break;
      }
      case AsmTyper::kMathSQRT2: {
        value = M_SQRT2;
        break;
      }
      default: { return false; }
    }
    byte code[] = {WASM_F64(value)};
    current_function_builder_->EmitCode(code, sizeof(code));
    return true;
  }

  void VisitVariableProxy(VariableProxy* expr) {
    if (scope_ == kFuncScope || scope_ == kInitScope) {
      Variable* var = expr->var();
      if (VisitStdlibConstant(var)) {
        return;
      }
      ValueType var_type = TypeOf(expr);
      DCHECK_NE(kWasmStmt, var_type);
      if (var->IsContextSlot()) {
        current_function_builder_->EmitWithVarInt(
            kExprGetGlobal, LookupOrInsertGlobal(var, var_type));
      } else {
        current_function_builder_->EmitGetLocal(
            LookupOrInsertLocal(var, var_type));
      }
    } else if (scope_ == kExportScope) {
      Variable* var = expr->var();
      DCHECK(var->is_function());
      WasmFunctionBuilder* function = LookupOrInsertFunction(var);
      function->ExportAs(CStrVector(AsmWasmBuilder::single_function_name));
    }
  }

  void VisitLiteral(Literal* expr) {
    Handle<Object> value = expr->value();
    if (!(value->IsNumber() || expr->raw_value()->IsTrue() ||
          expr->raw_value()->IsFalse()) ||
        (scope_ != kFuncScope && scope_ != kInitScope)) {
      return;
    }
    AsmType* type = typer_->TypeOf(expr);
    DCHECK_NE(type, AsmType::None());

    if (type->IsA(AsmType::Signed())) {
      int32_t i = 0;
      if (!value->ToInt32(&i)) {
        UNREACHABLE();
      }
      byte code[] = {WASM_I32V(i)};
      current_function_builder_->EmitCode(code, sizeof(code));
    } else if (type->IsA(AsmType::Unsigned()) || type->IsA(AsmType::FixNum())) {
      uint32_t u = 0;
      if (!value->ToUint32(&u)) {
        UNREACHABLE();
      }
      int32_t i = static_cast<int32_t>(u);
      byte code[] = {WASM_I32V(i)};
      current_function_builder_->EmitCode(code, sizeof(code));
    } else if (type->IsA(AsmType::Int())) {
      // The parser can collapse !0, !1 etc to true / false.
      // Allow these as int literals.
      if (expr->raw_value()->IsTrue()) {
        byte code[] = {WASM_I32V(1)};
        current_function_builder_->EmitCode(code, sizeof(code));
      } else if (expr->raw_value()->IsFalse()) {
        byte code[] = {WASM_I32V(0)};
        current_function_builder_->EmitCode(code, sizeof(code));
      } else if (expr->raw_value()->IsNumber()) {
        // This can happen when -x becomes x * -1 (due to the parser).
        int32_t i = 0;
        if (!value->ToInt32(&i) || i != -1) {
          UNREACHABLE();
        }
        byte code[] = {WASM_I32V(i)};
        current_function_builder_->EmitCode(code, sizeof(code));
      } else {
        UNREACHABLE();
      }
    } else if (type->IsA(AsmType::Double())) {
      // TODO(bradnelson): Pattern match the case where negation occurs and
      // emit f64.neg instead.
      double val = expr->raw_value()->AsNumber();
      byte code[] = {WASM_F64(val)};
      current_function_builder_->EmitCode(code, sizeof(code));
    } else if (type->IsA(AsmType::Float())) {
      // This can happen when -fround(x) becomes fround(x) * 1.0[float]
      // (due to the parser).
      // TODO(bradnelson): Pattern match this and emit f32.neg instead.
      double val = expr->raw_value()->AsNumber();
      DCHECK_EQ(-1.0, val);
      byte code[] = {WASM_F32(val)};
      current_function_builder_->EmitCode(code, sizeof(code));
    } else {
      UNREACHABLE();
    }
  }

  void VisitRegExpLiteral(RegExpLiteral* expr) { UNREACHABLE(); }

  void VisitObjectLiteral(ObjectLiteral* expr) {
    ZoneList<ObjectLiteralProperty*>* props = expr->properties();
    for (int i = 0; i < props->length(); ++i) {
      ObjectLiteralProperty* prop = props->at(i);
      DCHECK_EQ(kExportScope, scope_);
      VariableProxy* expr = prop->value()->AsVariableProxy();
      DCHECK_NOT_NULL(expr);
      Variable* var = expr->var();
      Literal* name = prop->key()->AsLiteral();
      DCHECK_NOT_NULL(name);
      DCHECK(name->IsPropertyName());
      Handle<String> function_name = name->AsPropertyName();
      int length;
      std::unique_ptr<char[]> utf8 = function_name->ToCString(
          DISALLOW_NULLS, FAST_STRING_TRAVERSAL, &length);
      if (var->is_function()) {
        WasmFunctionBuilder* function = LookupOrInsertFunction(var);
        function->ExportAs({utf8.get(), length});
      }
    }
  }

  void VisitArrayLiteral(ArrayLiteral* expr) { UNREACHABLE(); }

  void LoadInitFunction() {
    current_function_builder_ = init_function_;
    scope_ = kInitScope;
  }

  void UnLoadInitFunction() {
    scope_ = kModuleScope;
    current_function_builder_ = nullptr;
  }

  struct FunctionTableIndices : public ZoneObject {
    uint32_t start_index;
    uint32_t signature_index;
  };

  FunctionTableIndices* LookupOrAddFunctionTable(VariableProxy* table,
                                                 Property* p) {
    FunctionTableIndices* indices = LookupFunctionTable(table->var());
    if (indices != nullptr) {
      // Already setup.
      return indices;
    }
    indices = new (zone()) FunctionTableIndices();
    auto* func_type = typer_->TypeOf(p)->AsFunctionType();
    auto* func_table_type = typer_->TypeOf(p->obj()->AsVariableProxy()->var())
                                ->AsFunctionTableType();
    const auto& arguments = func_type->Arguments();
    ValueType return_type = TypeFrom(func_type->ReturnType());
    FunctionSig::Builder sig(zone(), return_type == kWasmStmt ? 0 : 1,
                             arguments.size());
    if (return_type != kWasmStmt) {
      sig.AddReturn(return_type);
    }
    for (auto* arg : arguments) {
      sig.AddParam(TypeFrom(arg));
    }
    uint32_t signature_index = builder_->AddSignature(sig.Build());
    indices->start_index = builder_->AllocateIndirectFunctions(
        static_cast<uint32_t>(func_table_type->length()));
    indices->signature_index = signature_index;
    ZoneHashMap::Entry* entry = function_tables_.LookupOrInsert(
        table->var(), ComputePointerHash(table->var()),
        ZoneAllocationPolicy(zone()));
    entry->value = indices;
    return indices;
  }

  FunctionTableIndices* LookupFunctionTable(Variable* v) {
    ZoneHashMap::Entry* entry =
        function_tables_.Lookup(v, ComputePointerHash(v));
    if (entry == nullptr) {
      return nullptr;
    }
    return reinterpret_cast<FunctionTableIndices*>(entry->value);
  }

  void PopulateFunctionTable(VariableProxy* table, ArrayLiteral* funcs) {
    FunctionTableIndices* indices = LookupFunctionTable(table->var());
    // Ignore unused function tables.
    if (indices == nullptr) {
      return;
    }
    for (int i = 0; i < funcs->values()->length(); ++i) {
      VariableProxy* func = funcs->values()->at(i)->AsVariableProxy();
      DCHECK_NOT_NULL(func);
      builder_->SetIndirectFunction(
          indices->start_index + i,
          LookupOrInsertFunction(func->var())->func_index());
    }
  }

  class ImportedFunctionTable {
   private:
    class ImportedFunctionIndices : public ZoneObject {
     public:
      const char* name_;
      int name_length_;
      WasmModuleBuilder::SignatureMap signature_to_index_;

      ImportedFunctionIndices(const char* name, int name_length, Zone* zone)
          : name_(name), name_length_(name_length), signature_to_index_(zone) {}
    };
    ZoneHashMap table_;
    AsmWasmBuilderImpl* builder_;

   public:
    explicit ImportedFunctionTable(AsmWasmBuilderImpl* builder)
        : table_(ZoneHashMap::kDefaultHashMapCapacity,
                 ZoneAllocationPolicy(builder->zone())),
          builder_(builder) {}

    ImportedFunctionIndices* LookupOrInsertImport(Variable* v) {
      auto* entry = table_.LookupOrInsert(
          v, ComputePointerHash(v), ZoneAllocationPolicy(builder_->zone()));
      ImportedFunctionIndices* indices;
      if (entry->value == nullptr) {
        indices = new (builder_->zone())
            ImportedFunctionIndices(nullptr, 0, builder_->zone());
        entry->value = indices;
      } else {
        indices = reinterpret_cast<ImportedFunctionIndices*>(entry->value);
      }
      return indices;
    }

    void SetImportName(Variable* v, const char* name, int name_length) {
      auto* indices = LookupOrInsertImport(v);
      indices->name_ = name;
      indices->name_length_ = name_length;
      for (auto i : indices->signature_to_index_) {
        builder_->builder_->SetImportName(i.second, indices->name_,
                                          indices->name_length_);
      }
    }

    // Get a function's index (or allocate if new).
    uint32_t LookupOrInsertImportUse(Variable* v, FunctionSig* sig) {
      auto* indices = LookupOrInsertImport(v);
      WasmModuleBuilder::SignatureMap::iterator pos =
          indices->signature_to_index_.find(sig);
      if (pos != indices->signature_to_index_.end()) {
        return pos->second;
      } else {
        uint32_t index = builder_->builder_->AddImport(
            indices->name_, indices->name_length_, sig);
        indices->signature_to_index_[sig] = index;
        return index;
      }
    }
  };

  void EmitAssignmentLhs(Expression* target, AsmType** atype) {
    // Match the left hand side of the assignment.
    VariableProxy* target_var = target->AsVariableProxy();
    if (target_var != nullptr) {
      // Left hand side is a local or a global variable, no code on LHS.
      return;
    }

    Property* target_prop = target->AsProperty();
    if (target_prop != nullptr) {
      // Left hand side is a property access, i.e. the asm.js heap.
      VisitPropertyAndEmitIndex(target_prop, atype);
      return;
    }

    if (target_var == nullptr && target_prop == nullptr) {
      UNREACHABLE();  // invalid assignment.
    }
  }

  void EmitAssignmentRhs(Expression* target, Expression* value, bool* is_nop) {
    BinaryOperation* binop = value->AsBinaryOperation();
    if (binop != nullptr) {
      if (scope_ == kInitScope) {
        // Handle foreign variables in the initialization scope.
        Property* prop = binop->left()->AsProperty();
        if (binop->op() == Token::MUL) {
          DCHECK(binop->right()->IsLiteral());
          DCHECK_EQ(1.0, binop->right()->AsLiteral()->raw_value()->AsNumber());
          DCHECK(binop->right()->AsLiteral()->raw_value()->ContainsDot());
          DCHECK(target->IsVariableProxy());
          VisitForeignVariable(true, target->AsVariableProxy()->var(), prop);
          *is_nop = true;
          return;
        } else if (binop->op() == Token::BIT_OR) {
          DCHECK(binop->right()->IsLiteral());
          DCHECK_EQ(0.0, binop->right()->AsLiteral()->raw_value()->AsNumber());
          DCHECK(!binop->right()->AsLiteral()->raw_value()->ContainsDot());
          DCHECK(target->IsVariableProxy());
          VisitForeignVariable(false, target->AsVariableProxy()->var(), prop);
          *is_nop = true;
          return;
        } else {
          UNREACHABLE();
        }
      }
      if (MatchBinaryOperation(binop) == kAsIs) {
        VariableProxy* target_var = target->AsVariableProxy();
        VariableProxy* effective_value_var = GetLeft(binop)->AsVariableProxy();
        if (target_var != nullptr && effective_value_var != nullptr &&
            target_var->var() == effective_value_var->var()) {
          *is_nop = true;
          return;
        }
      }
    }
    RECURSE(Visit(value));
  }

  void EmitAssignment(Assignment* expr, AsmType* type, ValueFate fate) {
    // Match the left hand side of the assignment.
    VariableProxy* target_var = expr->target()->AsVariableProxy();
    if (target_var != nullptr) {
      // Left hand side is a local or a global variable.
      Variable* var = target_var->var();
      ValueType var_type = TypeOf(expr);
      DCHECK_NE(kWasmStmt, var_type);
      if (var->IsContextSlot()) {
        uint32_t index = LookupOrInsertGlobal(var, var_type);
        current_function_builder_->EmitWithVarInt(kExprSetGlobal, index);
        if (fate == kLeaveOnStack) {
          current_function_builder_->EmitWithVarInt(kExprGetGlobal, index);
        }
      } else {
        if (fate == kDrop) {
          current_function_builder_->EmitSetLocal(
              LookupOrInsertLocal(var, var_type));
        } else {
          current_function_builder_->EmitTeeLocal(
              LookupOrInsertLocal(var, var_type));
        }
      }
    }

    Property* target_prop = expr->target()->AsProperty();
    if (target_prop != nullptr) {
      // Left hand side is a property access, i.e. the asm.js heap.
      if (TypeOf(expr->value()) == kWasmF64 && expr->target()->IsProperty() &&
          typer_->TypeOf(expr->target()->AsProperty()->obj())
              ->IsA(AsmType::Float32Array())) {
        current_function_builder_->Emit(kExprF32ConvertF64);
      }
      // Note that unlike StoreMem, AsmjsStoreMem ignores out-of-bounds writes.
      WasmOpcode opcode;
      if (type == AsmType::Int8Array()) {
        opcode = kExprI32AsmjsStoreMem8;
      } else if (type == AsmType::Uint8Array()) {
        opcode = kExprI32AsmjsStoreMem8;
      } else if (type == AsmType::Int16Array()) {
        opcode = kExprI32AsmjsStoreMem16;
      } else if (type == AsmType::Uint16Array()) {
        opcode = kExprI32AsmjsStoreMem16;
      } else if (type == AsmType::Int32Array()) {
        opcode = kExprI32AsmjsStoreMem;
      } else if (type == AsmType::Uint32Array()) {
        opcode = kExprI32AsmjsStoreMem;
      } else if (type == AsmType::Float32Array()) {
        opcode = kExprF32AsmjsStoreMem;
      } else if (type == AsmType::Float64Array()) {
        opcode = kExprF64AsmjsStoreMem;
      } else {
        UNREACHABLE();
      }
      current_function_builder_->Emit(opcode);
      if (fate == kDrop) {
        // Asm.js stores to memory leave their result on the stack.
        current_function_builder_->Emit(kExprDrop);
      }
    }

    if (target_var == nullptr && target_prop == nullptr) {
      UNREACHABLE();  // invalid assignment.
    }
  }

  void VisitAssignment(Assignment* expr) {
    VisitAssignment(expr, kLeaveOnStack);
  }

  void VisitAssignment(Assignment* expr, ValueFate fate) {
    bool as_init = false;
    if (scope_ == kModuleScope) {
      // Skip extra assignment inserted by the parser when in this form:
      // (function Module(a, b, c) {... })
      if (expr->target()->IsVariableProxy() &&
          expr->target()->AsVariableProxy()->var()->is_sloppy_function_name()) {
        return;
      }
      Property* prop = expr->value()->AsProperty();
      if (prop != nullptr) {
        VariableProxy* vp = prop->obj()->AsVariableProxy();
        if (vp != nullptr && vp->var()->IsParameter() &&
            vp->var()->index() == 1) {
          VariableProxy* target = expr->target()->AsVariableProxy();
          if (typer_->TypeOf(target)->AsFFIType() != nullptr) {
            const AstRawString* name =
                prop->key()->AsLiteral()->AsRawPropertyName();
            imported_function_table_.SetImportName(
                target->var(), reinterpret_cast<const char*>(name->raw_data()),
                name->length());
          }
        }
        // Property values in module scope don't emit code, so return.
        return;
      }
      ArrayLiteral* funcs = expr->value()->AsArrayLiteral();
      if (funcs != nullptr) {
        VariableProxy* target = expr->target()->AsVariableProxy();
        DCHECK_NOT_NULL(target);
        PopulateFunctionTable(target, funcs);
        // Only add to the function table. No init needed.
        return;
      }
      if (expr->value()->IsCallNew()) {
        // No init code to emit for CallNew nodes.
        return;
      }
      as_init = true;
    }

    if (as_init) LoadInitFunction();
    AsmType* atype = AsmType::None();
    bool is_nop = false;
    EmitAssignmentLhs(expr->target(), &atype);
    EmitAssignmentRhs(expr->target(), expr->value(), &is_nop);
    if (!is_nop) {
      EmitAssignment(expr, atype, fate);
    }
    if (as_init) UnLoadInitFunction();
  }

  void VisitYield(Yield* expr) { UNREACHABLE(); }

  void VisitThrow(Throw* expr) { UNREACHABLE(); }

  void VisitForeignVariable(bool is_float, Variable* var, Property* expr) {
    DCHECK(expr->obj()->AsVariableProxy());
    DCHECK(VariableLocation::PARAMETER ==
           expr->obj()->AsVariableProxy()->var()->location());
    DCHECK_EQ(1, expr->obj()->AsVariableProxy()->var()->index());
    Literal* key_literal = expr->key()->AsLiteral();
    DCHECK_NOT_NULL(key_literal);
    if (!key_literal->value().is_null()) {
      Handle<Name> name =
          Object::ToName(isolate_, key_literal->value()).ToHandleChecked();
      ValueType type = is_float ? kWasmF64 : kWasmI32;
      foreign_variables_.push_back({name, var, type});
    }
  }

  void VisitPropertyAndEmitIndex(Property* expr, AsmType** atype) {
    Expression* obj = expr->obj();
    *atype = typer_->TypeOf(obj);
    int32_t size = (*atype)->ElementSizeInBytes();
    if (size == 1) {
      // Allow more general expression in byte arrays than the spec
      // strictly permits.
      // Early versions of Emscripten emit HEAP8[HEAP32[..]|0] in
      // places that strictly should be HEAP8[HEAP32[..]>>0].
      RECURSE(Visit(expr->key()));
      return;
    }

    Literal* value = expr->key()->AsLiteral();
    if (value) {
      DCHECK(value->raw_value()->IsNumber());
      DCHECK_EQ(kWasmI32, TypeOf(value));
      int32_t val = static_cast<int32_t>(value->raw_value()->AsNumber());
      // TODO(titzer): handle overflow here.
      current_function_builder_->EmitI32Const(val * size);
      return;
    }
    BinaryOperation* binop = expr->key()->AsBinaryOperation();
    if (binop) {
      DCHECK_EQ(Token::SAR, binop->op());
      DCHECK(binop->right()->AsLiteral()->raw_value()->IsNumber());
      DCHECK(kWasmI32 == TypeOf(binop->right()->AsLiteral()));
      DCHECK_EQ(size,
                1 << static_cast<int>(
                    binop->right()->AsLiteral()->raw_value()->AsNumber()));
      // Mask bottom bits to match asm.js behavior.
      RECURSE(Visit(binop->left()));
      current_function_builder_->EmitI32Const(~(size - 1));
      current_function_builder_->Emit(kExprI32And);
      return;
    }
    UNREACHABLE();
  }

  void VisitProperty(Property* expr) {
    AsmType* type = AsmType::None();
    VisitPropertyAndEmitIndex(expr, &type);
    WasmOpcode opcode;
    if (type == AsmType::Int8Array()) {
      opcode = kExprI32AsmjsLoadMem8S;
    } else if (type == AsmType::Uint8Array()) {
      opcode = kExprI32AsmjsLoadMem8U;
    } else if (type == AsmType::Int16Array()) {
      opcode = kExprI32AsmjsLoadMem16S;
    } else if (type == AsmType::Uint16Array()) {
      opcode = kExprI32AsmjsLoadMem16U;
    } else if (type == AsmType::Int32Array()) {
      opcode = kExprI32AsmjsLoadMem;
    } else if (type == AsmType::Uint32Array()) {
      opcode = kExprI32AsmjsLoadMem;
    } else if (type == AsmType::Float32Array()) {
      opcode = kExprF32AsmjsLoadMem;
    } else if (type == AsmType::Float64Array()) {
      opcode = kExprF64AsmjsLoadMem;
    } else {
      UNREACHABLE();
    }

    current_function_builder_->Emit(opcode);
  }

  bool VisitStdlibFunction(Call* call, VariableProxy* expr) {
    Variable* var = expr->var();
    AsmTyper::StandardMember standard_object =
        typer_->VariableAsStandardMember(var);
    ZoneList<Expression*>* args = call->arguments();
    ValueType call_type = TypeOf(call);

    switch (standard_object) {
      case AsmTyper::kNone: {
        return false;
      }
      case AsmTyper::kMathAcos: {
        VisitCallArgs(call);
        DCHECK_EQ(kWasmF64, call_type);
        current_function_builder_->Emit(kExprF64Acos);
        break;
      }
      case AsmTyper::kMathAsin: {
        VisitCallArgs(call);
        DCHECK_EQ(kWasmF64, call_type);
        current_function_builder_->Emit(kExprF64Asin);
        break;
      }
      case AsmTyper::kMathAtan: {
        VisitCallArgs(call);
        DCHECK_EQ(kWasmF64, call_type);
        current_function_builder_->Emit(kExprF64Atan);
        break;
      }
      case AsmTyper::kMathCos: {
        VisitCallArgs(call);
        DCHECK_EQ(kWasmF64, call_type);
        current_function_builder_->Emit(kExprF64Cos);
        break;
      }
      case AsmTyper::kMathSin: {
        VisitCallArgs(call);
        DCHECK_EQ(kWasmF64, call_type);
        current_function_builder_->Emit(kExprF64Sin);
        break;
      }
      case AsmTyper::kMathTan: {
        VisitCallArgs(call);
        DCHECK_EQ(kWasmF64, call_type);
        current_function_builder_->Emit(kExprF64Tan);
        break;
      }
      case AsmTyper::kMathExp: {
        VisitCallArgs(call);
        DCHECK_EQ(kWasmF64, call_type);
        current_function_builder_->Emit(kExprF64Exp);
        break;
      }
      case AsmTyper::kMathLog: {
        VisitCallArgs(call);
        DCHECK_EQ(kWasmF64, call_type);
        current_function_builder_->Emit(kExprF64Log);
        break;
      }
      case AsmTyper::kMathCeil: {
        VisitCallArgs(call);
        if (call_type == kWasmF32) {
          current_function_builder_->Emit(kExprF32Ceil);
        } else if (call_type == kWasmF64) {
          current_function_builder_->Emit(kExprF64Ceil);
        } else {
          UNREACHABLE();
        }
        break;
      }
      case AsmTyper::kMathFloor: {
        VisitCallArgs(call);
        if (call_type == kWasmF32) {
          current_function_builder_->Emit(kExprF32Floor);
        } else if (call_type == kWasmF64) {
          current_function_builder_->Emit(kExprF64Floor);
        } else {
          UNREACHABLE();
        }
        break;
      }
      case AsmTyper::kMathSqrt: {
        VisitCallArgs(call);
        if (call_type == kWasmF32) {
          current_function_builder_->Emit(kExprF32Sqrt);
        } else if (call_type == kWasmF64) {
          current_function_builder_->Emit(kExprF64Sqrt);
        } else {
          UNREACHABLE();
        }
        break;
      }
      case AsmTyper::kMathClz32: {
        VisitCallArgs(call);
        DCHECK(call_type == kWasmI32);
        current_function_builder_->Emit(kExprI32Clz);
        break;
      }
      case AsmTyper::kMathAbs: {
        if (call_type == kWasmI32) {
          WasmTemporary tmp(current_function_builder_, kWasmI32);

          // if set_local(tmp, x) < 0
          Visit(call->arguments()->at(0));
          current_function_builder_->EmitTeeLocal(tmp.index());
          byte code[] = {WASM_ZERO};
          current_function_builder_->EmitCode(code, sizeof(code));
          current_function_builder_->Emit(kExprI32LtS);
          current_function_builder_->EmitWithU8(kExprIf, kLocalI32);

          // then (0 - tmp)
          current_function_builder_->EmitCode(code, sizeof(code));
          current_function_builder_->EmitGetLocal(tmp.index());
          current_function_builder_->Emit(kExprI32Sub);

          // else tmp
          current_function_builder_->Emit(kExprElse);
          current_function_builder_->EmitGetLocal(tmp.index());
          // end
          current_function_builder_->Emit(kExprEnd);

        } else if (call_type == kWasmF32) {
          VisitCallArgs(call);
          current_function_builder_->Emit(kExprF32Abs);
        } else if (call_type == kWasmF64) {
          VisitCallArgs(call);
          current_function_builder_->Emit(kExprF64Abs);
        } else {
          UNREACHABLE();
        }
        break;
      }
      case AsmTyper::kMathMin: {
        // TODO(bradnelson): Change wasm to match Math.min in asm.js mode.
        if (call_type == kWasmI32) {
          WasmTemporary tmp_x(current_function_builder_, kWasmI32);
          WasmTemporary tmp_y(current_function_builder_, kWasmI32);

          // if set_local(tmp_x, x) < set_local(tmp_y, y)
          Visit(call->arguments()->at(0));
          current_function_builder_->EmitTeeLocal(tmp_x.index());

          Visit(call->arguments()->at(1));
          current_function_builder_->EmitTeeLocal(tmp_y.index());

          current_function_builder_->Emit(kExprI32LeS);
          current_function_builder_->EmitWithU8(kExprIf, kLocalI32);

          // then tmp_x
          current_function_builder_->EmitGetLocal(tmp_x.index());

          // else tmp_y
          current_function_builder_->Emit(kExprElse);
          current_function_builder_->EmitGetLocal(tmp_y.index());
          current_function_builder_->Emit(kExprEnd);

        } else if (call_type == kWasmF32) {
          VisitCallArgs(call);
          current_function_builder_->Emit(kExprF32Min);
        } else if (call_type == kWasmF64) {
          VisitCallArgs(call);
          current_function_builder_->Emit(kExprF64Min);
        } else {
          UNREACHABLE();
        }
        break;
      }
      case AsmTyper::kMathMax: {
        // TODO(bradnelson): Change wasm to match Math.max in asm.js mode.
        if (call_type == kWasmI32) {
          WasmTemporary tmp_x(current_function_builder_, kWasmI32);
          WasmTemporary tmp_y(current_function_builder_, kWasmI32);

          // if set_local(tmp_x, x) < set_local(tmp_y, y)
          Visit(call->arguments()->at(0));

          current_function_builder_->EmitTeeLocal(tmp_x.index());

          Visit(call->arguments()->at(1));
          current_function_builder_->EmitTeeLocal(tmp_y.index());

          current_function_builder_->Emit(kExprI32LeS);
          current_function_builder_->EmitWithU8(kExprIf, kLocalI32);

          // then tmp_y
          current_function_builder_->EmitGetLocal(tmp_y.index());

          // else tmp_x
          current_function_builder_->Emit(kExprElse);
          current_function_builder_->EmitGetLocal(tmp_x.index());
          current_function_builder_->Emit(kExprEnd);

        } else if (call_type == kWasmF32) {
          VisitCallArgs(call);
          current_function_builder_->Emit(kExprF32Max);
        } else if (call_type == kWasmF64) {
          VisitCallArgs(call);
          current_function_builder_->Emit(kExprF64Max);
        } else {
          UNREACHABLE();
        }
        break;
      }
      case AsmTyper::kMathAtan2: {
        VisitCallArgs(call);
        DCHECK_EQ(kWasmF64, call_type);
        current_function_builder_->Emit(kExprF64Atan2);
        break;
      }
      case AsmTyper::kMathPow: {
        VisitCallArgs(call);
        DCHECK_EQ(kWasmF64, call_type);
        current_function_builder_->Emit(kExprF64Pow);
        break;
      }
      case AsmTyper::kMathImul: {
        VisitCallArgs(call);
        current_function_builder_->Emit(kExprI32Mul);
        break;
      }
      case AsmTyper::kMathFround: {
        DCHECK(args->length() == 1);
        Literal* literal = args->at(0)->AsLiteral();
        if (literal != nullptr) {
          // constant fold Math.fround(#const);
          if (literal->raw_value()->IsNumber()) {
            float val = static_cast<float>(literal->raw_value()->AsNumber());
            byte code[] = {WASM_F32(val)};
            current_function_builder_->EmitCode(code, sizeof(code));
            return true;
          }
        }
        VisitCallArgs(call);
        static const bool kDontIgnoreSign = false;
        switch (TypeIndexOf(args->at(0), kDontIgnoreSign)) {
          case kInt32:
          case kFixnum:
            current_function_builder_->Emit(kExprF32SConvertI32);
            break;
          case kUint32:
            current_function_builder_->Emit(kExprF32UConvertI32);
            break;
          case kFloat32:
            break;
          case kFloat64:
            current_function_builder_->Emit(kExprF32ConvertF64);
            break;
          default:
            UNREACHABLE();
        }
        break;
      }
      default: {
        UNREACHABLE();
        break;
      }
    }
    return true;
  }

  void VisitCallArgs(Call* expr) {
    ZoneList<Expression*>* args = expr->arguments();
    for (int i = 0; i < args->length(); ++i) {
      Expression* arg = args->at(i);
      RECURSE(Visit(arg));
    }
  }

  void VisitCall(Call* expr) { VisitCallExpression(expr); }

  bool VisitCallExpression(Call* expr) {
    Call::CallType call_type = expr->GetCallType();
    bool returns_value = true;

    // Save the parent now, it might be overwritten in VisitCallArgs.
    BinaryOperation* parent_binop = parent_binop_;

    switch (call_type) {
      case Call::OTHER_CALL: {
        VariableProxy* proxy = expr->expression()->AsVariableProxy();
        if (proxy != nullptr) {
          DCHECK(kFuncScope == scope_ ||
                 typer_->VariableAsStandardMember(proxy->var()) ==
                     AsmTyper::kMathFround);
          if (VisitStdlibFunction(expr, proxy)) {
            return true;
          }
        }
        DCHECK(kFuncScope == scope_);
        VariableProxy* vp = expr->expression()->AsVariableProxy();
        DCHECK_NOT_NULL(vp);
        if (typer_->TypeOf(vp)->AsFFIType() != nullptr) {
          ValueType return_type = TypeOf(expr);
          ZoneList<Expression*>* args = expr->arguments();
          FunctionSig::Builder sig(zone(), return_type == kWasmStmt ? 0 : 1,
                                   args->length());
          if (return_type != kWasmStmt) {
            sig.AddReturn(return_type);
          } else {
            returns_value = false;
          }
          for (int i = 0; i < args->length(); ++i) {
            sig.AddParam(TypeOf(args->at(i)));
          }
          uint32_t index = imported_function_table_.LookupOrInsertImportUse(
              vp->var(), sig.Build());
          VisitCallArgs(expr);
          // For non-void functions, we must know the parent node.
          DCHECK_IMPLIES(returns_value, parent_binop != nullptr);
          DCHECK_IMPLIES(returns_value, parent_binop->left() == expr ||
                                            parent_binop->right() == expr);
          int pos = expr->position();
          int parent_pos = returns_value ? parent_binop->position() : pos;
          current_function_builder_->AddAsmWasmOffset(pos, parent_pos);
          current_function_builder_->Emit(kExprCallFunction);
          current_function_builder_->EmitVarInt(index);
        } else {
          WasmFunctionBuilder* function = LookupOrInsertFunction(vp->var());
          VisitCallArgs(expr);
          current_function_builder_->AddAsmWasmOffset(expr->position(),
                                                      expr->position());
          current_function_builder_->Emit(kExprCallFunction);
          current_function_builder_->EmitDirectCallIndex(
              function->func_index());
          returns_value = function->signature()->return_count() > 0;
        }
        break;
      }
      case Call::KEYED_PROPERTY_CALL: {
        DCHECK_EQ(kFuncScope, scope_);
        Property* p = expr->expression()->AsProperty();
        DCHECK_NOT_NULL(p);
        VariableProxy* var = p->obj()->AsVariableProxy();
        DCHECK_NOT_NULL(var);
        FunctionTableIndices* indices = LookupOrAddFunctionTable(var, p);
        Visit(p->key());  // TODO(titzer): should use RECURSE()

        // We have to use a temporary for the correct order of evaluation.
        current_function_builder_->EmitI32Const(indices->start_index);
        current_function_builder_->Emit(kExprI32Add);
        WasmTemporary tmp(current_function_builder_, kWasmI32);
        current_function_builder_->EmitSetLocal(tmp.index());

        VisitCallArgs(expr);

        current_function_builder_->EmitGetLocal(tmp.index());
        current_function_builder_->AddAsmWasmOffset(expr->position(),
                                                    expr->position());
        current_function_builder_->Emit(kExprCallIndirect);
        current_function_builder_->EmitVarInt(indices->signature_index);
        current_function_builder_->EmitVarInt(0);  // table index
        returns_value =
            builder_->GetSignature(indices->signature_index)->return_count() >
            0;
        break;
      }
      default:
        UNREACHABLE();
    }
    return returns_value;
  }

  void VisitCallNew(CallNew* expr) { UNREACHABLE(); }

  void VisitCallRuntime(CallRuntime* expr) { UNREACHABLE(); }

  void VisitUnaryOperation(UnaryOperation* expr) {
    RECURSE(Visit(expr->expression()));
    switch (expr->op()) {
      case Token::NOT: {
        DCHECK_EQ(kWasmI32, TypeOf(expr->expression()));
        current_function_builder_->Emit(kExprI32Eqz);
        break;
      }
      default:
        UNREACHABLE();
    }
  }

  void VisitCountOperation(CountOperation* expr) { UNREACHABLE(); }

  bool MatchIntBinaryOperation(BinaryOperation* expr, Token::Value op,
                               int32_t val) {
    DCHECK_NOT_NULL(expr->right());
    if (expr->op() == op && expr->right()->IsLiteral() &&
        TypeOf(expr) == kWasmI32) {
      Literal* right = expr->right()->AsLiteral();
      if (right->raw_value()->IsNumber() &&
          static_cast<int32_t>(right->raw_value()->AsNumber()) == val) {
        return true;
      }
    }
    return false;
  }

  bool MatchDoubleBinaryOperation(BinaryOperation* expr, Token::Value op,
                                  double val) {
    DCHECK_NOT_NULL(expr->right());
    if (expr->op() == op && expr->right()->IsLiteral() &&
        TypeOf(expr) == kWasmF64) {
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
        (TypeOf(expr->left()) == kWasmI32)) {
      return kAsIs;
    } else {
      return kNone;
    }
  }

  ConvertOperation MatchShr(BinaryOperation* expr) {
    if (MatchIntBinaryOperation(expr, Token::SHR, 0)) {
      // TODO(titzer): this probably needs to be kToUint
      return (TypeOf(expr->left()) == kWasmI32) ? kAsIs : kToInt;
    } else {
      return kNone;
    }
  }

  ConvertOperation MatchXor(BinaryOperation* expr) {
    if (MatchIntBinaryOperation(expr, Token::BIT_XOR, 0xffffffff)) {
      DCHECK_EQ(kWasmI32, TypeOf(expr->left()));
      DCHECK_EQ(kWasmI32, TypeOf(expr->right()));
      BinaryOperation* op = expr->left()->AsBinaryOperation();
      if (op != nullptr) {
        if (MatchIntBinaryOperation(op, Token::BIT_XOR, 0xffffffff)) {
          DCHECK_EQ(kWasmI32, TypeOf(op->right()));
          if (TypeOf(op->left()) != kWasmI32) {
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
      DCHECK_EQ(kWasmF64, TypeOf(expr->right()));
      if (TypeOf(expr->left()) != kWasmF64) {
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
    static const bool kDontIgnoreSign = false;
    parent_binop_ = expr;
    if (convertOperation == kToDouble) {
      RECURSE(Visit(expr->left()));
      TypeIndex type = TypeIndexOf(expr->left(), kDontIgnoreSign);
      if (type == kInt32 || type == kFixnum) {
        current_function_builder_->Emit(kExprF64SConvertI32);
      } else if (type == kUint32) {
        current_function_builder_->Emit(kExprF64UConvertI32);
      } else if (type == kFloat32) {
        current_function_builder_->Emit(kExprF64ConvertF32);
      } else {
        UNREACHABLE();
      }
    } else if (convertOperation == kToInt) {
      RECURSE(Visit(GetLeft(expr)));
      TypeIndex type = TypeIndexOf(GetLeft(expr), kDontIgnoreSign);
      if (type == kFloat32) {
        current_function_builder_->Emit(kExprI32AsmjsSConvertF32);
      } else if (type == kFloat64) {
        current_function_builder_->Emit(kExprI32AsmjsSConvertF64);
      } else {
        UNREACHABLE();
      }
    } else if (convertOperation == kAsIs) {
      RECURSE(Visit(GetLeft(expr)));
    } else {
      if (expr->op() == Token::COMMA) {
        RECURSE(VisitForEffect(expr->left()));
        RECURSE(Visit(expr->right()));
        return;
      }
      RECURSE(Visit(expr->left()));
      RECURSE(Visit(expr->right()));

      switch (expr->op()) {
        BINOP_CASE(Token::ADD, Add, NON_SIGNED_BINOP, true);
        BINOP_CASE(Token::SUB, Sub, NON_SIGNED_BINOP, true);
        BINOP_CASE(Token::MUL, Mul, NON_SIGNED_BINOP, true);
        BINOP_CASE(Token::BIT_OR, Ior, NON_SIGNED_INT_BINOP, true);
        BINOP_CASE(Token::BIT_AND, And, NON_SIGNED_INT_BINOP, true);
        BINOP_CASE(Token::BIT_XOR, Xor, NON_SIGNED_INT_BINOP, true);
        BINOP_CASE(Token::SHL, Shl, NON_SIGNED_INT_BINOP, true);
        BINOP_CASE(Token::SAR, ShrS, NON_SIGNED_INT_BINOP, true);
        BINOP_CASE(Token::SHR, ShrU, NON_SIGNED_INT_BINOP, true);
        case Token::DIV: {
          static WasmOpcode opcodes[] = {kExprI32AsmjsDivS, kExprI32AsmjsDivU,
                                         kExprF32Div, kExprF64Div};
          int type = TypeIndexOf(expr->left(), expr->right(), false);
          current_function_builder_->Emit(opcodes[type]);
          break;
        }
        case Token::MOD: {
          TypeIndex type = TypeIndexOf(expr->left(), expr->right(), false);
          if (type == kInt32) {
            current_function_builder_->Emit(kExprI32AsmjsRemS);
          } else if (type == kUint32) {
            current_function_builder_->Emit(kExprI32AsmjsRemU);
          } else if (type == kFloat64) {
            current_function_builder_->Emit(kExprF64Mod);
            return;
          } else {
            UNREACHABLE();
          }
          break;
        }
        case Token::COMMA: {
          break;
        }
        default:
          UNREACHABLE();
      }
    }
  }

  void VisitCompareOperation(CompareOperation* expr) {
    RECURSE(Visit(expr->left()));
    RECURSE(Visit(expr->right()));
    switch (expr->op()) {
      BINOP_CASE(Token::EQ, Eq, NON_SIGNED_BINOP, false);
      BINOP_CASE(Token::LT, Lt, SIGNED_BINOP, false);
      BINOP_CASE(Token::LTE, Le, SIGNED_BINOP, false);
      BINOP_CASE(Token::GT, Gt, SIGNED_BINOP, false);
      BINOP_CASE(Token::GTE, Ge, SIGNED_BINOP, false);
      default:
        UNREACHABLE();
    }
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
    TypeIndex left_index = TypeIndexOf(left, ignore_sign);
    TypeIndex right_index = TypeIndexOf(right, ignore_sign);
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
    if (left_index != right_index) {
      DCHECK(ignore_sign && (left_index <= 1) && (right_index <= 1));
    }
    return left_index;
  }

  TypeIndex TypeIndexOf(Expression* expr, bool ignore_sign) {
    AsmType* type = typer_->TypeOf(expr);
    if (type->IsA(AsmType::FixNum())) {
      return kFixnum;
    }

    if (type->IsA(AsmType::Signed())) {
      return kInt32;
    }

    if (type->IsA(AsmType::Unsigned())) {
      return kUint32;
    }

    if (type->IsA(AsmType::Intish())) {
      if (!ignore_sign) {
        // TODO(jpp): log a warning and move on.
      }
      return kInt32;
    }

    if (type->IsA(AsmType::Floatish())) {
      return kFloat32;
    }

    if (type->IsA(AsmType::DoubleQ())) {
      return kFloat64;
    }

    UNREACHABLE();
    return kInt32;
  }

#undef CASE
#undef NON_SIGNED_INT
#undef SIGNED
#undef NON_SIGNED

  void VisitThisFunction(ThisFunction* expr) { UNREACHABLE(); }

  void VisitDeclarations(Declaration::List* decls) {
    for (Declaration* decl : *decls) {
      RECURSE(Visit(decl));
      if (typer_failed_) {
        return;
      }
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
    uint32_t index;
  };

  uint32_t LookupOrInsertLocal(Variable* v, ValueType type) {
    DCHECK_NOT_NULL(current_function_builder_);
    ZoneHashMap::Entry* entry =
        local_variables_.Lookup(v, ComputePointerHash(v));
    if (entry == nullptr) {
      uint32_t index;
      DCHECK(!v->IsParameter());
      index = current_function_builder_->AddLocal(type);
      IndexContainer* container = new (zone()) IndexContainer();
      container->index = index;
      entry = local_variables_.LookupOrInsert(v, ComputePointerHash(v),
                                              ZoneAllocationPolicy(zone()));
      entry->value = container;
    }
    return (reinterpret_cast<IndexContainer*>(entry->value))->index;
  }

  void InsertParameter(Variable* v, ValueType type, uint32_t index) {
    DCHECK(v->IsParameter());
    DCHECK_NOT_NULL(current_function_builder_);
    ZoneHashMap::Entry* entry =
        local_variables_.Lookup(v, ComputePointerHash(v));
    DCHECK_NULL(entry);
    IndexContainer* container = new (zone()) IndexContainer();
    container->index = index;
    entry = local_variables_.LookupOrInsert(v, ComputePointerHash(v),
                                            ZoneAllocationPolicy(zone()));
    entry->value = container;
  }

  uint32_t LookupOrInsertGlobal(Variable* v, ValueType type) {
    ZoneHashMap::Entry* entry =
        global_variables_.Lookup(v, ComputePointerHash(v));
    if (entry == nullptr) {
      uint32_t index = builder_->AddGlobal(type, 0);
      IndexContainer* container = new (zone()) IndexContainer();
      container->index = index;
      entry = global_variables_.LookupOrInsert(v, ComputePointerHash(v),
                                               ZoneAllocationPolicy(zone()));
      entry->value = container;
    }
    return (reinterpret_cast<IndexContainer*>(entry->value))->index;
  }

  WasmFunctionBuilder* LookupOrInsertFunction(Variable* v) {
    DCHECK_NOT_NULL(builder_);
    ZoneHashMap::Entry* entry = functions_.Lookup(v, ComputePointerHash(v));
    if (entry == nullptr) {
      auto* func_type = typer_->TypeOf(v)->AsFunctionType();
      DCHECK_NOT_NULL(func_type);
      // Build the signature for the function.
      ValueType return_type = TypeFrom(func_type->ReturnType());
      const auto& arguments = func_type->Arguments();
      FunctionSig::Builder b(zone(), return_type == kWasmStmt ? 0 : 1,
                             arguments.size());
      if (return_type != kWasmStmt) b.AddReturn(return_type);
      for (int i = 0; i < static_cast<int>(arguments.size()); ++i) {
        ValueType type = TypeFrom(arguments[i]);
        DCHECK_NE(kWasmStmt, type);
        b.AddParam(type);
      }

      WasmFunctionBuilder* function = builder_->AddFunction(b.Build());
      entry = functions_.LookupOrInsert(v, ComputePointerHash(v),
                                        ZoneAllocationPolicy(zone()));
      function->SetName(
          {reinterpret_cast<const char*>(v->raw_name()->raw_data()),
           v->raw_name()->length()});
      entry->value = function;
    }
    return (reinterpret_cast<WasmFunctionBuilder*>(entry->value));
  }

  ValueType TypeOf(Expression* expr) { return TypeFrom(typer_->TypeOf(expr)); }

  ValueType TypeFrom(AsmType* type) {
    if (type->IsA(AsmType::Intish())) {
      return kWasmI32;
    }

    if (type->IsA(AsmType::Floatish())) {
      return kWasmF32;
    }

    if (type->IsA(AsmType::DoubleQ())) {
      return kWasmF64;
    }

    return kWasmStmt;
  }

  Zone* zone() { return zone_; }

  ZoneHashMap local_variables_;
  ZoneHashMap functions_;
  ZoneHashMap global_variables_;
  AsmScope scope_;
  WasmModuleBuilder* builder_;
  WasmFunctionBuilder* current_function_builder_;
  FunctionLiteral* literal_;
  Isolate* isolate_;
  Zone* zone_;
  CompilationInfo* info_;
  AstValueFactory* ast_value_factory_;
  Handle<Script> script_;
  AsmTyper* typer_;
  bool typer_failed_;
  bool typer_finished_;
  ZoneVector<std::pair<BreakableStatement*, bool>> breakable_blocks_;
  ZoneVector<ForeignVariable> foreign_variables_;
  WasmFunctionBuilder* init_function_;
  WasmFunctionBuilder* foreign_init_function_;
  uint32_t next_table_index_;
  ZoneHashMap function_tables_;
  ImportedFunctionTable imported_function_table_;
  // Remember the parent node for reporting the correct location for ToNumber
  // conversions after calls.
  BinaryOperation* parent_binop_;

  DEFINE_AST_VISITOR_SUBCLASS_MEMBERS();

 private:
  DISALLOW_COPY_AND_ASSIGN(AsmWasmBuilderImpl);
};

AsmWasmBuilder::AsmWasmBuilder(CompilationInfo* info)
    : info_(info),
      typer_(info->isolate(), info->zone(), info->script(), info->literal()) {}

// TODO(aseemgarg): probably should take zone (to write wasm to) as input so
// that zone in constructor may be thrown away once wasm module is written.
AsmWasmBuilder::Result AsmWasmBuilder::Run(Handle<FixedArray>* foreign_args) {
  Zone* zone = info_->zone();
  AsmWasmBuilderImpl impl(info_->isolate(), zone, info_,
                          info_->parse_info()->ast_value_factory(),
                          info_->script(), info_->literal(), &typer_);
  bool success = impl.Build();
  *foreign_args = impl.GetForeignArgs();
  ZoneBuffer* module_buffer = new (zone) ZoneBuffer(zone);
  impl.builder_->WriteTo(*module_buffer);
  ZoneBuffer* asm_offsets_buffer = new (zone) ZoneBuffer(zone);
  impl.builder_->WriteAsmJsOffsetTable(*asm_offsets_buffer);
  return {module_buffer, asm_offsets_buffer, success};
}

const char* AsmWasmBuilder::foreign_init_name = "__foreign_init__";
const char* AsmWasmBuilder::single_function_name = "__single_function__";

}  // namespace wasm
}  // namespace internal
}  // namespace v8
