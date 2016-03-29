// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/typing-asm.h"

#include <limits>

#include "src/v8.h"

#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/codegen.h"
#include "src/type-cache.h"

namespace v8 {
namespace internal {

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
    : zone_(zone),
      isolate_(isolate),
      script_(script),
      root_(root),
      valid_(true),
      allow_simd_(false),
      property_info_(NULL),
      intish_(0),
      stdlib_types_(zone),
      stdlib_heap_types_(zone),
      stdlib_math_types_(zone),
#define V(NAME, Name, name, lane_count, lane_type) \
  stdlib_simd_##name##_types_(zone),
      SIMD128_TYPES(V)
#undef V
          global_variable_type_(HashMap::PointersMatch,
                                ZoneHashMap::kDefaultHashMapCapacity,
                                ZoneAllocationPolicy(zone)),
      local_variable_type_(HashMap::PointersMatch,
                           ZoneHashMap::kDefaultHashMapCapacity,
                           ZoneAllocationPolicy(zone)),
      in_function_(false),
      building_function_tables_(false),
      visiting_exports_(false),
      cache_(TypeCache::Get()) {
  InitializeAstVisitor(isolate);
  InitializeStdlib();
}


bool AsmTyper::Validate() {
  VisitAsmModule(root_);
  return valid_ && !HasStackOverflow();
}


void AsmTyper::VisitAsmModule(FunctionLiteral* fun) {
  Scope* scope = fun->scope();
  if (!scope->is_function_scope()) FAIL(fun, "not at function scope");

  ExpressionStatement* use_asm = fun->body()->first()->AsExpressionStatement();
  if (use_asm == NULL) FAIL(fun, "missing \"use asm\"");
  Literal* use_asm_literal = use_asm->expression()->AsLiteral();
  if (use_asm_literal == NULL) FAIL(fun, "missing \"use asm\"");
  if (!use_asm_literal->raw_value()->AsString()->IsOneByteEqualTo("use asm"))
    FAIL(fun, "missing \"use asm\"");

  // Module parameters.
  for (int i = 0; i < scope->num_parameters(); ++i) {
    Variable* param = scope->parameter(i);
    DCHECK(GetType(param) == NULL);
    SetType(param, Type::None());
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
      if (property_info_ != NULL) {
        SetVariableInfo(var, property_info_);
        property_info_ = NULL;
      }
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
      RECURSE(VisitWithExpectation(decl->fun(), Type::Any(), "UNREACHABLE"));
      if (!computed_type_->IsFunction()) {
        FAIL(decl->fun(), "function literal expected to be a function");
      }
    }
  }

