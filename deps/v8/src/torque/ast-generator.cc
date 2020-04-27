// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "src/base/macros.h"
#include "src/torque/ast-generator.h"

namespace v8 {
namespace internal {
namespace torque {

namespace {

std::vector<std::string> GetIdentifierVector(
    std::vector<antlr4::tree::TerminalNode*> source) {
  std::vector<std::string> result;
  for (auto s : source) {
    result.push_back(s->getSymbol()->getText());
  }
  return result;
}

std::string StringLiteralUnquote(const std::string& s) {
  assert('"' == s.front() || '\'' == s.front());
  assert('"' == s.back() || '\'' == s.back());
  std::stringstream result;
  for (size_t i = 1; i < s.length() - 1; ++i) {
    if (s[i] == '\\') {
      switch (s[++i]) {
        case 'n':
          result << '\n';
          break;
        case 'r':
          result << '\r';
          break;
        case 't':
          result << '\t';
          break;
        case '\'':
        case '"':
        case '\\':
          result << s[i];
          break;
        default:
          UNREACHABLE();
      }
    } else {
      result << s[i];
    }
  }
  return result.str();
}

}  // namespace

LabelAndTypesVector AstGenerator::GetOptionalLabelAndTypeList(
    TorqueParser::OptionalLabelListContext* context) {
  LabelAndTypesVector labels;
  if (context) {
    for (auto* label : context->labelParameter()) {
      LabelAndTypes new_label;
      new_label.name = label->IDENTIFIER()->getSymbol()->getText();
      if (label->typeList() != nullptr) {
        for (auto* type : label->typeList()->type()) {
          new_label.types.emplace_back(GetType(type));
        }
      }
      labels.emplace_back(new_label);
    }
  }
  return labels;
}

TypeExpression* AstGenerator::GetType(TorqueParser::TypeContext* context) {
  if (context->BUILTIN()) {
    ParameterList parameters = context->typeList()->accept(this);
    TypeExpression* return_type = GetType(context->type());
    return RegisterNode(
        new FunctionTypeExpression(Pos(context), parameters, return_type));
  } else {
    bool is_constexpr = context->CONSTEXPR() != nullptr;
    std::string name = context->IDENTIFIER()->getSymbol()->getText();
    return RegisterNode(
        new BasicTypeExpression(Pos(context), is_constexpr, std::move(name)));
  }
}

TypeExpression* AstGenerator::GetOptionalType(
    TorqueParser::OptionalTypeContext* context) {
  if (!context->type())
    return RegisterNode(new BasicTypeExpression(Pos(context), false, "void"));
  return GetType(context->type());
}

std::vector<TypeExpression*> AstGenerator::GetTypeVector(
    TorqueParser::TypeListContext* type_list) {
  std::vector<TypeExpression*> result;
  for (auto t : type_list->type()) {
    result.push_back(GetType(t));
  }
  return result;
}

ParameterList AstGenerator::GetOptionalParameterList(
    TorqueParser::ParameterListContext* context) {
  if (context != nullptr) {
    return context->accept(this).as<ParameterList>();
  } else {
    return ParameterList();
  }
}

Statement* AstGenerator::GetOptionalHelperBody(
    TorqueParser::HelperBodyContext* context) {
  if (context) return context->accept(this).as<Statement*>();
  return nullptr;
}

antlrcpp::Any AstGenerator::visitParameterList(
    TorqueParser::ParameterListContext* context) {
  ParameterList result{{}, {}, context->VARARGS() != nullptr, {}};
  if (context->VARARGS()) {
    result.arguments_variable = context->IDENTIFIER()->getSymbol()->getText();
  }
  for (auto* parameter : context->parameter()) {
    parameter->accept(this);
    result.names.push_back(parameter->IDENTIFIER()->getSymbol()->getText());
    result.types.push_back(GetType(parameter->type()));
  }
  return std::move(result);
}

antlrcpp::Any AstGenerator::visitTypeList(
    TorqueParser::TypeListContext* context) {
  ParameterList result{{}, {}, false, {}};
  result.types = GetTypeVector(context);
  return std::move(result);
}

antlrcpp::Any AstGenerator::visitTypeListMaybeVarArgs(
    TorqueParser::TypeListMaybeVarArgsContext* context) {
  ParameterList result{{}, {}, context->VARARGS() != nullptr, {}};
  result.types.reserve(context->type().size());
  for (auto* type : context->type()) {
    result.types.push_back(GetType(type));
  }
  return std::move(result);
}

antlrcpp::Any AstGenerator::visitModuleDeclaration(
    TorqueParser::ModuleDeclarationContext* context) {
  ModuleDeclaration* result = RegisterNode(new ExplicitModuleDeclaration{
      Pos(context), context->IDENTIFIER()->getSymbol()->getText(), {}});
  for (auto* declaration : context->declaration()) {
    result->declarations.push_back(
        declaration->accept(this).as<Declaration*>());
  }
  return implicit_cast<Declaration*>(result);
}

antlrcpp::Any AstGenerator::visitMacroDeclaration(
    TorqueParser::MacroDeclarationContext* context) {
  auto generic_parameters =
      GetIdentifierVector(context->optionalGenericTypeList()->IDENTIFIER());
  MacroDeclaration* macro = RegisterNode(new TorqueMacroDeclaration{
      Pos(context), context->IDENTIFIER()->getSymbol()->getText(),
      GetOptionalParameterList(context->parameterList()),
      GetOptionalType(context->optionalType()),
      GetOptionalLabelAndTypeList(context->optionalLabelList())});
  auto body = context->helperBody()->accept(this).as<Statement*>();
  Declaration* result = nullptr;
  if (generic_parameters.size() != 0) {
    result = RegisterNode(
        new GenericDeclaration{Pos(context), macro, generic_parameters, body});
  } else {
    result = RegisterNode(new StandardDeclaration{Pos(context), macro, body});
  }
  return result;
}

antlrcpp::Any AstGenerator::visitBuiltinDeclaration(
    TorqueParser::BuiltinDeclarationContext* context) {
  auto generic_parameters =
      GetIdentifierVector(context->optionalGenericTypeList()->IDENTIFIER());
  Statement* body = context->helperBody()->accept(this).as<Statement*>();

  TorqueBuiltinDeclaration* builtin = RegisterNode(new TorqueBuiltinDeclaration{
      Pos(context), context->JAVASCRIPT() != nullptr,
      context->IDENTIFIER()->getSymbol()->getText(),
      std::move(context->parameterList()->accept(this).as<ParameterList>()),
      GetOptionalType(context->optionalType())});

  Declaration* result = nullptr;
  if (generic_parameters.size() != 0) {
    result = RegisterNode(new GenericDeclaration{Pos(context), builtin,
                                                 generic_parameters, body});
  } else {
    result = RegisterNode(new StandardDeclaration{Pos(context), builtin, body});
  }
  return result;
}

antlrcpp::Any AstGenerator::visitExternalMacro(
    TorqueParser::ExternalMacroContext* context) {
  auto generic_parameters =
      GetIdentifierVector(context->optionalGenericTypeList()->IDENTIFIER());
  MacroDeclaration* macro = RegisterNode(new ExternalMacroDeclaration{
      Pos(context),
      context->IDENTIFIER()->getSymbol()->getText(),
      context->IMPLICIT() != nullptr,
      {},
      std::move(
          context->typeListMaybeVarArgs()->accept(this).as<ParameterList>()),
      GetOptionalType(context->optionalType()),
      GetOptionalLabelAndTypeList(context->optionalLabelList())});
  if (auto* op = context->STRING_LITERAL())
    macro->op = StringLiteralUnquote(op->getSymbol()->getText());
  Declaration* result = nullptr;
  if (generic_parameters.size() != 0) {
    result = RegisterNode(new GenericDeclaration{Pos(context), macro,
                                                 generic_parameters, nullptr});
  } else {
    result =
        RegisterNode(new StandardDeclaration{Pos(context), macro, nullptr});
  }
  return result;
}

antlrcpp::Any AstGenerator::visitExternalBuiltin(
    TorqueParser::ExternalBuiltinContext* context) {
  auto generic_parameters =
      GetIdentifierVector(context->optionalGenericTypeList()->IDENTIFIER());
  ExternalBuiltinDeclaration* builtin =
      RegisterNode(new ExternalBuiltinDeclaration{
          Pos(context), context->JAVASCRIPT() != nullptr,
          context->IDENTIFIER()->getSymbol()->getText(),
          std::move(context->typeList()->accept(this).as<ParameterList>()),
          GetOptionalType(context->optionalType())});

  Declaration* result = nullptr;
  if (generic_parameters.size() != 0) {
    result = RegisterNode(new GenericDeclaration{Pos(context), builtin,
                                                 generic_parameters, nullptr});
  } else {
    result =
        RegisterNode(new StandardDeclaration{Pos(context), builtin, nullptr});
  }
  return result;
}

antlrcpp::Any AstGenerator::visitExternalRuntime(
    TorqueParser::ExternalRuntimeContext* context) {
  ExternalRuntimeDeclaration* runtime =
      RegisterNode(new ExternalRuntimeDeclaration{
          Pos(context), context->IDENTIFIER()->getSymbol()->getText(),
          std::move(context->typeListMaybeVarArgs()
                        ->accept(this)
                        .as<ParameterList>()),
          GetOptionalType(context->optionalType())});
  return implicit_cast<Declaration*>(
      RegisterNode(new StandardDeclaration{Pos(context), runtime, nullptr}));
}

antlrcpp::Any AstGenerator::visitGenericSpecialization(
    TorqueParser::GenericSpecializationContext* context) {
  auto name = context->IDENTIFIER()->getSymbol()->getText();
  auto specialization_parameters =
      GetTypeVector(context->genericSpecializationTypeList()->typeList());
  Statement* body = context->helperBody()->accept(this).as<Statement*>();
  return implicit_cast<Declaration*>(RegisterNode(new SpecializationDeclaration{
      Pos(context), name, specialization_parameters,
      GetOptionalParameterList(context->parameterList()),
      GetOptionalType(context->optionalType()),
      GetOptionalLabelAndTypeList(context->optionalLabelList()), body}));
}

antlrcpp::Any AstGenerator::visitConstDeclaration(
    TorqueParser::ConstDeclarationContext* context) {
  return implicit_cast<Declaration*>(RegisterNode(new ConstDeclaration{
      Pos(context), context->IDENTIFIER()->getSymbol()->getText(),
      GetType(context->type()),
      StringLiteralUnquote(
          context->STRING_LITERAL()->getSymbol()->getText())}));
}

antlrcpp::Any AstGenerator::visitTypeDeclaration(
    TorqueParser::TypeDeclarationContext* context) {
  TypeDeclaration* result = RegisterNode(new TypeDeclaration{
      Pos(context), context->IDENTIFIER()->getSymbol()->getText(), {}, {}});
  if (context->extendsDeclaration())
    result->extends =
        context->extendsDeclaration()->IDENTIFIER()->getSymbol()->getText();
  if (context->generatesDeclaration()) {
    result->generates = StringLiteralUnquote(context->generatesDeclaration()
                                                 ->STRING_LITERAL()
                                                 ->getSymbol()
                                                 ->getText());
  }
  if (context->constexprDeclaration()) {
    result->constexpr_generates =
        StringLiteralUnquote(context->constexprDeclaration()
                                 ->STRING_LITERAL()
                                 ->getSymbol()
                                 ->getText());
  }
  return implicit_cast<Declaration*>(result);
}

antlrcpp::Any AstGenerator::visitVariableDeclaration(
    TorqueParser::VariableDeclarationContext* context) {
  return RegisterNode(
      new VarDeclarationStatement{Pos(context),
                                  context->IDENTIFIER()->getSymbol()->getText(),
                                  GetType(context->type()),
                                  {}});
}

antlrcpp::Any AstGenerator::visitVariableDeclarationWithInitialization(
    TorqueParser::VariableDeclarationWithInitializationContext* context) {
  VarDeclarationStatement* result =
      VarDeclarationStatement::cast(context->variableDeclaration()
                                        ->accept(this)
                                        .as<VarDeclarationStatement*>());
  result->pos = Pos(context);
  if (context->expression())
    result->initializer = context->expression()->accept(this).as<Expression*>();
  return implicit_cast<Statement*>(result);
}

antlrcpp::Any AstGenerator::visitHelperCall(
    TorqueParser::HelperCallContext* context) {
  antlr4::tree::TerminalNode* callee;
  bool is_operator = context->MIN() || context->MAX();
  if (context->MIN()) callee = context->MIN();
  if (context->MAX()) callee = context->MAX();
  if (context->IDENTIFIER()) callee = context->IDENTIFIER();
  std::vector<std::string> labels;
  for (auto label : context->optionalOtherwise()->IDENTIFIER()) {
    labels.push_back(label->getSymbol()->getText());
  }
  std::vector<TypeExpression*> templateArguments;
  if (context->genericSpecializationTypeList()) {
    templateArguments =
        GetTypeVector(context->genericSpecializationTypeList()->typeList());
  }
  CallExpression* result =
      RegisterNode(new CallExpression{Pos(context),
                                      callee->getSymbol()->getText(),
                                      is_operator,
                                      templateArguments,
                                      {},
                                      labels});
  for (auto* arg : context->argumentList()->argument()) {
    result->arguments.push_back(arg->accept(this).as<Expression*>());
  }
  return implicit_cast<Expression*>(result);
}

antlrcpp::Any AstGenerator::visitHelperCallStatement(
    TorqueParser::HelperCallStatementContext* context) {
  Statement* result;
  if (context->TAIL()) {
    result = RegisterNode(new TailCallStatement{
        Pos(context),
        CallExpression::cast(
            context->helperCall()->accept(this).as<Expression*>())});
  } else {
    result = RegisterNode(new ExpressionStatement{
        Pos(context), context->helperCall()->accept(this).as<Expression*>()});
  }
  return result;
}

antlrcpp::Any AstGenerator::visitStatementScope(
    TorqueParser::StatementScopeContext* context) {
  BlockStatement* result = RegisterNode(
      new BlockStatement{Pos(context), context->DEFERRED() != nullptr, {}});
  for (auto* child : context->statementList()->statement()) {
    result->statements.push_back(child->accept(this).as<Statement*>());
  }
  return implicit_cast<Statement*>(result);
}

antlrcpp::Any AstGenerator::visitExpressionStatement(
    TorqueParser::ExpressionStatementContext* context) {
  return implicit_cast<Statement*>(RegisterNode(new ExpressionStatement{
      Pos(context), context->assignment()->accept(this).as<Expression*>()}));
}

antlrcpp::Any AstGenerator::visitReturnStatement(
    TorqueParser::ReturnStatementContext* context) {
  if (context->expression() != nullptr) {
    return implicit_cast<Statement*>(RegisterNode(new ReturnStatement{
        Pos(context), context->expression()->accept(this).as<Expression*>()}));
  } else {
    return implicit_cast<Statement*>(
        RegisterNode(new ReturnStatement{Pos(context), {}}));
  }
}

antlrcpp::Any AstGenerator::visitBreakStatement(
    TorqueParser::BreakStatementContext* context) {
  return implicit_cast<Statement*>(
      RegisterNode(new BreakStatement{Pos(context)}));
}

antlrcpp::Any AstGenerator::visitContinueStatement(
    TorqueParser::ContinueStatementContext* context) {
  return implicit_cast<Statement*>(
      RegisterNode(new ContinueStatement{Pos(context)}));
}

antlrcpp::Any AstGenerator::visitGotoStatement(
    TorqueParser::GotoStatementContext* context) {
  GotoStatement* result = RegisterNode(new GotoStatement{Pos(context), {}, {}});
  if (context->labelReference())
    result->label =
        context->labelReference()->IDENTIFIER()->getSymbol()->getText();
  if (context->argumentList() != nullptr) {
    for (auto a : context->argumentList()->argument()) {
      result->arguments.push_back(a->accept(this).as<Expression*>());
    }
  }
  return implicit_cast<Statement*>(result);
}

antlrcpp::Any AstGenerator::visitIfStatement(
    TorqueParser::IfStatementContext* context) {
  IfStatement* result = RegisterNode(new IfStatement{
      Pos(context),
      std::move(context->expression()->accept(this).as<Expression*>()),
      context->CONSTEXPR() != nullptr,
      std::move(context->statementBlock(0)->accept(this).as<Statement*>()),
      {}});
  if (context->statementBlock(1))
    result->if_false =
        std::move(context->statementBlock(1)->accept(this).as<Statement*>());
  return implicit_cast<Statement*>(result);
}

antlrcpp::Any AstGenerator::visitWhileLoop(
    TorqueParser::WhileLoopContext* context) {
  return implicit_cast<Statement*>(RegisterNode(new WhileStatement{
      Pos(context), context->expression()->accept(this).as<Expression*>(),
      context->statementBlock()->accept(this).as<Statement*>()}));
}

antlrcpp::Any AstGenerator::visitForLoop(
    TorqueParser::ForLoopContext* context) {
  ForLoopStatement* result = RegisterNode(new ForLoopStatement{
      Pos(context),
      {},
      context->expression()->accept(this).as<Expression*>(),
      context->assignment()->accept(this).as<Expression*>(),
      context->statementBlock()->accept(this).as<Statement*>()});
  if (auto* init = context->forInitialization()
                       ->variableDeclarationWithInitialization()) {
    result->var_declaration =
        VarDeclarationStatement::cast(init->accept(this).as<Statement*>());
  }
  return implicit_cast<Statement*>(result);
}

antlrcpp::Any AstGenerator::visitForOfLoop(
    TorqueParser::ForOfLoopContext* context) {
  ForOfLoopStatement* result = RegisterNode(new ForOfLoopStatement{
      Pos(context),
      context->variableDeclaration()
          ->accept(this)
          .as<VarDeclarationStatement*>(),
      context->expression()->accept(this).as<Expression*>(),
      {},
      {},
      context->statementBlock()->accept(this).as<Statement*>()});
  if (auto* range = context->forOfRange()->rangeSpecifier()) {
    if (auto* begin = range->begin) {
      result->begin = begin->accept(this).as<Expression*>();
    }
    if (auto* end = range->end) {
      result->end = end->accept(this).as<Expression*>();
    }
  }
  return implicit_cast<Statement*>(result);
}

antlrcpp::Any AstGenerator::visitTryCatch(
    TorqueParser::TryCatchContext* context) {
  TryCatchStatement* result = RegisterNode(new TryCatchStatement{
      Pos(context),
      context->statementBlock()->accept(this).as<Statement*>(),
      {}});
  for (auto* handler : context->handlerWithStatement()) {
    if (handler->CATCH() != nullptr) {
      CatchBlock* catch_block = RegisterNode(new CatchBlock{
          Pos(handler->statementBlock()),
          {},
          handler->statementBlock()->accept(this).as<Statement*>()});
      catch_block->caught = handler->IDENTIFIER()->getSymbol()->getText();
      result->catch_blocks.push_back(catch_block);
    } else {
      handler->labelDeclaration()->accept(this);
      auto parameter_list = handler->labelDeclaration()->parameterList();
      ParameterList label_parameters = parameter_list == nullptr
                                           ? ParameterList()
                                           : handler->labelDeclaration()
                                                 ->parameterList()
                                                 ->accept(this)
                                                 .as<ParameterList>();
      LabelBlock* label_block = RegisterNode(new LabelBlock{
          Pos(handler->statementBlock()),
          handler->labelDeclaration()->IDENTIFIER()->getSymbol()->getText(),
          label_parameters,
          handler->statementBlock()->accept(this).as<Statement*>()});
      result->label_blocks.push_back(label_block);
    }
  }
  return implicit_cast<Statement*>(result);
}

antlrcpp::Any AstGenerator::visitPrimaryExpression(
    TorqueParser::PrimaryExpressionContext* context) {
  if (auto* e = context->helperCall()) return e->accept(this);
  if (auto* e = context->DECIMAL_LITERAL())
    return implicit_cast<Expression*>(RegisterNode(
        new NumberLiteralExpression{Pos(context), e->getSymbol()->getText()}));
  if (auto* e = context->STRING_LITERAL())
    return implicit_cast<Expression*>(RegisterNode(
        new StringLiteralExpression{Pos(context), e->getSymbol()->getText()}));
  if (context->CONVERT_KEYWORD())
    return implicit_cast<Expression*>(RegisterNode(new ConvertExpression{
        Pos(context), GetType(context->type()),
        context->expression()->accept(this).as<Expression*>()}));
  if (context->CAST_KEYWORD())
    return implicit_cast<Expression*>(RegisterNode(new CastExpression{
        Pos(context), GetType(context->type()),
        context->IDENTIFIER()->getSymbol()->getText(),
        context->expression()->accept(this).as<Expression*>()}));
  return context->expression()->accept(this);
}

antlrcpp::Any AstGenerator::visitAssignment(
    TorqueParser::AssignmentContext* context) {
  if (auto* e = context->incrementDecrement()) return e->accept(this);
  LocationExpression* location = LocationExpression::cast(
      context->locationExpression()->accept(this).as<Expression*>());
  if (auto* e = context->expression()) {
    AssignmentExpression* result = RegisterNode(new AssignmentExpression{
        Pos(context), location, {}, e->accept(this).as<Expression*>()});
    if (auto* op_node = context->ASSIGNMENT_OPERATOR()) {
      std::string op = op_node->getSymbol()->getText();
      result->op = op.substr(0, op.length() - 1);
    }
    return implicit_cast<Expression*>(result);
  }
  return implicit_cast<Expression*>(location);
}

antlrcpp::Any AstGenerator::visitIncrementDecrement(
    TorqueParser::IncrementDecrementContext* context) {
  bool postfix = context->op;
  return implicit_cast<Expression*>(
      RegisterNode(new IncrementDecrementExpression{
          Pos(context),
          LocationExpression::cast(
              context->locationExpression()->accept(this).as<Expression*>()),
          context->INCREMENT() ? IncrementDecrementOperator::kIncrement
                               : IncrementDecrementOperator::kDecrement,
          postfix}));
}

antlrcpp::Any AstGenerator::visitLocationExpression(
    TorqueParser::LocationExpressionContext* context) {
  if (auto* l = context->locationExpression()) {
    Expression* location = l->accept(this).as<Expression*>();
    if (auto* e = context->expression()) {
      return implicit_cast<Expression*>(
          RegisterNode(new ElementAccessExpression{
              Pos(context), location, e->accept(this).as<Expression*>()}));
    }
    return implicit_cast<Expression*>(RegisterNode(new FieldAccessExpression{
        Pos(context), location,
        context->IDENTIFIER()->getSymbol()->getText()}));
  }
  std::vector<TypeExpression*> templateArguments;
  if (context->genericSpecializationTypeList()) {
    templateArguments =
        GetTypeVector(context->genericSpecializationTypeList()->typeList());
  }
  return implicit_cast<Expression*>(RegisterNode(new IdentifierExpression{
      Pos(context), context->IDENTIFIER()->getSymbol()->getText(),
      std::move(templateArguments)}));
}

antlrcpp::Any AstGenerator::visitUnaryExpression(
    TorqueParser::UnaryExpressionContext* context) {
  if (auto* e = context->assignmentExpression()) return e->accept(this);
  std::vector<Expression*> args;
  args.push_back(context->unaryExpression()->accept(this).as<Expression*>());
  return implicit_cast<Expression*>(RegisterNode(new CallExpression{
      Pos(context), context->op->getText(), true, {}, std::move(args), {}}));
}

antlrcpp::Any AstGenerator::visitMultiplicativeExpression(
    TorqueParser::MultiplicativeExpressionContext* context) {
  auto* right = context->unaryExpression();
  if (auto* left = context->multiplicativeExpression()) {
    return implicit_cast<Expression*>(
        RegisterNode(new CallExpression{Pos(context),
                                        context->op->getText(),
                                        true,
                                        {},
                                        {left->accept(this).as<Expression*>(),
                                         right->accept(this).as<Expression*>()},
                                        {}}));
  }
  return right->accept(this);
}

antlrcpp::Any AstGenerator::visitAdditiveExpression(
    TorqueParser::AdditiveExpressionContext* context) {
  auto* right = context->multiplicativeExpression();
  if (auto* left = context->additiveExpression()) {
    return implicit_cast<Expression*>(
        RegisterNode(new CallExpression{Pos(context),
                                        context->op->getText(),
                                        true,
                                        {},
                                        {left->accept(this).as<Expression*>(),
                                         right->accept(this).as<Expression*>()},
                                        {}}));
  }
  return right->accept(this);
}

antlrcpp::Any AstGenerator::visitShiftExpression(
    TorqueParser::ShiftExpressionContext* context) {
  auto* right = context->additiveExpression();
  if (auto* left = context->shiftExpression()) {
    return implicit_cast<Expression*>(
        RegisterNode(new CallExpression{Pos(context),
                                        context->op->getText(),
                                        true,
                                        {},
                                        {left->accept(this).as<Expression*>(),
                                         right->accept(this).as<Expression*>()},
                                        {}}));
  }
  return right->accept(this);
}

antlrcpp::Any AstGenerator::visitRelationalExpression(
    TorqueParser::RelationalExpressionContext* context) {
  auto* right = context->shiftExpression();
  if (auto* left = context->relationalExpression()) {
    return implicit_cast<Expression*>(
        RegisterNode(new CallExpression{Pos(context),
                                        context->op->getText(),
                                        true,
                                        {},
                                        {left->accept(this).as<Expression*>(),
                                         right->accept(this).as<Expression*>()},
                                        {}}));
  }
  return right->accept(this);
}

antlrcpp::Any AstGenerator::visitEqualityExpression(
    TorqueParser::EqualityExpressionContext* context) {
  auto* right = context->relationalExpression();
  if (auto* left = context->equalityExpression()) {
    return implicit_cast<Expression*>(
        RegisterNode(new CallExpression{Pos(context),
                                        context->op->getText(),
                                        true,
                                        {},
                                        {left->accept(this).as<Expression*>(),
                                         right->accept(this).as<Expression*>()},
                                        {}}));
  }
  return right->accept(this);
}

antlrcpp::Any AstGenerator::visitBitwiseExpression(
    TorqueParser::BitwiseExpressionContext* context) {
  auto* right = context->equalityExpression();
  if (auto* left = context->bitwiseExpression()) {
    return implicit_cast<Expression*>(
        RegisterNode(new CallExpression{Pos(context),
                                        context->op->getText(),
                                        true,
                                        {},
                                        {left->accept(this).as<Expression*>(),
                                         right->accept(this).as<Expression*>()},
                                        {}}));
  }
  return right->accept(this);
}

antlrcpp::Any AstGenerator::visitLogicalANDExpression(
    TorqueParser::LogicalANDExpressionContext* context) {
  auto* right = context->bitwiseExpression();
  if (auto* left = context->logicalANDExpression()) {
    return implicit_cast<Expression*>(RegisterNode(new LogicalAndExpression{
        Pos(context), left->accept(this).as<Expression*>(),
        right->accept(this).as<Expression*>()}));
  }
  return right->accept(this);
}

antlrcpp::Any AstGenerator::visitLogicalORExpression(
    TorqueParser::LogicalORExpressionContext* context) {
  auto* right = context->logicalANDExpression();
  if (auto* left = context->logicalORExpression()) {
    return implicit_cast<Expression*>(RegisterNode(new LogicalOrExpression{
        Pos(context), left->accept(this).as<Expression*>(),
        right->accept(this).as<Expression*>()}));
  }
  return right->accept(this);
}

antlrcpp::Any AstGenerator::visitConditionalExpression(
    TorqueParser::ConditionalExpressionContext* context) {
  if (auto* condition = context->conditionalExpression()) {
    return implicit_cast<Expression*>(RegisterNode(new ConditionalExpression{
        Pos(context), condition->accept(this).as<Expression*>(),
        context->logicalORExpression(0)->accept(this).as<Expression*>(),
        context->logicalORExpression(1)->accept(this).as<Expression*>()}));
  }
  return context->logicalORExpression(0)->accept(this);
}

antlrcpp::Any AstGenerator::visitDiagnosticStatement(
    TorqueParser::DiagnosticStatementContext* context) {
  if (context->ASSERT()) {
    size_t a = context->expression()->start->getStartIndex();
    size_t b = context->expression()->stop->getStopIndex();
    antlr4::misc::Interval interval(a, b);
    std::string source = source_file_context_->stream->getText(interval);
    return implicit_cast<Statement*>(RegisterNode(new AssertStatement{
        Pos(context), context->expression()->accept(this).as<Expression*>(),
        source}));
  } else if (context->UNREACHABLE_TOKEN()) {
    return implicit_cast<Statement*>(
        RegisterNode(new DebugStatement{Pos(context), "unreachable", true}));
  } else {
    DCHECK(context->DEBUG_TOKEN());
    return implicit_cast<Statement*>(
        RegisterNode(new DebugStatement{Pos(context), "debug", false}));
  }
}

void AstGenerator::visitSourceFile(SourceFileContext* context) {
  source_file_context_ = context;
  current_source_file_ = SourceFileMap::Get().AddSource(context->name);
  for (auto* declaration : context->file->children) {
    ast_.declarations().push_back(declaration->accept(this).as<Declaration*>());
  }
  source_file_context_ = nullptr;
}

SourcePosition AstGenerator::Pos(antlr4::ParserRuleContext* context) {
  antlr4::misc::Interval i = context->getSourceInterval();
  auto token = source_file_context_->tokens->get(i.a);
  int line = static_cast<int>(token->getLine());
  int column = static_cast<int>(token->getCharPositionInLine());
  return SourcePosition{current_source_file_, line, column};
}

}  // namespace torque
}  // namespace internal
}  // namespace v8
