"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.Var = exports.User = exports.Statement = exports.SpreadProperty = exports.Scope = exports.RestProperty = exports.ReferencedMemberExpression = exports.ReferencedIdentifier = exports.Referenced = exports.Pure = exports.NumericLiteralTypeAnnotation = exports.Generated = exports.ForAwaitStatement = exports.Flow = exports.Expression = exports.ExistentialTypeParam = exports.BlockScoped = exports.BindingIdentifier = void 0;

var _t = require("@babel/types");

const {
  isBinding,
  isBlockScoped,
  isExportDeclaration,
  isExpression,
  isFlow,
  isForStatement,
  isForXStatement,
  isIdentifier,
  isImportDeclaration,
  isImportSpecifier,
  isJSXIdentifier,
  isJSXMemberExpression,
  isMemberExpression,
  isReferenced,
  isScope,
  isStatement,
  isVar,
  isVariableDeclaration,
  react
} = _t;
const {
  isCompatTag
} = react;
const ReferencedIdentifier = {
  types: ["Identifier", "JSXIdentifier"],

  checkPath(path, opts) {
    const {
      node,
      parent
    } = path;

    if (!isIdentifier(node, opts) && !isJSXMemberExpression(parent, opts)) {
      if (isJSXIdentifier(node, opts)) {
        if (isCompatTag(node.name)) return false;
      } else {
        return false;
      }
    }

    return isReferenced(node, parent, path.parentPath.parent);
  }

};
exports.ReferencedIdentifier = ReferencedIdentifier;
const ReferencedMemberExpression = {
  types: ["MemberExpression"],

  checkPath({
    node,
    parent
  }) {
    return isMemberExpression(node) && isReferenced(node, parent);
  }

};
exports.ReferencedMemberExpression = ReferencedMemberExpression;
const BindingIdentifier = {
  types: ["Identifier"],

  checkPath(path) {
    const {
      node,
      parent
    } = path;
    const grandparent = path.parentPath.parent;
    return isIdentifier(node) && isBinding(node, parent, grandparent);
  }

};
exports.BindingIdentifier = BindingIdentifier;
const Statement = {
  types: ["Statement"],

  checkPath({
    node,
    parent
  }) {
    if (isStatement(node)) {
      if (isVariableDeclaration(node)) {
        if (isForXStatement(parent, {
          left: node
        })) return false;
        if (isForStatement(parent, {
          init: node
        })) return false;
      }

      return true;
    } else {
      return false;
    }
  }

};
exports.Statement = Statement;
const Expression = {
  types: ["Expression"],

  checkPath(path) {
    if (path.isIdentifier()) {
      return path.isReferencedIdentifier();
    } else {
      return isExpression(path.node);
    }
  }

};
exports.Expression = Expression;
const Scope = {
  types: ["Scopable", "Pattern"],

  checkPath(path) {
    return isScope(path.node, path.parent);
  }

};
exports.Scope = Scope;
const Referenced = {
  checkPath(path) {
    return isReferenced(path.node, path.parent);
  }

};
exports.Referenced = Referenced;
const BlockScoped = {
  checkPath(path) {
    return isBlockScoped(path.node);
  }

};
exports.BlockScoped = BlockScoped;
const Var = {
  types: ["VariableDeclaration"],

  checkPath(path) {
    return isVar(path.node);
  }

};
exports.Var = Var;
const User = {
  checkPath(path) {
    return path.node && !!path.node.loc;
  }

};
exports.User = User;
const Generated = {
  checkPath(path) {
    return !path.isUser();
  }

};
exports.Generated = Generated;
const Pure = {
  checkPath(path, constantsOnly) {
    return path.scope.isPure(path.node, constantsOnly);
  }

};
exports.Pure = Pure;
const Flow = {
  types: ["Flow", "ImportDeclaration", "ExportDeclaration", "ImportSpecifier"],

  checkPath({
    node
  }) {
    if (isFlow(node)) {
      return true;
    } else if (isImportDeclaration(node)) {
      return node.importKind === "type" || node.importKind === "typeof";
    } else if (isExportDeclaration(node)) {
      return node.exportKind === "type";
    } else if (isImportSpecifier(node)) {
      return node.importKind === "type" || node.importKind === "typeof";
    } else {
      return false;
    }
  }

};
exports.Flow = Flow;
const RestProperty = {
  types: ["RestElement"],

  checkPath(path) {
    return path.parentPath && path.parentPath.isObjectPattern();
  }

};
exports.RestProperty = RestProperty;
const SpreadProperty = {
  types: ["RestElement"],

  checkPath(path) {
    return path.parentPath && path.parentPath.isObjectExpression();
  }

};
exports.SpreadProperty = SpreadProperty;
const ExistentialTypeParam = {
  types: ["ExistsTypeAnnotation"]
};
exports.ExistentialTypeParam = ExistentialTypeParam;
const NumericLiteralTypeAnnotation = {
  types: ["NumberLiteralTypeAnnotation"]
};
exports.NumericLiteralTypeAnnotation = NumericLiteralTypeAnnotation;
const ForAwaitStatement = {
  types: ["ForOfStatement"],

  checkPath({
    node
  }) {
    return node.await === true;
  }

};
exports.ForAwaitStatement = ForAwaitStatement;