  // Validate exports.
  visiting_exports_ = true;
  ReturnStatement* stmt = fun->body()->last()->AsReturnStatement();
  if (stmt == nullptr) {
    FAIL(fun->body()->last(), "last statement in module is not a return");
  }
  RECURSE(VisitWithExpectation(stmt->expression(), Type::Object(),
                               "expected object export"));
}


void AsmTyper::VisitVariableDeclaration(VariableDeclaration* decl) {
  Variable* var = decl->proxy()->var();
  if (var->location() != VariableLocation::PARAMETER) {
    if (GetType(var) == NULL) {
      SetType(var, Type::Any());
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
  // Set function type so global references to functions have some type
  // (so they can give a more useful error).
  Variable* var = decl->proxy()->var();
  SetType(var, Type::Function());
}


void AsmTyper::VisitFunctionAnnotation(FunctionLiteral* fun) {
  // Extract result type.
  ZoneList<Statement*>* body = fun->body();
  Type* result_type = Type::Undefined();
  if (body->length() > 0) {
    ReturnStatement* stmt = body->last()->AsReturnStatement();
    if (stmt != NULL) {
      Literal* literal = stmt->expression()->AsLiteral();
      Type* old_expected = expected_type_;
      expected_type_ = Type::Any();
      if (literal) {
        RECURSE(VisitLiteral(literal, true));
      } else {
        RECURSE(VisitExpressionAnnotation(stmt->expression(), NULL, true));
      }
      expected_type_ = old_expected;
      result_type = computed_type_;
    }
  }
  Type* type =
      Type::Function(result_type, Type::Any(), fun->parameter_count(), zone());

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
    RECURSE(VisitExpressionAnnotation(expr->value(), var, false));
    if (property_info_ != NULL) {
      SetVariableInfo(var, property_info_);
      property_info_ = NULL;
    }
    SetType(var, computed_type_);
    type->AsFunction()->InitParameter(i, computed_type_);
    good = true;
  }
  if (!good) FAIL(fun, "missing parameter type annotations");

  SetResult(fun, type);
}


void AsmTyper::VisitExpressionAnnotation(Expression* expr, Variable* var,
                                         bool is_return) {
  // Normal +x or x|0 annotations.
  BinaryOperation* bin = expr->AsBinaryOperation();
  if (bin != NULL) {
    if (var != NULL) {
      VariableProxy* proxy = bin->left()->AsVariableProxy();
      if (proxy == NULL) {
        FAIL(bin->left(), "expected variable for type annotation");
      }
      if (proxy->var() != var) {
        FAIL(proxy, "annotation source doesn't match destination");
      }
    }
    Literal* right = bin->right()->AsLiteral();
    if (right != NULL) {
      switch (bin->op()) {
        case Token::MUL:  // We encode +x as x*1.0
          if (right->raw_value()->ContainsDot() &&
              right->raw_value()->AsNumber() == 1.0) {
            SetResult(expr, cache_.kAsmDouble);
            return;
          }
          break;
        case Token::BIT_OR:
          if (!right->raw_value()->ContainsDot() &&
              right->raw_value()->AsNumber() == 0.0) {
            if (is_return) {
              SetResult(expr, cache_.kAsmSigned);
            } else {
              SetResult(expr, cache_.kAsmInt);
            }
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
    VariableProxy* proxy = call->expression()->AsVariableProxy();
    if (proxy != NULL) {
      VariableInfo* info = GetVariableInfo(proxy->var(), false);
      if (!info ||
          (!info->is_check_function && !info->is_constructor_function)) {
        if (allow_simd_) {
          FAIL(call->expression(),
               "only fround/SIMD.checks allowed on expression annotations");
        } else {
          FAIL(call->expression(),
               "only fround allowed on expression annotations");
        }
      }
      Type* type = info->type;
      DCHECK(type->IsFunction());
      if (info->is_check_function) {
        DCHECK(type->AsFunction()->Arity() == 1);
      }
      if (call->arguments()->length() != type->AsFunction()->Arity()) {
        FAIL(call, "invalid argument count calling function");
      }
      SetResult(expr, type->AsFunction()->Result());
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
  RECURSE(VisitWithExpectation(stmt->condition(), cache_.kAsmSigned,
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
  Literal* literal = stmt->expression()->AsLiteral();
  if (literal) {
    VisitLiteral(literal, true);
  } else {
    RECURSE(
        VisitWithExpectation(stmt->expression(), Type::Any(),
                             "return expression expected to have return type"));
  }
  if (!computed_type_->Is(return_type_) || !return_type_->Is(computed_type_)) {
    FAIL(stmt->expression(), "return type does not match function signature");
  }
}


void AsmTyper::VisitWithStatement(WithStatement* stmt) {
  FAIL(stmt, "bad with statement");
}


void AsmTyper::VisitSwitchStatement(SwitchStatement* stmt) {
  if (!in_function_) {
    FAIL(stmt, "switch statement inside module body");
  }
  RECURSE(VisitWithExpectation(stmt->tag(), cache_.kAsmSigned,
                               "switch expression non-integer"));
  ZoneList<CaseClause*>* clauses = stmt->cases();
  ZoneSet<int32_t> cases(zone());
  for (int i = 0; i < clauses->length(); ++i) {
    CaseClause* clause = clauses->at(i);
    if (clause->is_default()) {
      if (i != clauses->length() - 1) {
        FAIL(clause, "default case out of order");
      }
    } else {
      Expression* label = clause->label();
      RECURSE(VisitWithExpectation(label, cache_.kAsmSigned,
                                   "case label non-integer"));
      if (!label->IsLiteral()) FAIL(label, "non-literal case label");
      Handle<Object> value = label->AsLiteral()->value();
      int32_t value32;
      if (!value->ToInt32(&value32)) FAIL(label, "illegal case label value");
      if (cases.find(value32) != cases.end()) {
        FAIL(label, "duplicate case value");
      }
      cases.insert(value32);
    }
    // TODO(bradnelson): Detect duplicates.
    ZoneList<Statement*>* stmts = clause->statements();
    RECURSE(VisitStatements(stmts));
  }
  if (cases.size() > 0) {
    int64_t min_case = *cases.begin();
    int64_t max_case = *cases.rbegin();
    if (max_case - min_case > std::numeric_limits<int32_t>::max()) {
      FAIL(stmt, "case range too large");
    }
  }
}


void AsmTyper::VisitCaseClause(CaseClause* clause) { UNREACHABLE(); }


void AsmTyper::VisitDoWhileStatement(DoWhileStatement* stmt) {
  if (!in_function_) {
    FAIL(stmt, "do statement inside module body");
  }
  RECURSE(Visit(stmt->body()));
  RECURSE(VisitWithExpectation(stmt->cond(), cache_.kAsmSigned,
                               "do condition expected to be integer"));
}


void AsmTyper::VisitWhileStatement(WhileStatement* stmt) {
  if (!in_function_) {
    FAIL(stmt, "while statement inside module body");
  }
  RECURSE(VisitWithExpectation(stmt->cond(), cache_.kAsmSigned,
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
    RECURSE(VisitWithExpectation(stmt->cond(), cache_.kAsmSigned,
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
  if (in_function_) {
    FAIL(expr, "invalid nested function");
  }
  Scope* scope = expr->scope();
  DCHECK(scope->is_function_scope());

  if (!expr->bounds().upper->IsFunction()) {
    FAIL(expr, "invalid function literal");
  }

  Type* type = expr->bounds().upper;
  Type* save_return_type = return_type_;
  return_type_ = type->AsFunction()->Result();
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


void AsmTyper::VisitDoExpression(DoExpression* expr) {
  FAIL(expr, "do-expression encountered");
}


void AsmTyper::VisitConditional(Conditional* expr) {
  if (!in_function_) {
    FAIL(expr, "ternary operator inside module body");
  }
  RECURSE(VisitWithExpectation(expr->condition(), Type::Number(),
                               "condition expected to be integer"));
  if (!computed_type_->Is(cache_.kAsmInt)) {
    FAIL(expr->condition(), "condition must be of type int");
  }

  RECURSE(VisitWithExpectation(
      expr->then_expression(), expected_type_,
      "conditional then branch type mismatch with enclosing expression"));
  Type* then_type = StorageType(computed_type_);
  if (intish_ != 0 || !then_type->Is(cache_.kAsmComparable)) {
    FAIL(expr->then_expression(), "invalid type in ? then expression");
  }

  RECURSE(VisitWithExpectation(
      expr->else_expression(), expected_type_,
      "conditional else branch type mismatch with enclosing expression"));
  Type* else_type = StorageType(computed_type_);
  if (intish_ != 0 || !else_type->Is(cache_.kAsmComparable)) {
    FAIL(expr->else_expression(), "invalid type in ? else expression");
  }

  if (!then_type->Is(else_type) || !else_type->Is(then_type)) {
    FAIL(expr, "then and else expressions in ? must have the same type");
  }

  IntersectResult(expr, then_type);
}


void AsmTyper::VisitVariableProxy(VariableProxy* expr) {
  VisitVariableProxy(expr, false);
}

void AsmTyper::VisitVariableProxy(VariableProxy* expr, bool assignment) {
  Variable* var = expr->var();
  VariableInfo* info = GetVariableInfo(var, false);
  if (!assignment && !in_function_ && !building_function_tables_ &&
      !visiting_exports_) {
    if (var->location() != VariableLocation::PARAMETER || var->index() >= 3) {
      FAIL(expr, "illegal variable reference in module body");
    }
  }
  if (info == NULL || info->type == NULL) {
    if (var->mode() == TEMPORARY) {
      SetType(var, Type::Any());
      info = GetVariableInfo(var, false);
    } else {
      FAIL(expr, "unbound variable");
    }
  }
  if (property_info_ != NULL) {
    SetVariableInfo(var, property_info_);
    property_info_ = NULL;
  }
  Type* type = Type::Intersect(info->type, expected_type_, zone());
  if (type->Is(cache_.kAsmInt)) {
    type = cache_.kAsmInt;
  }
  info->type = type;
  intish_ = 0;
  IntersectResult(expr, type);
}


void AsmTyper::VisitLiteral(Literal* expr, bool is_return) {
  intish_ = 0;
  Handle<Object> value = expr->value();
  if (value->IsNumber()) {
    int32_t i;
    uint32_t u;
    if (expr->raw_value()->ContainsDot()) {
      IntersectResult(expr, cache_.kAsmDouble);
    } else if (!is_return && value->ToUint32(&u)) {
      if (u <= 0x7fffffff) {
        IntersectResult(expr, cache_.kAsmFixnum);
      } else {
        IntersectResult(expr, cache_.kAsmUnsigned);
      }
    } else if (value->ToInt32(&i)) {
      IntersectResult(expr, cache_.kAsmSigned);
    } else {
      FAIL(expr, "illegal number");
    }
  } else if (!is_return && value->IsString()) {
    IntersectResult(expr, Type::String());
  } else if (value->IsUndefined()) {
    IntersectResult(expr, Type::Undefined());
  } else {
    FAIL(expr, "illegal literal");
  }
}


void AsmTyper::VisitLiteral(Literal* expr) { VisitLiteral(expr, false); }


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
    RECURSE(VisitWithExpectation(prop->value(), Type::Any(),
                                 "object property expected to be a function"));
    if (!computed_type_->IsFunction()) {
      FAIL(prop->value(), "non-function in function table");
    }
  }
  IntersectResult(expr, Type::Object());
}


void AsmTyper::VisitArrayLiteral(ArrayLiteral* expr) {
  if (in_function_) {
    FAIL(expr, "array literal inside a function");
  }
  // Allowed for function tables.
  ZoneList<Expression*>* values = expr->values();
  Type* elem_type = Type::None();
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
  Type* target_type = StorageType(computed_type_);
  if (expr->target()->IsVariableProxy()) {
    if (intish_ != 0) {
      FAIL(expr, "intish or floatish assignment");
    }
    expected_type_ = target_type;
    VisitVariableProxy(expr->target()->AsVariableProxy(), true);
  } else if (expr->target()->IsProperty()) {
    int value_intish = intish_;
    Property* property = expr->target()->AsProperty();
    RECURSE(VisitWithExpectation(property->obj(), Type::Any(),
                                 "bad propety object"));
    if (!computed_type_->IsArray()) {
      FAIL(property->obj(), "array expected");
    }
    if (value_intish != 0 && computed_type_->Is(cache_.kFloat64Array)) {
      FAIL(expr, "floatish assignment to double array");
    }
    VisitHeapAccess(property, true, target_type);
  }
  IntersectResult(expr, target_type);
}


void AsmTyper::VisitYield(Yield* expr) {
  FAIL(expr, "yield expression encountered");
}


void AsmTyper::VisitThrow(Throw* expr) {
  FAIL(expr, "throw statement encountered");
}


int AsmTyper::ElementShiftSize(Type* type) {
  if (type->Is(cache_.kAsmSize8)) return 0;
  if (type->Is(cache_.kAsmSize16)) return 1;
  if (type->Is(cache_.kAsmSize32)) return 2;
  if (type->Is(cache_.kAsmSize64)) return 3;
  return -1;
}


Type* AsmTyper::StorageType(Type* type) {
  if (type->Is(cache_.kAsmInt)) {
    return cache_.kAsmInt;
  } else {
    return type;
  }
}


void AsmTyper::VisitHeapAccess(Property* expr, bool assigning,
                               Type* assignment_type) {
  ArrayType* array_type = computed_type_->AsArray();
  //  size_t size = array_size_;
  Type* type = array_type->Element();
  if (type->IsFunction()) {
    if (assigning) {
      FAIL(expr, "assigning to function table is illegal");
    }
    // TODO(bradnelson): Fix the parser and then un-comment this part
    // BinaryOperation* bin = expr->key()->AsBinaryOperation();
    // if (bin == NULL || bin->op() != Token::BIT_AND) {
    //   FAIL(expr->key(), "expected & in call");
    // }
    // RECURSE(VisitWithExpectation(bin->left(), cache_.kAsmSigned,
    //                              "array index expected to be integer"));
    // Literal* right = bin->right()->AsLiteral();
    // if (right == NULL || right->raw_value()->ContainsDot()) {
    //   FAIL(right, "call mask must be integer");
    // }
    // RECURSE(VisitWithExpectation(bin->right(), cache_.kAsmSigned,
    //                              "call mask expected to be integer"));
    // if (static_cast<size_t>(right->raw_value()->AsNumber()) != size - 1) {
    //   FAIL(right, "call mask must match function table");
    // }
    // bin->set_bounds(Bounds(cache_.kAsmSigned));
    RECURSE(VisitWithExpectation(expr->key(), cache_.kAsmSigned,
                                 "must be integer"));
    IntersectResult(expr, type);
  } else {
    Literal* literal = expr->key()->AsLiteral();
    if (literal) {
      RECURSE(VisitWithExpectation(literal, cache_.kAsmSigned,
                                   "array index expected to be integer"));
    } else {
      int expected_shift = ElementShiftSize(type);
      if (expected_shift == 0) {
        RECURSE(Visit(expr->key()));
      } else {
        BinaryOperation* bin = expr->key()->AsBinaryOperation();
        if (bin == NULL || bin->op() != Token::SAR) {
          FAIL(expr->key(), "expected >> in heap access");
        }
        RECURSE(VisitWithExpectation(bin->left(), cache_.kAsmSigned,
                                     "array index expected to be integer"));
        Literal* right = bin->right()->AsLiteral();
        if (right == NULL || right->raw_value()->ContainsDot()) {
          FAIL(right, "heap access shift must be integer");
        }
        RECURSE(VisitWithExpectation(bin->right(), cache_.kAsmSigned,
                                     "array shift expected to be integer"));
        int n = static_cast<int>(right->raw_value()->AsNumber());
        if (expected_shift < 0 || n != expected_shift) {
          FAIL(right, "heap access shift must match element size");
        }
      }
      expr->key()->set_bounds(Bounds(cache_.kAsmSigned));
    }
    Type* result_type;
    if (type->Is(cache_.kAsmIntArrayElement)) {
      result_type = cache_.kAsmIntQ;
      intish_ = kMaxUncombinedAdditiveSteps;
    } else if (type->Is(cache_.kAsmFloat)) {
      if (assigning) {
        result_type = cache_.kAsmFloatDoubleQ;
      } else {
        result_type = cache_.kAsmFloatQ;
      }
      intish_ = 0;
    } else if (type->Is(cache_.kAsmDouble)) {
      if (assigning) {
        result_type = cache_.kAsmFloatDoubleQ;
        if (intish_ != 0) {
          FAIL(expr, "Assignment of floatish to Float64Array");
        }
      } else {
        result_type = cache_.kAsmDoubleQ;
      }
      intish_ = 0;
    } else {
      UNREACHABLE();
    }
    if (assigning) {
      if (!assignment_type->Is(result_type)) {
        FAIL(expr, "illegal type in assignment");
      }
    } else {
      IntersectResult(expr, expected_type_);
      IntersectResult(expr, result_type);
    }
  }
}


bool AsmTyper::IsStdlibObject(Expression* expr) {
  VariableProxy* proxy = expr->AsVariableProxy();
  if (proxy == NULL) {
    return false;
  }
  Variable* var = proxy->var();
  VariableInfo* info = GetVariableInfo(var, false);
  if (info) {
    if (info->standard_member == kStdlib) return true;
  }
  if (var->location() != VariableLocation::PARAMETER || var->index() != 0) {
    return false;
  }
  info = GetVariableInfo(var, true);
  info->type = Type::Object();
  info->standard_member = kStdlib;
  return true;
}


Expression* AsmTyper::GetReceiverOfPropertyAccess(Expression* expr,
                                                  const char* name) {
  Property* property = expr->AsProperty();
  if (property == NULL) {
    return NULL;
  }
  Literal* key = property->key()->AsLiteral();
  if (key == NULL || !key->IsPropertyName() ||
      !key->AsPropertyName()->IsUtf8EqualTo(CStrVector(name))) {
    return NULL;
  }
  return property->obj();
}


bool AsmTyper::IsMathObject(Expression* expr) {
  Expression* obj = GetReceiverOfPropertyAccess(expr, "Math");
  return obj && IsStdlibObject(obj);
}


bool AsmTyper::IsSIMDObject(Expression* expr) {
  Expression* obj = GetReceiverOfPropertyAccess(expr, "SIMD");
  return obj && IsStdlibObject(obj);
}


bool AsmTyper::IsSIMDTypeObject(Expression* expr, const char* name) {
  Expression* obj = GetReceiverOfPropertyAccess(expr, name);
  return obj && IsSIMDObject(obj);
}


void AsmTyper::VisitProperty(Property* expr) {
  if (IsMathObject(expr->obj())) {
    VisitLibraryAccess(&stdlib_math_types_, expr);
    return;
  }
#define V(NAME, Name, name, lane_count, lane_type)               \
  if (IsSIMDTypeObject(expr->obj(), #Name)) {                    \
    VisitLibraryAccess(&stdlib_simd_##name##_types_, expr);      \
    return;                                                      \
  }                                                              \
  if (IsSIMDTypeObject(expr, #Name)) {                           \
    VariableInfo* info = stdlib_simd_##name##_constructor_type_; \
    SetResult(expr, info->type);                                 \
    property_info_ = info;                                       \
    return;                                                      \
  }
  SIMD128_TYPES(V)
#undef V
  if (IsStdlibObject(expr->obj())) {
    VisitLibraryAccess(&stdlib_types_, expr);
    return;
  }

  property_info_ = NULL;

  // Only recurse at this point so that we avoid needing
  // stdlib.Math to have a real type.
  RECURSE(
      VisitWithExpectation(expr->obj(), Type::Any(), "bad property object"));

  // For heap view or function table access.
  if (computed_type_->IsArray()) {
    VisitHeapAccess(expr, false, NULL);
    return;
  }

  VariableProxy* proxy = expr->obj()->AsVariableProxy();
  if (proxy != NULL) {
    Variable* var = proxy->var();
    if (var->location() == VariableLocation::PARAMETER && var->index() == 1) {
      // foreign.x - Function represent as () -> Any
      if (Type::Any()->Is(expected_type_)) {
        SetResult(expr, Type::Function(Type::Any(), zone()));
      } else {
        SetResult(expr, expected_type_);
      }
      return;
    }
  }

  FAIL(expr, "invalid property access");
}


void AsmTyper::VisitCall(Call* expr) {
  Type* expected_type = expected_type_;
  RECURSE(VisitWithExpectation(expr->expression(), Type::Any(),
                               "callee expected to be any"));
  StandardMember standard_member = kNone;
  VariableProxy* proxy = expr->expression()->AsVariableProxy();
  if (proxy) {
    standard_member = VariableAsStandardMember(proxy->var());
  }
  if (!in_function_ && (proxy == NULL || standard_member != kMathFround)) {
    FAIL(expr, "calls forbidden outside function bodies");
  }
  if (proxy == NULL && !expr->expression()->IsProperty()) {
    FAIL(expr, "calls must be to bound variables or function tables");
  }
  if (computed_type_->IsFunction()) {
    FunctionType* fun_type = computed_type_->AsFunction();
    Type* result_type = fun_type->Result();
    ZoneList<Expression*>* args = expr->arguments();
    if (Type::Any()->Is(result_type)) {
      // For foreign calls.
      ZoneList<Expression*>* args = expr->arguments();
      for (int i = 0; i < args->length(); ++i) {
        Expression* arg = args->at(i);
        RECURSE(VisitWithExpectation(
            arg, Type::Any(), "foreign call argument expected to be any"));
        // Checking for asm extern types explicitly, as the type system
        // doesn't correctly check their inheritance relationship.
        if (!computed_type_->Is(cache_.kAsmSigned) &&
            !computed_type_->Is(cache_.kAsmFixnum) &&
            !computed_type_->Is(cache_.kAsmDouble)) {
          FAIL(arg,
               "foreign call argument expected to be int, double, or fixnum");
        }
      }
      intish_ = 0;
      expr->expression()->set_bounds(
          Bounds(Type::Function(Type::Any(), zone())));
      IntersectResult(expr, expected_type);
    } else {
      if (fun_type->Arity() != args->length()) {
        FAIL(expr, "call with wrong arity");
      }
      for (int i = 0; i < args->length(); ++i) {
        Expression* arg = args->at(i);
        RECURSE(VisitWithExpectation(
            arg, fun_type->Parameter(i),
            "call argument expected to match callee parameter"));
        if (standard_member != kNone && standard_member != kMathFround &&
            i == 0) {
          result_type = computed_type_;
        }
      }
      // Handle polymorphic stdlib functions specially.
      if (standard_member == kMathCeil || standard_member == kMathFloor ||
          standard_member == kMathSqrt) {
        if (!args->at(0)->bounds().upper->Is(cache_.kAsmFloat) &&
            !args->at(0)->bounds().upper->Is(cache_.kAsmDouble)) {
          FAIL(expr, "illegal function argument type");
        }
      } else if (standard_member == kMathAbs || standard_member == kMathMin ||
                 standard_member == kMathMax) {
        if (!args->at(0)->bounds().upper->Is(cache_.kAsmFloat) &&
            !args->at(0)->bounds().upper->Is(cache_.kAsmDouble) &&
            !args->at(0)->bounds().upper->Is(cache_.kAsmSigned)) {
          FAIL(expr, "illegal function argument type");
        }
        if (args->length() > 1) {
          Type* other = Type::Intersect(args->at(0)->bounds().upper,
                                        args->at(1)->bounds().upper, zone());
          if (!other->Is(cache_.kAsmFloat) && !other->Is(cache_.kAsmDouble) &&
              !other->Is(cache_.kAsmSigned)) {
            FAIL(expr, "function arguments types don't match");
          }
        }
      }
      intish_ = 0;
      IntersectResult(expr, result_type);
    }
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
    FunctionType* fun_type = computed_type_->AsFunction();
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
  if (!in_function_) {
    FAIL(expr, "unary operator inside module body");
  }
  switch (expr->op()) {
    case Token::NOT:  // Used to encode != and !==
      RECURSE(VisitWithExpectation(expr->expression(), cache_.kAsmInt,
                                   "operand expected to be integer"));
      IntersectResult(expr, cache_.kAsmSigned);
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


void AsmTyper::VisitIntegerBitwiseOperator(BinaryOperation* expr,
                                           Type* left_expected,
                                           Type* right_expected,
                                           Type* result_type, bool conversion) {
  RECURSE(VisitWithExpectation(expr->left(), Type::Number(),
                               "left bitwise operand expected to be a number"));
  int left_intish = intish_;
  Type* left_type = computed_type_;
  if (!left_type->Is(left_expected)) {
    FAIL(expr->left(), "left bitwise operand expected to be an integer");
  }
  if (left_intish > kMaxUncombinedAdditiveSteps) {
    FAIL(expr->left(), "too many consecutive additive ops");
  }

  RECURSE(
      VisitWithExpectation(expr->right(), Type::Number(),
                           "right bitwise operand expected to be a number"));
  int right_intish = intish_;
  Type* right_type = computed_type_;
  if (!right_type->Is(right_expected)) {
    FAIL(expr->right(), "right bitwise operand expected to be an integer");
  }
  if (right_intish > kMaxUncombinedAdditiveSteps) {
    FAIL(expr->right(), "too many consecutive additive ops");
  }

  intish_ = 0;

  if (left_type->Is(cache_.kAsmFixnum) && right_type->Is(cache_.kAsmInt)) {
    left_type = right_type;
  }
  if (right_type->Is(cache_.kAsmFixnum) && left_type->Is(cache_.kAsmInt)) {
    right_type = left_type;
  }
  if (!conversion) {
    if (!left_type->Is(right_type) || !right_type->Is(left_type)) {
      FAIL(expr, "ill-typed bitwise operation");
    }
  }
  IntersectResult(expr, result_type);
}


void AsmTyper::VisitBinaryOperation(BinaryOperation* expr) {
  if (!in_function_) {
    if (expr->op() != Token::BIT_OR && expr->op() != Token::MUL) {
      FAIL(expr, "illegal binary operator inside module body");
    }
    if (!(expr->left()->IsProperty() || expr->left()->IsVariableProxy()) ||
        !expr->right()->IsLiteral()) {
      FAIL(expr, "illegal computation inside module body");
    }
    DCHECK(expr->right()->AsLiteral() != nullptr);
    const AstValue* right_value = expr->right()->AsLiteral()->raw_value();
    if (expr->op() == Token::BIT_OR) {
      if (right_value->AsNumber() != 0.0 || right_value->ContainsDot()) {
        FAIL(expr, "illegal integer annotation value");
      }
    }
    if (expr->op() == Token::MUL) {
      if (right_value->AsNumber() != 1.0 && right_value->ContainsDot()) {
        FAIL(expr, "illegal double annotation value");
      }
    }
  }
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
      FAIL(expr, "illegal logical operator");
    case Token::BIT_OR: {
      // BIT_OR allows Any since it is used as a type coercion.
      VisitIntegerBitwiseOperator(expr, Type::Any(), cache_.kAsmInt,
                                  cache_.kAsmSigned, true);
      if (expr->left()->IsCall() && expr->op() == Token::BIT_OR) {
        expr->left()->set_bounds(Bounds(cache_.kAsmSigned));
      }
      return;
    }
    case Token::BIT_XOR: {
      // Handle booleans specially to handle de-sugared !
      Literal* left = expr->left()->AsLiteral();
      if (left && left->value()->IsBoolean()) {
        if (left->ToBooleanIsTrue()) {
          left->set_bounds(Bounds(cache_.kSingletonOne));
          RECURSE(VisitWithExpectation(expr->right(), cache_.kAsmInt,
                                       "not operator expects an integer"));
          IntersectResult(expr, cache_.kAsmSigned);
          return;
        } else {
          FAIL(left, "unexpected false");
        }
      }
      // BIT_XOR allows Number since it is used as a type coercion (via ~~).
      VisitIntegerBitwiseOperator(expr, Type::Number(), cache_.kAsmInt,
                                  cache_.kAsmSigned, true);
      return;
    }
    case Token::SHR: {
      VisitIntegerBitwiseOperator(expr, cache_.kAsmInt, cache_.kAsmInt,
                                  cache_.kAsmUnsigned, false);
      return;
    }
    case Token::SHL:
    case Token::SAR:
    case Token::BIT_AND: {
      VisitIntegerBitwiseOperator(expr, cache_.kAsmInt, cache_.kAsmInt,
                                  cache_.kAsmSigned, false);
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
      if (type->Is(cache_.kAsmInt)) {
        if (expr->op() == Token::MUL) {
          Literal* right = expr->right()->AsLiteral();
          if (!right) {
            FAIL(expr, "direct integer multiply forbidden");
          }
          if (!right->value()->IsNumber()) {
            FAIL(expr, "multiply must be by an integer");
          }
          int32_t i;
          if (!right->value()->ToInt32(&i)) {
            FAIL(expr, "multiply must be a signed integer");
          }
          i = abs(i);
          if (i >= 1 << 20) {
            FAIL(expr, "multiply must be by value in -2^20 < n < 2^20");
          }
          intish_ = i;
          IntersectResult(expr, cache_.kAsmInt);
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
          IntersectResult(expr, cache_.kAsmInt);
          return;
        }
      } else if (expr->op() == Token::MUL && expr->right()->IsLiteral() &&
                 right_type->Is(cache_.kAsmDouble)) {
        // For unary +, expressed as x * 1.0
        if (expr->left()->IsCall() && expr->op() == Token::MUL) {
          expr->left()->set_bounds(Bounds(cache_.kAsmDouble));
        }
        IntersectResult(expr, cache_.kAsmDouble);
        return;
      } else if (type->Is(cache_.kAsmFloat) && expr->op() != Token::MOD) {
        if (left_intish != 0 || right_intish != 0) {
          FAIL(expr, "float operation before required fround");
        }
        IntersectResult(expr, cache_.kAsmFloat);
        intish_ = 1;
        return;
      } else if (type->Is(cache_.kAsmDouble)) {
        IntersectResult(expr, cache_.kAsmDouble);
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
  if (!in_function_) {
    FAIL(expr, "comparison inside module body");
  }
  Token::Value op = expr->op();
  if (op != Token::EQ && op != Token::NE && op != Token::LT &&
      op != Token::LTE && op != Token::GT && op != Token::GTE) {
    FAIL(expr, "illegal comparison operator");
  }

  RECURSE(
      VisitWithExpectation(expr->left(), Type::Number(),
                           "left comparison operand expected to be number"));
  Type* left_type = computed_type_;
  if (!left_type->Is(cache_.kAsmComparable)) {
    FAIL(expr->left(), "bad type on left side of comparison");
  }

  RECURSE(
      VisitWithExpectation(expr->right(), Type::Number(),
                           "right comparison operand expected to be number"));
  Type* right_type = computed_type_;
  if (!right_type->Is(cache_.kAsmComparable)) {
    FAIL(expr->right(), "bad type on right side of comparison");
  }

  if (!left_type->Is(right_type) && !right_type->Is(left_type)) {
    FAIL(expr, "left and right side of comparison must match");
  }

  IntersectResult(expr, cache_.kAsmSigned);
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


void AsmTyper::InitializeStdlibSIMD() {
#define V(NAME, Name, name, lane_count, lane_type)                            \
  {                                                                           \
    Type* type = Type::Function(Type::Name(isolate_, zone()), Type::Any(),    \
                                lane_count, zone());                          \
    for (int i = 0; i < lane_count; ++i) {                                    \
      type->AsFunction()->InitParameter(i, Type::Number());                   \
    }                                                                         \
    stdlib_simd_##name##_constructor_type_ = new (zone()) VariableInfo(type); \
    stdlib_simd_##name##_constructor_type_->is_constructor_function = true;   \
  }
  SIMD128_TYPES(V)
#undef V
}


void AsmTyper::InitializeStdlib() {
  if (allow_simd_) {
    InitializeStdlibSIMD();
  }
  Type* number_type = Type::Number();
  Type* double_type = cache_.kAsmDouble;
  Type* double_fn1_type = Type::Function(double_type, double_type, zone());
  Type* double_fn2_type =
      Type::Function(double_type, double_type, double_type, zone());

  Type* fround_type = Type::Function(cache_.kAsmFloat, number_type, zone());
  Type* imul_type =
      Type::Function(cache_.kAsmSigned, cache_.kAsmInt, cache_.kAsmInt, zone());
  // TODO(bradnelson): currently only approximating the proper intersection type
  // (which we cannot currently represent).
  Type* number_fn1_type = Type::Function(number_type, number_type, zone());
  Type* number_fn2_type =
      Type::Function(number_type, number_type, number_type, zone());

  struct Assignment {
    const char* name;
    StandardMember standard_member;
    Type* type;
  };

  const Assignment math[] = {{"PI", kMathPI, double_type},
                             {"E", kMathE, double_type},
                             {"LN2", kMathLN2, double_type},
                             {"LN10", kMathLN10, double_type},
                             {"LOG2E", kMathLOG2E, double_type},
                             {"LOG10E", kMathLOG10E, double_type},
                             {"SQRT2", kMathSQRT2, double_type},
                             {"SQRT1_2", kMathSQRT1_2, double_type},
                             {"imul", kMathImul, imul_type},
                             {"abs", kMathAbs, number_fn1_type},
                             {"ceil", kMathCeil, number_fn1_type},
                             {"floor", kMathFloor, number_fn1_type},
                             {"fround", kMathFround, fround_type},
                             {"pow", kMathPow, double_fn2_type},
                             {"exp", kMathExp, double_fn1_type},
                             {"log", kMathLog, double_fn1_type},
                             {"min", kMathMin, number_fn2_type},
                             {"max", kMathMax, number_fn2_type},
                             {"sqrt", kMathSqrt, number_fn1_type},
                             {"cos", kMathCos, double_fn1_type},
                             {"sin", kMathSin, double_fn1_type},
                             {"tan", kMathTan, double_fn1_type},
                             {"acos", kMathAcos, double_fn1_type},
                             {"asin", kMathAsin, double_fn1_type},
                             {"atan", kMathAtan, double_fn1_type},
                             {"atan2", kMathAtan2, double_fn2_type}};
  for (unsigned i = 0; i < arraysize(math); ++i) {
    stdlib_math_types_[math[i].name] = new (zone()) VariableInfo(math[i].type);
    stdlib_math_types_[math[i].name]->standard_member = math[i].standard_member;
  }
  stdlib_math_types_["fround"]->is_check_function = true;

  stdlib_types_["Infinity"] = new (zone()) VariableInfo(double_type);
  stdlib_types_["Infinity"]->standard_member = kInfinity;
  stdlib_types_["NaN"] = new (zone()) VariableInfo(double_type);
  stdlib_types_["NaN"]->standard_member = kNaN;
  Type* buffer_type = Type::Any();
#define TYPED_ARRAY(TypeName, type_name, TYPE_NAME, ctype, size) \
  stdlib_types_[#TypeName "Array"] = new (zone()) VariableInfo(  \
      Type::Function(cache_.k##TypeName##Array, buffer_type, zone()));
  TYPED_ARRAYS(TYPED_ARRAY)
#undef TYPED_ARRAY

#define TYPED_ARRAY(TypeName, type_name, TYPE_NAME, ctype, size)     \
  stdlib_heap_types_[#TypeName "Array"] = new (zone()) VariableInfo( \
      Type::Function(cache_.k##TypeName##Array, buffer_type, zone()));
  TYPED_ARRAYS(TYPED_ARRAY)
#undef TYPED_ARRAY
}


void AsmTyper::VisitLibraryAccess(ObjectTypeMap* map, Property* expr) {
  Literal* key = expr->key()->AsLiteral();
  if (key == NULL || !key->IsPropertyName())
    FAIL(expr, "invalid key used on stdlib member");
  Handle<String> name = key->AsPropertyName();
  VariableInfo* info = LibType(map, name);
  if (info == NULL || info->type == NULL) FAIL(expr, "unknown stdlib function");
  SetResult(expr, info->type);
  property_info_ = info;
}


AsmTyper::VariableInfo* AsmTyper::LibType(ObjectTypeMap* map,
                                          Handle<String> name) {
  base::SmartArrayPointer<char> aname = name->ToCString();
  ObjectTypeMap::iterator i = map->find(std::string(aname.get()));
  if (i == map->end()) {
    return NULL;
  }
  return i->second;
}


void AsmTyper::SetType(Variable* variable, Type* type) {
  VariableInfo* info = GetVariableInfo(variable, true);
  info->type = type;
}


Type* AsmTyper::GetType(Variable* variable) {
  VariableInfo* info = GetVariableInfo(variable, false);
  if (!info) return NULL;
  return info->type;
}


AsmTyper::VariableInfo* AsmTyper::GetVariableInfo(Variable* variable,
                                                  bool setting) {
  ZoneHashMap::Entry* entry;
  ZoneHashMap* map;
  if (in_function_) {
    map = &local_variable_type_;
  } else {
    map = &global_variable_type_;
  }
  if (setting) {
    entry = map->LookupOrInsert(variable, ComputePointerHash(variable),
                                ZoneAllocationPolicy(zone()));
  } else {
    entry = map->Lookup(variable, ComputePointerHash(variable));
    if (!entry && in_function_) {
      entry =
          global_variable_type_.Lookup(variable, ComputePointerHash(variable));
      if (entry && entry->value) {
      }
    }
  }
  if (!entry) return NULL;
  if (!entry->value) {
    if (!setting) return NULL;
    entry->value = new (zone()) VariableInfo;
  }
  return reinterpret_cast<VariableInfo*>(entry->value);
}


void AsmTyper::SetVariableInfo(Variable* variable, const VariableInfo* info) {
  VariableInfo* dest = GetVariableInfo(variable, true);
  dest->type = info->type;
  dest->is_check_function = info->is_check_function;
  dest->is_constructor_function = info->is_constructor_function;
  dest->standard_member = info->standard_member;
}


AsmTyper::StandardMember AsmTyper::VariableAsStandardMember(
    Variable* variable) {
  VariableInfo* info = GetVariableInfo(variable, false);
  if (!info) return kNone;
  return info->standard_member;
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
  if (bounded_type->Is(Type::None())) {
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


void AsmTyper::VisitRewritableExpression(RewritableExpression* expr) {
  RECURSE(Visit(expr->expression()));
}


}  // namespace internal
}  // namespace v8
