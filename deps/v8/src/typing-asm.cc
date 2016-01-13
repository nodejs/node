// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/typing-asm.h"

#include "src/ast.h"
#include "src/codegen.h"
#include "src/scopes.h"
#include "src/zone-type-cache.h"

namespace v8 {
namespace internal {
namespace {

base::LazyInstance<ZoneTypeCache>::type kCache = LAZY_INSTANCE_INITIALIZER;

}  // namespace


#define FAIL(node, msg)                                        \
  do {                                                         \
    valid_ = false;                                            \
    int line = node->position() == RelocInfo::kNoPosition      \
                   ? -1                                        \
                   : script_->GetLineNumber(node->position()); \
    base::OS::SNPrintF(error_message_, sizeof(error_message_), \
                       "asm: line %d: %s\n", line + 1, msg);   \
    return;                                                    \
  } while (false)


#define RECURSE(call)               \
  do {                              \
    DCHECK(!HasStackOverflow());    \
    call;                           \
    if (HasStackOverflow()) return; \
    if (!valid_) return;            \
  } while (false)


AsmTyper::AsmTyper(Isolate* isolate, Zone* zone, Script* script,
                   FunctionLiteral* root)
    : script_(script),
      root_(root),
      valid_(true),
      stdlib_types_(zone),
      stdlib_heap_types_(zone),
      stdlib_math_types_(zone),
      global_variable_type_(HashMap::PointersMatch,
                            ZoneHashMap::kDefaultHashMapCapacity,
                            ZoneAllocationPolicy(zone)),
      local_variable_type_(HashMap::PointersMatch,
                           ZoneHashMap::kDefaultHashMapCapacity,
                           ZoneAllocationPolicy(zone)),
      in_function_(false),
      building_function_tables_(false),
      cache_(kCache.Get()) {
  InitializeAstVisitor(isolate, zone);
  InitializeStdlib();
}


bool AsmTyper::Validate() {
  VisitAsmModule(root_);
  return valid_ && !HasStackOverflow();
}


void AsmTyper::VisitAsmModule(FunctionLiteral* fun) {
  Scope* scope = fun->scope();
  if (!scope->is_function_scope()) FAIL(fun, "not at function scope");

  // Module parameters.
  for (int i = 0; i < scope->num_parameters(); ++i) {
    Variable* param = scope->parameter(i);
    DCHECK(GetType(param) == NULL);
    SetType(param, Type::None(zone()));
  }

  ZoneList<Declaration*>* decls = scope->declarations();

  // Set all globals to type Any.
  VariableDeclaration* decl = scope->function();
  if (decl != NULL) SetType(decl->proxy()->var(), Type::None());
  RECURSE(VisitDeclarations(scope->declarations()));

  // Validate global variables.
  RECURSE(VisitStatements(fun->body()));

  // Validate function annotations.
  for (int i = 0; i < decls->length(); ++i) {
    FunctionDeclaration* decl = decls->at(i)->AsFunctionDeclaration();
    if (decl != NULL) {
      RECURSE(VisitFunctionAnnotation(decl->fun()));
      Variable* var = decl->proxy()->var();
      DCHECK(GetType(var) == NULL);
      SetType(var, computed_type_);
      DCHECK(GetType(var) != NULL);
    }
  }

  // Build function tables.
  building_function_tables_ = true;
  RECURSE(VisitStatements(fun->body()));
  building_function_tables_ = false;

  // Validate function bodies.
  for (int i = 0; i < decls->length(); ++i) {
    FunctionDeclaration* decl = decls->at(i)->AsFunctionDeclaration();
    if (decl != NULL) {
      RECURSE(
          VisitWithExpectation(decl->fun(), Type::Any(zone()), "UNREACHABLE"));
      if (!computed_type_->IsFunction()) {
        FAIL(decl->fun(), "function literal expected to be a function");
      }
    }
  }

  // Validate exports.
  ReturnStatement* stmt = fun->body()->last()->AsReturnStatement();
  RECURSE(VisitWithExpectation(stmt->expression(), Type::Object(),
                               "expected object export"));
}


void AsmTyper::VisitVariableDeclaration(VariableDeclaration* decl) {
  Variable* var = decl->proxy()->var();
  if (var->location() != VariableLocation::PARAMETER) {
    if (GetType(var) == NULL) {
      SetType(var, Type::Any(zone()));
    } else {
      DCHECK(!GetType(var)->IsFunction());
    }
  }
  DCHECK(GetType(var) != NULL);
  intish_ = 0;
}


void AsmTyper::VisitFunctionDeclaration(FunctionDeclaration* decl) {
  if (in_function_) {
    FAIL(decl, "function declared inside another");
  }
}


void AsmTyper::VisitFunctionAnnotation(FunctionLiteral* fun) {
  // Extract result type.
  ZoneList<Statement*>* body = fun->body();
  Type* result_type = Type::Undefined(zone());
  if (body->length() > 0) {
    ReturnStatement* stmt = body->last()->AsReturnStatement();
    if (stmt != NULL) {
      RECURSE(VisitExpressionAnnotation(stmt->expression()));
      result_type = computed_type_;
    }
  }
  Type::FunctionType* type =
      Type::Function(result_type, Type::Any(), fun->parameter_count(), zone())
          ->AsFunction();

  // Extract parameter types.
  bool good = true;
  for (int i = 0; i < fun->parameter_count(); ++i) {
    good = false;
    if (i >= body->length()) break;
    ExpressionStatement* stmt = body->at(i)->AsExpressionStatement();
    if (stmt == NULL) break;
    Assignment* expr = stmt->expression()->AsAssignment();
    if (expr == NULL || expr->is_compound()) break;
    VariableProxy* proxy = expr->target()->AsVariableProxy();
    if (proxy == NULL) break;
    Variable* var = proxy->var();
    if (var->location() != VariableLocation::PARAMETER || var->index() != i)
      break;
    RECURSE(VisitExpressionAnnotation(expr->value()));
    SetType(var, computed_type_);
    type->InitParameter(i, computed_type_);
    good = true;
  }
  if (!good) FAIL(fun, "missing parameter type annotations");

  SetResult(fun, type);
}


void AsmTyper::VisitExpressionAnnotation(Expression* expr) {
  // Normal +x or x|0 annotations.
  BinaryOperation* bin = expr->AsBinaryOperation();
  if (bin != NULL) {
    Literal* right = bin->right()->AsLiteral();
    if (right != NULL) {
      switch (bin->op()) {
        case Token::MUL:  // We encode +x as 1*x
          if (right->raw_value()->ContainsDot() &&
              right->raw_value()->AsNumber() == 1.0) {
            SetResult(expr, cache_.kFloat64);
            return;
          }
          break;
        case Token::BIT_OR:
          if (!right->raw_value()->ContainsDot() &&
              right->raw_value()->AsNumber() == 0.0) {
            SetResult(expr, cache_.kInt32);
            return;
          }
          break;
        default:
          break;
      }
    }
    FAIL(expr, "invalid type annotation on binary op");
  }

  // Numbers or the undefined literal (for empty returns).
  if (expr->IsLiteral()) {
    RECURSE(VisitWithExpectation(expr, Type::Any(), "invalid literal"));
    return;
  }

  Call* call = expr->AsCall();
  if (call != NULL) {
    if (call->expression()->IsVariableProxy()) {
      RECURSE(VisitWithExpectation(
          call->expression(), Type::Any(zone()),
          "only fround allowed on expression annotations"));
      if (!computed_type_->Is(
              Type::Function(cache_.kFloat32, Type::Number(zone()), zone()))) {
        FAIL(call->expression(),
             "only fround allowed on expression annotations");
      }
      if (call->arguments()->length() != 1) {
        FAIL(call, "invalid argument count calling fround");
      }
      SetResult(expr, cache_.kFloat32);
      return;
    }
  }

  FAIL(expr, "invalid type annotation");
}


void AsmTyper::VisitStatements(ZoneList<Statement*>* stmts) {
  for (int i = 0; i < stmts->length(); ++i) {
    Statement* stmt = stmts->at(i);
    RECURSE(Visit(stmt));
  }
}


void AsmTyper::VisitBlock(Block* stmt) {
  RECURSE(VisitStatements(stmt->statements()));
}


void AsmTyper::VisitExpressionStatement(ExpressionStatement* stmt) {
  RECURSE(VisitWithExpectation(stmt->expression(), Type::Any(),
                               "expression statement expected to be any"));
}


void AsmTyper::VisitEmptyStatement(EmptyStatement* stmt) {}


void AsmTyper::VisitSloppyBlockFunctionStatement(
    SloppyBlockFunctionStatement* stmt) {
  Visit(stmt->statement());
}


void AsmTyper::VisitEmptyParentheses(EmptyParentheses* expr) { UNREACHABLE(); }


void AsmTyper::VisitIfStatement(IfStatement* stmt) {
  if (!in_function_) {
    FAIL(stmt, "if statement inside module body");
  }
  RECURSE(VisitWithExpectation(stmt->condition(), cache_.kInt32,
                               "if condition expected to be integer"));
  RECURSE(Visit(stmt->then_statement()));
  RECURSE(Visit(stmt->else_statement()));
}


void AsmTyper::VisitContinueStatement(ContinueStatement* stmt) {
  if (!in_function_) {
    FAIL(stmt, "continue statement inside module body");
  }
}


void AsmTyper::VisitBreakStatement(BreakStatement* stmt) {
  if (!in_function_) {
    FAIL(stmt, "continue statement inside module body");
  }
}


void AsmTyper::VisitReturnStatement(ReturnStatement* stmt) {
  // Handle module return statement in VisitAsmModule.
  if (!in_function_) {
    return;
  }
  RECURSE(
      VisitWithExpectation(stmt->expression(), return_type_,
                           "return expression expected to have return type"));
}


void AsmTyper::VisitWithStatement(WithStatement* stmt) {
  FAIL(stmt, "bad with statement");
}


void AsmTyper::VisitSwitchStatement(SwitchStatement* stmt) {
  if (!in_function_) {
    FAIL(stmt, "switch statement inside module body");
  }
  RECURSE(VisitWithExpectation(stmt->tag(), cache_.kInt32,
                               "switch expression non-integer"));
  ZoneList<CaseClause*>* clauses = stmt->cases();
  for (int i = 0; i < clauses->length(); ++i) {
    CaseClause* clause = clauses->at(i);
    if (clause->is_default()) continue;
    Expression* label = clause->label();
    RECURSE(
        VisitWithExpectation(label, cache_.kInt32, "case label non-integer"));
    if (!label->IsLiteral()) FAIL(label, "non-literal case label");
    Handle<Object> value = label->AsLiteral()->value();
    int32_t value32;
    if (!value->ToInt32(&value32)) FAIL(label, "illegal case label value");
    // TODO(bradnelson): Detect duplicates.
    ZoneList<Statement*>* stmts = clause->statements();
    RECURSE(VisitStatements(stmts));
  }
}


void AsmTyper::VisitCaseClause(CaseClause* clause) { UNREACHABLE(); }


void AsmTyper::VisitDoWhileStatement(DoWhileStatement* stmt) {
  if (!in_function_) {
    FAIL(stmt, "do statement inside module body");
  }
  RECURSE(Visit(stmt->body()));
  RECURSE(VisitWithExpectation(stmt->cond(), cache_.kInt32,
                               "do condition expected to be integer"));
}


void AsmTyper::VisitWhileStatement(WhileStatement* stmt) {
  if (!in_function_) {
    FAIL(stmt, "while statement inside module body");
  }
  RECURSE(VisitWithExpectation(stmt->cond(), cache_.kInt32,
                               "while condition expected to be integer"));
  RECURSE(Visit(stmt->body()));
}


void AsmTyper::VisitForStatement(ForStatement* stmt) {
  if (!in_function_) {
    FAIL(stmt, "for statement inside module body");
  }
  if (stmt->init() != NULL) {
    RECURSE(Visit(stmt->init()));
  }
  if (stmt->cond() != NULL) {
    RECURSE(VisitWithExpectation(stmt->cond(), cache_.kInt32,
                                 "for condition expected to be integer"));
  }
  if (stmt->next() != NULL) {
    RECURSE(Visit(stmt->next()));
  }
  RECURSE(Visit(stmt->body()));
}


void AsmTyper::VisitForInStatement(ForInStatement* stmt) {
  FAIL(stmt, "for-in statement encountered");
}


void AsmTyper::VisitForOfStatement(ForOfStatement* stmt) {
  FAIL(stmt, "for-of statement encountered");
}


void AsmTyper::VisitTryCatchStatement(TryCatchStatement* stmt) {
  FAIL(stmt, "try statement encountered");
}


void AsmTyper::VisitTryFinallyStatement(TryFinallyStatement* stmt) {
  FAIL(stmt, "try statement encountered");
}


void AsmTyper::VisitDebuggerStatement(DebuggerStatement* stmt) {
  FAIL(stmt, "debugger statement encountered");
}


void AsmTyper::VisitFunctionLiteral(FunctionLiteral* expr) {
  Scope* scope = expr->scope();
  DCHECK(scope->is_function_scope());
  if (in_function_) {
    FAIL(expr, "invalid nested function");
  }

  if (!expr->bounds().upper->IsFunction()) {
    FAIL(expr, "invalid function literal");
  }

  Type::FunctionType* type = expr->bounds().upper->AsFunction();
  Type* save_return_type = return_type_;
  return_type_ = type->Result();
  in_function_ = true;
  local_variable_type_.Clear();
  RECURSE(VisitDeclarations(scope->declarations()));
  RECURSE(VisitStatements(expr->body()));
  in_function_ = false;
  return_type_ = save_return_type;
  IntersectResult(expr, type);
}


void AsmTyper::VisitNativeFunctionLiteral(NativeFunctionLiteral* expr) {
  FAIL(expr, "function info literal encountered");
}


void AsmTyper::VisitConditional(Conditional* expr) {
  RECURSE(VisitWithExpectation(expr->condition(), cache_.kInt32,
                               "condition expected to be integer"));
  RECURSE(VisitWithExpectation(
      expr->then_expression(), expected_type_,
      "conditional then branch type mismatch with enclosing expression"));
  Type* then_type = computed_type_;
  RECURSE(VisitWithExpectation(
      expr->else_expression(), expected_type_,
      "conditional else branch type mismatch with enclosing expression"));
  Type* else_type = computed_type_;
  Type* type = Type::Intersect(then_type, else_type, zone());
  if (!(type->Is(cache_.kInt32) || type->Is(cache_.kFloat64))) {
    FAIL(expr, "ill-typed conditional");
  }
  IntersectResult(expr, type);
}


void AsmTyper::VisitVariableProxy(VariableProxy* expr) {
  Variable* var = expr->var();
  if (GetType(var) == NULL) {
    FAIL(expr, "unbound variable");
  }
  Type* type = Type::Intersect(GetType(var), expected_type_, zone());
  if (type->Is(cache_.kInt32)) {
    type = cache_.kInt32;
  }
  SetType(var, type);
  intish_ = 0;
  IntersectResult(expr, type);
}


void AsmTyper::VisitLiteral(Literal* expr) {
  intish_ = 0;
  Handle<Object> value = expr->value();
  if (value->IsNumber()) {
    int32_t i;
    uint32_t u;
    if (expr->raw_value()->ContainsDot()) {
      IntersectResult(expr, cache_.kFloat64);
    } else if (value->ToUint32(&u)) {
      IntersectResult(expr, cache_.kInt32);
    } else if (value->ToInt32(&i)) {
      IntersectResult(expr, cache_.kInt32);
    } else {
      FAIL(expr, "illegal number");
    }
  } else if (value->IsString()) {
    IntersectResult(expr, Type::String());
  } else if (value->IsUndefined()) {
    IntersectResult(expr, Type::Undefined());
  } else {
    FAIL(expr, "illegal literal");
  }
}


void AsmTyper::VisitRegExpLiteral(RegExpLiteral* expr) {
  FAIL(expr, "regular expression encountered");
}


void AsmTyper::VisitObjectLiteral(ObjectLiteral* expr) {
  if (in_function_) {
    FAIL(expr, "object literal in function");
  }
  // Allowed for asm module's export declaration.
  ZoneList<ObjectLiteralProperty*>* props = expr->properties();
  for (int i = 0; i < props->length(); ++i) {
    ObjectLiteralProperty* prop = props->at(i);
    RECURSE(VisitWithExpectation(prop->value(), Type::Any(zone()),
                                 "object property expected to be a function"));
    if (!computed_type_->IsFunction()) {
      FAIL(prop->value(), "non-function in function table");
    }
  }
  IntersectResult(expr, Type::Object(zone()));
}


void AsmTyper::VisitArrayLiteral(ArrayLiteral* expr) {
  if (in_function_) {
    FAIL(expr, "array literal inside a function");
  }
  // Allowed for function tables.
  ZoneList<Expression*>* values = expr->values();
  Type* elem_type = Type::None(zone());
  for (int i = 0; i < values->length(); ++i) {
    Expression* value = values->at(i);
    RECURSE(VisitWithExpectation(value, Type::Any(), "UNREACHABLE"));
    if (!computed_type_->IsFunction()) {
      FAIL(value, "array component expected to be a function");
    }
    elem_type = Type::Union(elem_type, computed_type_, zone());
  }
  array_size_ = values->length();
  IntersectResult(expr, Type::Array(elem_type, zone()));
}


void AsmTyper::VisitAssignment(Assignment* expr) {
  // Handle function tables and everything else in different passes.
  if (!in_function_) {
    if (expr->value()->IsArrayLiteral()) {
      if (!building_function_tables_) {
        return;
      }
    } else {
      if (building_function_tables_) {
        return;
      }
    }
  }
  if (expr->is_compound()) FAIL(expr, "compound assignment encountered");
  Type* type = expected_type_;
  RECURSE(VisitWithExpectation(
      expr->value(), type, "assignment value expected to match surrounding"));
  if (intish_ != 0) {
    FAIL(expr, "value still an intish");
  }
  RECURSE(VisitWithExpectation(expr->target(), computed_type_,
                               "assignment target expected to match value"));
  if (intish_ != 0) {
    FAIL(expr, "value still an intish");
  }
  IntersectResult(expr, computed_type_);
}


void AsmTyper::VisitYield(Yield* expr) {
  FAIL(expr, "yield expression encountered");
}


void AsmTyper::VisitThrow(Throw* expr) {
  FAIL(expr, "throw statement encountered");
}


int AsmTyper::ElementShiftSize(Type* type) {
  if (type->Is(cache_.kInt8) || type->Is(cache_.kUint8)) return 0;
  if (type->Is(cache_.kInt16) || type->Is(cache_.kUint16)) return 1;
  if (type->Is(cache_.kInt32) || type->Is(cache_.kUint32) ||
      type->Is(cache_.kFloat32))
    return 2;
  if (type->Is(cache_.kFloat64)) return 3;
  return -1;
}


void AsmTyper::VisitHeapAccess(Property* expr) {
  Type::ArrayType* array_type = computed_type_->AsArray();
  size_t size = array_size_;
  Type* type = array_type->AsArray()->Element();
  if (type->IsFunction()) {
    BinaryOperation* bin = expr->key()->AsBinaryOperation();
    if (bin == NULL || bin->op() != Token::BIT_AND) {
      FAIL(expr->key(), "expected & in call");
    }
    RECURSE(VisitWithExpectation(bin->left(), cache_.kInt32,
                                 "array index expected to be integer"));
    Literal* right = bin->right()->AsLiteral();
    if (right == NULL || right->raw_value()->ContainsDot()) {
      FAIL(right, "call mask must be integer");
    }
    RECURSE(VisitWithExpectation(bin->right(), cache_.kInt32,
                                 "call mask expected to be integer"));
    if (static_cast<size_t>(right->raw_value()->AsNumber()) != size - 1) {
      FAIL(right, "call mask must match function table");
    }
    bin->set_bounds(Bounds(cache_.kInt32));
  } else {
    BinaryOperation* bin = expr->key()->AsBinaryOperation();
    if (bin == NULL || bin->op() != Token::SAR) {
      FAIL(expr->key(), "expected >> in heap access");
    }
    RECURSE(VisitWithExpectation(bin->left(), cache_.kInt32,
                                 "array index expected to be integer"));
    Literal* right = bin->right()->AsLiteral();
    if (right == NULL || right->raw_value()->ContainsDot()) {
      FAIL(right, "heap access shift must be integer");
    }
    RECURSE(VisitWithExpectation(bin->right(), cache_.kInt32,
                                 "array shift expected to be integer"));
    int n = static_cast<int>(right->raw_value()->AsNumber());
    int expected_shift = ElementShiftSize(type);
    if (expected_shift < 0 || n != expected_shift) {
      FAIL(right, "heap access shift must match element size");
    }
    bin->set_bounds(Bounds(cache_.kInt32));
  }
  IntersectResult(expr, type);
}


void AsmTyper::VisitProperty(Property* expr) {
  // stdlib.Math.x
  Property* inner_prop = expr->obj()->AsProperty();
  if (inner_prop != NULL) {
    // Get property name.
    Literal* key = expr->key()->AsLiteral();
    if (key == NULL || !key->IsPropertyName())
      FAIL(expr, "invalid type annotation on property 2");
    Handle<String> name = key->AsPropertyName();

    // Check that inner property name is "Math".
    Literal* math_key = inner_prop->key()->AsLiteral();
    if (math_key == NULL || !math_key->IsPropertyName() ||
        !math_key->AsPropertyName()->IsUtf8EqualTo(CStrVector("Math")))
      FAIL(expr, "invalid type annotation on stdlib (a1)");

    // Check that object is stdlib.
    VariableProxy* proxy = inner_prop->obj()->AsVariableProxy();
    if (proxy == NULL) FAIL(expr, "invalid type annotation on stdlib (a2)");
    Variable* var = proxy->var();
    if (var->location() != VariableLocation::PARAMETER || var->index() != 0)
      FAIL(expr, "invalid type annotation on stdlib (a3)");

    // Look up library type.
    Type* type = LibType(stdlib_math_types_, name);
    if (type == NULL) FAIL(expr, "unknown standard function 3 ");
    SetResult(expr, type);
    return;
  }

  // Only recurse at this point so that we avoid needing
  // stdlib.Math to have a real type.
  RECURSE(VisitWithExpectation(expr->obj(), Type::Any(),
                               "property holder expected to be object"));

  // For heap view or function table access.
  if (computed_type_->IsArray()) {
    VisitHeapAccess(expr);
    return;
  }

  // Get property name.
  Literal* key = expr->key()->AsLiteral();
  if (key == NULL || !key->IsPropertyName())
    FAIL(expr, "invalid type annotation on property 3");
  Handle<String> name = key->AsPropertyName();

  // stdlib.x or foreign.x
  VariableProxy* proxy = expr->obj()->AsVariableProxy();
  if (proxy != NULL) {
    Variable* var = proxy->var();
    if (var->location() != VariableLocation::PARAMETER) {
      FAIL(expr, "invalid type annotation on variable");
    }
    switch (var->index()) {
      case 0: {
        // Object is stdlib, look up library type.
        Type* type = LibType(stdlib_types_, name);
        if (type == NULL) {
          FAIL(expr, "unknown standard function 4");
        }
        SetResult(expr, type);
        return;
      }
      case 1:
        // Object is foreign lib.
        SetResult(expr, expected_type_);
        return;
      default:
        FAIL(expr, "invalid type annotation on parameter");
    }
  }

  FAIL(expr, "invalid property access");
}


void AsmTyper::VisitCall(Call* expr) {
  RECURSE(VisitWithExpectation(expr->expression(), Type::Any(),
                               "callee expected to be any"));
  if (computed_type_->IsFunction()) {
    Type::FunctionType* fun_type = computed_type_->AsFunction();
    ZoneList<Expression*>* args = expr->arguments();
    if (fun_type->Arity() != args->length()) {
      FAIL(expr, "call with wrong arity");
    }
    for (int i = 0; i < args->length(); ++i) {
      Expression* arg = args->at(i);
      RECURSE(VisitWithExpectation(
          arg, fun_type->Parameter(i),
          "call argument expected to match callee parameter"));
    }
    IntersectResult(expr, fun_type->Result());
  } else if (computed_type_->Is(Type::Any())) {
    // For foreign calls.
    ZoneList<Expression*>* args = expr->arguments();
    for (int i = 0; i < args->length(); ++i) {
      Expression* arg = args->at(i);
      RECURSE(VisitWithExpectation(arg, Type::Any(),
                                   "foreign call argument expected to be any"));
    }
    IntersectResult(expr, Type::Number());
  } else {
    FAIL(expr, "invalid callee");
  }
}


void AsmTyper::VisitCallNew(CallNew* expr) {
  if (in_function_) {
    FAIL(expr, "new not allowed in module function");
  }
  RECURSE(VisitWithExpectation(expr->expression(), Type::Any(),
                               "expected stdlib function"));
  if (computed_type_->IsFunction()) {
    Type::FunctionType* fun_type = computed_type_->AsFunction();
    ZoneList<Expression*>* args = expr->arguments();
    if (fun_type->Arity() != args->length())
      FAIL(expr, "call with wrong arity");
    for (int i = 0; i < args->length(); ++i) {
      Expression* arg = args->at(i);
      RECURSE(VisitWithExpectation(
          arg, fun_type->Parameter(i),
          "constructor argument expected to match callee parameter"));
    }
    IntersectResult(expr, fun_type->Result());
    return;
  }

  FAIL(expr, "ill-typed new operator");
}


void AsmTyper::VisitCallRuntime(CallRuntime* expr) {
  // Allow runtime calls for now.
}


void AsmTyper::VisitUnaryOperation(UnaryOperation* expr) {
  switch (expr->op()) {
    case Token::NOT:  // Used to encode != and !==
      RECURSE(VisitWithExpectation(expr->expression(), cache_.kInt32,
                                   "operand expected to be integer"));
      IntersectResult(expr, cache_.kInt32);
      return;
    case Token::DELETE:
      FAIL(expr, "delete operator encountered");
    case Token::VOID:
      FAIL(expr, "void operator encountered");
    case Token::TYPEOF:
      FAIL(expr, "typeof operator encountered");
    default:
      UNREACHABLE();
  }
}


void AsmTyper::VisitCountOperation(CountOperation* expr) {
  FAIL(expr, "increment or decrement operator encountered");
}


void AsmTyper::VisitBinaryOperation(BinaryOperation* expr) {
  switch (expr->op()) {
    case Token::COMMA: {
      RECURSE(VisitWithExpectation(expr->left(), Type::Any(),
                                   "left comma operand expected to be any"));
      RECURSE(VisitWithExpectation(expr->right(), Type::Any(),
                                   "right comma operand expected to be any"));
      IntersectResult(expr, computed_type_);
      return;
    }
    case Token::OR:
    case Token::AND:
      FAIL(expr, "logical operator encountered");
    case Token::BIT_OR:
    case Token::BIT_AND:
    case Token::BIT_XOR:
    case Token::SHL:
    case Token::SHR:
    case Token::SAR: {
      // BIT_OR allows Any since it is used as a type coercion.
      // BIT_XOR allows Number since it is used as a type coercion (encoding ~).
      Type* expectation =
          expr->op() == Token::BIT_OR
              ? Type::Any()
              : expr->op() == Token::BIT_XOR ? Type::Number() : cache_.kInt32;
      Type* result =
          expr->op() == Token::SHR ? Type::Unsigned32() : cache_.kInt32;
      RECURSE(VisitWithExpectation(expr->left(), expectation,
                                   "left bit operand expected to be integer"));
      int left_intish = intish_;
      RECURSE(VisitWithExpectation(expr->right(), expectation,
                                   "right bit operand expected to be integer"));
      int right_intish = intish_;
      if (left_intish > kMaxUncombinedAdditiveSteps) {
        FAIL(expr, "too many consecutive additive ops");
      }
      if (right_intish > kMaxUncombinedAdditiveSteps) {
        FAIL(expr, "too many consecutive additive ops");
      }
      intish_ = 0;
      IntersectResult(expr, result);
      return;
    }
    case Token::ADD:
    case Token::SUB:
    case Token::MUL:
    case Token::DIV:
    case Token::MOD: {
      RECURSE(VisitWithExpectation(
          expr->left(), Type::Number(),
          "left arithmetic operand expected to be number"));
      Type* left_type = computed_type_;
      int left_intish = intish_;
      RECURSE(VisitWithExpectation(
          expr->right(), Type::Number(),
          "right arithmetic operand expected to be number"));
      Type* right_type = computed_type_;
      int right_intish = intish_;
      Type* type = Type::Union(left_type, right_type, zone());
      if (type->Is(cache_.kInt32)) {
        if (expr->op() == Token::MUL) {
          if (!expr->left()->IsLiteral() && !expr->right()->IsLiteral()) {
            FAIL(expr, "direct integer multiply forbidden");
          }
          intish_ = 0;
          IntersectResult(expr, cache_.kInt32);
          return;
        } else {
          intish_ = left_intish + right_intish + 1;
          if (expr->op() == Token::ADD || expr->op() == Token::SUB) {
            if (intish_ > kMaxUncombinedAdditiveSteps) {
              FAIL(expr, "too many consecutive additive ops");
            }
          } else {
            if (intish_ > kMaxUncombinedMultiplicativeSteps) {
              FAIL(expr, "too many consecutive multiplicative ops");
            }
          }
          IntersectResult(expr, cache_.kInt32);
          return;
        }
      } else if (type->Is(Type::Number())) {
        IntersectResult(expr, cache_.kFloat64);
        return;
      } else {
        FAIL(expr, "ill-typed arithmetic operation");
      }
    }
    default:
      UNREACHABLE();
  }
}


void AsmTyper::VisitCompareOperation(CompareOperation* expr) {
  RECURSE(
      VisitWithExpectation(expr->left(), Type::Number(),
                           "left comparison operand expected to be number"));
  Type* left_type = computed_type_;
  RECURSE(
      VisitWithExpectation(expr->right(), Type::Number(),
                           "right comparison operand expected to be number"));
  Type* right_type = computed_type_;
  Type* type = Type::Union(left_type, right_type, zone());
  expr->set_combined_type(type);
  if (type->Is(Type::Integral32()) || type->Is(Type::UntaggedFloat64())) {
    IntersectResult(expr, cache_.kInt32);
  } else {
    FAIL(expr, "ill-typed comparison operation");
  }
}


void AsmTyper::VisitThisFunction(ThisFunction* expr) {
  FAIL(expr, "this function not allowed");
}


void AsmTyper::VisitDeclarations(ZoneList<Declaration*>* decls) {
  for (int i = 0; i < decls->length(); ++i) {
    Declaration* decl = decls->at(i);
    RECURSE(Visit(decl));
  }
}


void AsmTyper::VisitImportDeclaration(ImportDeclaration* decl) {
  FAIL(decl, "import declaration encountered");
}


void AsmTyper::VisitExportDeclaration(ExportDeclaration* decl) {
  FAIL(decl, "export declaration encountered");
}


void AsmTyper::VisitClassLiteral(ClassLiteral* expr) {
  FAIL(expr, "class literal not allowed");
}


void AsmTyper::VisitSpread(Spread* expr) { FAIL(expr, "spread not allowed"); }


void AsmTyper::VisitSuperPropertyReference(SuperPropertyReference* expr) {
  FAIL(expr, "super property reference not allowed");
}


void AsmTyper::VisitSuperCallReference(SuperCallReference* expr) {
  FAIL(expr, "call reference not allowed");
}


void AsmTyper::InitializeStdlib() {
  Type* number_type = Type::Number(zone());
  Type* double_type = cache_.kFloat64;
  Type* double_fn1_type = Type::Function(double_type, double_type, zone());
  Type* double_fn2_type =
      Type::Function(double_type, double_type, double_type, zone());

  Type* fround_type = Type::Function(cache_.kFloat32, number_type, zone());
  Type* imul_type =
      Type::Function(cache_.kInt32, cache_.kInt32, cache_.kInt32, zone());
  // TODO(bradnelson): currently only approximating the proper intersection type
  // (which we cannot currently represent).
  Type* abs_type = Type::Function(number_type, number_type, zone());

  struct Assignment {
    const char* name;
    Type* type;
  };

  const Assignment math[] = {
      {"PI", double_type},       {"E", double_type},
      {"LN2", double_type},      {"LN10", double_type},
      {"LOG2E", double_type},    {"LOG10E", double_type},
      {"SQRT2", double_type},    {"SQRT1_2", double_type},
      {"imul", imul_type},       {"abs", abs_type},
      {"ceil", double_fn1_type}, {"floor", double_fn1_type},
      {"fround", fround_type},   {"pow", double_fn2_type},
      {"exp", double_fn1_type},  {"log", double_fn1_type},
      {"min", double_fn2_type},  {"max", double_fn2_type},
      {"sqrt", double_fn1_type}, {"cos", double_fn1_type},
      {"sin", double_fn1_type},  {"tan", double_fn1_type},
      {"acos", double_fn1_type}, {"asin", double_fn1_type},
      {"atan", double_fn1_type}, {"atan2", double_fn2_type}};
  for (unsigned i = 0; i < arraysize(math); ++i) {
    stdlib_math_types_[math[i].name] = math[i].type;
  }

  stdlib_types_["Infinity"] = double_type;
  stdlib_types_["NaN"] = double_type;
  Type* buffer_type = Type::Any(zone());
#define TYPED_ARRAY(TypeName, type_name, TYPE_NAME, ctype, size) \
  stdlib_types_[#TypeName "Array"] =                             \
      Type::Function(cache_.k##TypeName##Array, buffer_type, zone());
  TYPED_ARRAYS(TYPED_ARRAY)
#undef TYPED_ARRAY

#define TYPED_ARRAY(TypeName, type_name, TYPE_NAME, ctype, size) \
  stdlib_heap_types_[#TypeName "Array"] =                        \
      Type::Function(cache_.k##TypeName##Array, buffer_type, zone());
  TYPED_ARRAYS(TYPED_ARRAY)
#undef TYPED_ARRAY
}


Type* AsmTyper::LibType(ObjectTypeMap map, Handle<String> name) {
  base::SmartArrayPointer<char> aname = name->ToCString();
  ObjectTypeMap::iterator i = map.find(std::string(aname.get()));
  if (i == map.end()) {
    return NULL;
  }
  return i->second;
}


void AsmTyper::SetType(Variable* variable, Type* type) {
  ZoneHashMap::Entry* entry;
  if (in_function_) {
    entry = local_variable_type_.LookupOrInsert(
        variable, ComputePointerHash(variable), ZoneAllocationPolicy(zone()));
  } else {
    entry = global_variable_type_.LookupOrInsert(
        variable, ComputePointerHash(variable), ZoneAllocationPolicy(zone()));
  }
  entry->value = reinterpret_cast<void*>(type);
}


Type* AsmTyper::GetType(Variable* variable) {
  i::ZoneHashMap::Entry* entry = NULL;
  if (in_function_) {
    entry = local_variable_type_.Lookup(variable, ComputePointerHash(variable));
  }
  if (entry == NULL) {
    entry =
        global_variable_type_.Lookup(variable, ComputePointerHash(variable));
  }
  if (entry == NULL) {
    return NULL;
  } else {
    return reinterpret_cast<Type*>(entry->value);
  }
}


void AsmTyper::SetResult(Expression* expr, Type* type) {
  computed_type_ = type;
  expr->set_bounds(Bounds(computed_type_));
}


void AsmTyper::IntersectResult(Expression* expr, Type* type) {
  computed_type_ = type;
  Type* bounded_type = Type::Intersect(computed_type_, expected_type_, zone());
  expr->set_bounds(Bounds(bounded_type));
}


void AsmTyper::VisitWithExpectation(Expression* expr, Type* expected_type,
                                    const char* msg) {
  Type* save = expected_type_;
  expected_type_ = expected_type;
  RECURSE(Visit(expr));
  Type* bounded_type = Type::Intersect(computed_type_, expected_type_, zone());
  if (bounded_type->Is(Type::None(zone()))) {
#ifdef DEBUG
    PrintF("Computed type: ");
    computed_type_->Print();
    PrintF("Expected type: ");
    expected_type_->Print();
#endif
    FAIL(expr, msg);
  }
  expected_type_ = save;
}
}
}  // namespace v8::internal
