// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_AST_VISITOR_H_
#define V8_TORQUE_AST_VISITOR_H_

#include "src/torque/ast.h"

namespace v8::internal::torque {

template <typename Derived>
class AstVisitor {
 public:
  void Run(Ast& ast) { d_this()->Visit(ast.declarations()); }

 protected:
  template <typename T>
    requires(std::is_base_of_v<AstNode, T>)
  T* Visit(T* node) {
    switch (node->kind) {
#define NODE_CASE(name)        \
  case AstNode::Kind::k##name: \
    return T::cast(d_this()->Visit##name(name::cast(node)));
      AST_NODE_KIND_LIST(NODE_CASE)
#undef NODE_CASE
      default:
        UNREACHABLE();
    }
  }

  template <typename T>
    requires(std::is_base_of_v<AstNode, T>)
  std::vector<T*> Visit(std::vector<T*> nodes) {
    std::vector<T*> result;
    for (T* node : nodes) {
      AstNode* new_node = d_this()->Visit(node);
      if (new_node) {
        result.push_back(T::cast(new_node));
      }
    }
    return result;
  }

  template <typename T>
    requires(std::is_base_of_v<AstNode, T>)
  std::optional<T*> Visit(std::optional<T*> node) {
    if (node.has_value()) {
      AstNode* new_node = d_this()->Visit(node.value());
      if (!new_node) return {};
      return {T::cast(new_node)};
    }
    return node;
  }

  std::vector<NameAndExpression> Visit(std::vector<NameAndExpression> naes) {
    for (NameAndExpression& nae : naes) {
      nae.name = d_this()->Visit(nae.name);
      nae.expression = d_this()->Visit(nae.expression);
    }
    return naes;
  }

  NameAndTypeExpression Visit(NameAndTypeExpression nate) {
    nate.name = d_this()->Visit(nate.name);
    nate.type = d_this()->Visit(nate.type);
    return nate;
  }

  ParameterList Visit(ParameterList list) {
    for (auto& name : list.names) name = d_this()->Visit(name);
    for (auto& type : list.types) type = d_this()->Visit(type);
    return list;
  }

  std::vector<LabelAndTypes> Visit(std::vector<LabelAndTypes> lats) {
    for (auto& lat : lats) {
      lat.name = d_this()->Visit(lat.name);
      for (auto& type : lat.types) {
        type = d_this()->Visit(type);
      }
    }
    return lats;
  }

  std::vector<ClassFieldExpression> Visit(
      std::vector<ClassFieldExpression> cfes) {
    for (auto& cfe : cfes) {
      cfe.name_and_type = d_this()->Visit(cfe.name_and_type);
      if (cfe.index.has_value()) {
        cfe.index.value().expr = d_this()->Visit(cfe.index.value().expr);
      }
    }
    return cfes;
  }

  std::vector<TypeswitchCase> Visit(std::vector<TypeswitchCase> cases) {
    for (auto& c : cases) {
      c.name = d_this()->Visit(c.name);
      c.type = d_this()->Visit(c.type);
      c.block = d_this()->Visit(c.block);
    }
    return cases;
  }

  std::vector<BitFieldDeclaration> Visit(
      std::vector<BitFieldDeclaration> fields) {
    for (auto& field : fields) {
      field.name_and_type = d_this()->Visit(field.name_and_type);
    }
    return fields;
  }

  std::vector<GenericParameter> Visit(std::vector<GenericParameter> gps) {
    for (auto& gp : gps) {
      gp.name = d_this()->Visit(gp.name);
      gp.constraint = d_this()->Visit(gp.constraint);
    }
    return gps;
  }

  std::vector<StructFieldExpression> Visit(
      std::vector<StructFieldExpression> structs) {
    for (auto& s : structs) {
      s.name_and_type = Visit(s.name_and_type);
    }
    return structs;
  }

#define VISIT(member) node->member = d_this()->Visit(node->member);
  AstNode* VisitCallExpression(CallExpression* node) {
    VISIT(callee);
    VISIT(arguments);
    VISIT(labels);
    return node;
  }

  AstNode* VisitCallMethodExpression(CallMethodExpression* node) {
    VISIT(target);
    VISIT(method);
    VISIT(arguments);
    VISIT(labels);
    return node;
  }

  AstNode* VisitIntrinsicCallExpression(IntrinsicCallExpression* node) {
    VISIT(name);
    VISIT(generic_arguments);
    VISIT(arguments);
    return node;
  }

  AstNode* VisitStructExpression(StructExpression* node) {
    VISIT(type);
    VISIT(initializers);
    return node;
  }

  AstNode* VisitLogicalOrExpression(LogicalOrExpression* node) {
    VISIT(left);
    VISIT(right);
    return node;
  }

  AstNode* VisitLogicalAndExpression(LogicalAndExpression* node) {
    VISIT(left);
    VISIT(right);
    return node;
  }

  AstNode* VisitSpreadExpression(SpreadExpression* node) {
    VISIT(spreadee);
    return node;
  }

  AstNode* VisitConditionalExpression(ConditionalExpression* node) {
    VISIT(condition);
    VISIT(if_true);
    VISIT(if_false);
    return node;
  }

  AstNode* VisitIdentifierExpression(IdentifierExpression* node) {
    VISIT(name);
    VISIT(generic_arguments);
    return node;
  }

  AstNode* VisitStringLiteralExpression(StringLiteralExpression* node) {
    return node;
  }

  AstNode* VisitIntegerLiteralExpression(IntegerLiteralExpression* node) {
    return node;
  }

  AstNode* VisitFloatingPointLiteralExpression(
      FloatingPointLiteralExpression* node) {
    return node;
  }

  AstNode* VisitFieldAccessExpression(FieldAccessExpression* node) {
    VISIT(object);
    VISIT(field);
    return node;
  }

  AstNode* VisitElementAccessExpression(ElementAccessExpression* node) {
    VISIT(array);
    VISIT(index);
    return node;
  }

  AstNode* VisitDereferenceExpression(DereferenceExpression* node) {
    VISIT(reference);
    return node;
  }

  AstNode* VisitAssignmentExpression(AssignmentExpression* node) {
    VISIT(location);
    VISIT(value);
    return node;
  }

  AstNode* VisitIncrementDecrementExpression(
      IncrementDecrementExpression* node) {
    VISIT(location);
    return node;
  }

  AstNode* VisitNewExpression(NewExpression* node) {
    VISIT(type);
    VISIT(initializers);
    return node;
  }

  AstNode* VisitAssumeTypeImpossibleExpression(
      AssumeTypeImpossibleExpression* node) {
    VISIT(excluded_type);
    VISIT(expression);
    return node;
  }

  AstNode* VisitStatementExpression(StatementExpression* node) {
    VISIT(statement);
    return node;
  }

  AstNode* VisitTryLabelExpression(TryLabelExpression* node) {
    VISIT(try_expression);
    VISIT(label_block);
    return node;
  }

  AstNode* VisitBasicTypeExpression(BasicTypeExpression* node) {
    VISIT(name);
    VISIT(generic_arguments);
    return node;
  }

  AstNode* VisitFunctionTypeExpression(FunctionTypeExpression* node) {
    VISIT(parameters);
    VISIT(return_type);
    return node;
  }

  AstNode* VisitPrecomputedTypeExpression(PrecomputedTypeExpression* node) {
    return node;
  }

  AstNode* VisitUnionTypeExpression(UnionTypeExpression* node) {
    VISIT(a);
    VISIT(b);
    return node;
  }

  AstNode* VisitBlockStatement(BlockStatement* node) {
    VISIT(statements);
    return node;
  }

  AstNode* VisitExpressionStatement(ExpressionStatement* node) {
    VISIT(expression);
    return node;
  }

  AstNode* VisitIfStatement(IfStatement* node) {
    VISIT(condition);
    VISIT(if_true);
    VISIT(if_false);
    return node;
  }

  AstNode* VisitWhileStatement(WhileStatement* node) {
    VISIT(condition);
    VISIT(body);
    return node;
  }

  AstNode* VisitTypeswitchStatement(TypeswitchStatement* node) {
    VISIT(expr);
    VISIT(cases);
    return node;
  }

  AstNode* VisitForLoopStatement(ForLoopStatement* node) {
    VISIT(var_declaration);
    VISIT(test);
    VISIT(action);
    VISIT(body);
    return node;
  }

  AstNode* VisitBreakStatement(BreakStatement* node) { return node; }

  AstNode* VisitContinueStatement(ContinueStatement* node) { return node; }

  AstNode* VisitReturnStatement(ReturnStatement* node) {
    VISIT(value);
    return node;
  }

  AstNode* VisitDebugStatement(DebugStatement* node) { return node; }

  AstNode* VisitAssertStatement(AssertStatement* node) {
    VISIT(expression);
    return node;
  }

  AstNode* VisitTailCallStatement(TailCallStatement* node) {
    VISIT(call);
    return node;
  }

  AstNode* VisitVarDeclarationStatement(VarDeclarationStatement* node) {
    VISIT(name);
    VISIT(type);
    VISIT(initializer);
    return node;
  }

  AstNode* VisitGotoStatement(GotoStatement* node) {
    VISIT(label);
    VISIT(arguments);
    return node;
  }

  AstNode* VisitAbstractTypeDeclaration(AbstractTypeDeclaration* node) {
    // TypeDeclaration
    VISIT(name);
    // AbstractTypeDeclaration
    VISIT(extends);
    return node;
  }

  AstNode* VisitTypeAliasDeclaration(TypeAliasDeclaration* node) {
    // TypeDeclaration
    VISIT(name);
    // TypeAliasDeclaration
    VISIT(type);
    return node;
  }

  AstNode* VisitBitFieldStructDeclaration(BitFieldStructDeclaration* node) {
    // TypeDeclaration
    VISIT(name);
    // BitFieldStructDeclaration
    VISIT(parent);
    VISIT(fields);
    return node;
  }

  AstNode* VisitClassDeclaration(ClassDeclaration* node) {
    // TypeDeclaration
    VISIT(name);
    // ClassDeclaration
    VISIT(super);
    VISIT(methods);
    VISIT(fields);
    return node;
  }

  AstNode* VisitStructDeclaration(StructDeclaration* node) {
    // TypeDeclaration
    VISIT(name);
    // StructDeclaration
    VISIT(methods);
    VISIT(fields);
    return node;
  }

  AstNode* VisitGenericCallableDeclaration(GenericCallableDeclaration* node) {
    VISIT(generic_parameters);
    VISIT(declaration);
    return node;
  }

  AstNode* VisitGenericTypeDeclaration(GenericTypeDeclaration* node) {
    VISIT(generic_parameters);
    VISIT(declaration);
    return node;
  }

  AstNode* VisitSpecializationDeclaration(SpecializationDeclaration* node) {
    // CallableDeclaration
    VISIT(name);
    VISIT(parameters);
    VISIT(return_type);
    VISIT(labels);
    // SpecializationDeclaration
    VISIT(generic_parameters);
    VISIT(body);
    return node;
  }

  AstNode* VisitExternConstDeclaration(ExternConstDeclaration* node) {
    VISIT(name);
    VISIT(type);
    return node;
  }

  AstNode* VisitNamespaceDeclaration(NamespaceDeclaration* node) {
    VISIT(declarations);
    return node;
  }

  AstNode* VisitConstDeclaration(ConstDeclaration* node) {
    VISIT(name);
    VISIT(type);
    VISIT(expression);
    return node;
  }

  AstNode* VisitCppIncludeDeclaration(CppIncludeDeclaration* node) {
    return node;
  }

  AstNode* VisitTorqueMacroDeclaration(TorqueMacroDeclaration* node) {
    // CallableDeclaration
    VISIT(name);
    VISIT(parameters);
    VISIT(return_type);
    VISIT(labels);
    // TorqueMacroDeclaration
    VISIT(body);
    return node;
  }

  AstNode* VisitTorqueBuiltinDeclaration(TorqueBuiltinDeclaration* node) {
    // CallableDeclaration
    VISIT(name);
    VISIT(parameters);
    VISIT(return_type);
    VISIT(labels);
    // TorqueBuiltinDeclaration
    VISIT(body);
    return node;
  }

  AstNode* VisitExternalMacroDeclaration(ExternalMacroDeclaration* node) {
    // CallableDeclaration
    VISIT(name);
    VISIT(parameters);
    VISIT(return_type);
    VISIT(labels);
    return node;
  }

  AstNode* VisitExternalBuiltinDeclaration(ExternalBuiltinDeclaration* node) {
    // CallableDeclaration
    VISIT(name);
    VISIT(parameters);
    VISIT(return_type);
    VISIT(labels);
    return node;
  }

  AstNode* VisitExternalRuntimeDeclaration(ExternalRuntimeDeclaration* node) {
    // CallableDeclaration
    VISIT(name);
    VISIT(parameters);
    VISIT(return_type);
    VISIT(labels);
    return node;
  }

  AstNode* VisitIntrinsicDeclaration(IntrinsicDeclaration* node) {
    // CallableDeclaration
    VISIT(name);
    VISIT(parameters);
    VISIT(return_type);
    VISIT(labels);
    return node;
  }

  AstNode* VisitIdentifier(Identifier* node) { return node; }

  AstNode* VisitTryHandler(TryHandler* node) {
    VISIT(label);
    VISIT(parameters);
    VISIT(body);
    return node;
  }

  AstNode* VisitClassBody(ClassBody* node) {
    VISIT(methods);
    VISIT(fields);
    return node;
  }

#undef VISIT

  Derived* d_this() { return static_cast<Derived*>(this); }
};

}  // namespace v8::internal::torque

#endif  // V8_TORQUE_AST_VISITOR_H_
