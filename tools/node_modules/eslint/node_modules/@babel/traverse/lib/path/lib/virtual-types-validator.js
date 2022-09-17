"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.isBindingIdentifier = isBindingIdentifier;
exports.isBlockScoped = isBlockScoped;
exports.isExistentialTypeParam = isExistentialTypeParam;
exports.isExpression = isExpression;
exports.isFlow = isFlow;
exports.isForAwaitStatement = isForAwaitStatement;
exports.isGenerated = isGenerated;
exports.isNumericLiteralTypeAnnotation = isNumericLiteralTypeAnnotation;
exports.isPure = isPure;
exports.isReferenced = isReferenced;
exports.isReferencedIdentifier = isReferencedIdentifier;
exports.isReferencedMemberExpression = isReferencedMemberExpression;
exports.isRestProperty = isRestProperty;
exports.isScope = isScope;
exports.isSpreadProperty = isSpreadProperty;
exports.isStatement = isStatement;
exports.isUser = isUser;
exports.isVar = isVar;

var _t = require("@babel/types");

const {
  isBinding,
  isBlockScoped: nodeIsBlockScoped,
  isExportDeclaration,
  isExpression: nodeIsExpression,
  isFlow: nodeIsFlow,
  isForStatement,
  isForXStatement,
  isIdentifier,
  isImportDeclaration,
  isImportSpecifier,
  isJSXIdentifier,
  isJSXMemberExpression,
  isMemberExpression,
  isRestElement: nodeIsRestElement,
  isReferenced: nodeIsReferenced,
  isScope: nodeIsScope,
  isStatement: nodeIsStatement,
  isVar: nodeIsVar,
  isVariableDeclaration,
  react,
  isForOfStatement
} = _t;
const {
  isCompatTag
} = react;

function isReferencedIdentifier(opts) {
  const {
    node,
    parent
  } = this;

  if (!isIdentifier(node, opts) && !isJSXMemberExpression(parent, opts)) {
    if (isJSXIdentifier(node, opts)) {
      if (isCompatTag(node.name)) return false;
    } else {
      return false;
    }
  }

  return nodeIsReferenced(node, parent, this.parentPath.parent);
}

function isReferencedMemberExpression() {
  const {
    node,
    parent
  } = this;
  return isMemberExpression(node) && nodeIsReferenced(node, parent);
}

function isBindingIdentifier() {
  const {
    node,
    parent
  } = this;
  const grandparent = this.parentPath.parent;
  return isIdentifier(node) && isBinding(node, parent, grandparent);
}

function isStatement() {
  const {
    node,
    parent
  } = this;

  if (nodeIsStatement(node)) {
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

function isExpression() {
  if (this.isIdentifier()) {
    return this.isReferencedIdentifier();
  } else {
    return nodeIsExpression(this.node);
  }
}

function isScope() {
  return nodeIsScope(this.node, this.parent);
}

function isReferenced() {
  return nodeIsReferenced(this.node, this.parent);
}

function isBlockScoped() {
  return nodeIsBlockScoped(this.node);
}

function isVar() {
  return nodeIsVar(this.node);
}

function isUser() {
  return this.node && !!this.node.loc;
}

function isGenerated() {
  return !this.isUser();
}

function isPure(constantsOnly) {
  return this.scope.isPure(this.node, constantsOnly);
}

function isFlow() {
  const {
    node
  } = this;

  if (nodeIsFlow(node)) {
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

function isRestProperty() {
  return nodeIsRestElement(this.node) && this.parentPath && this.parentPath.isObjectPattern();
}

function isSpreadProperty() {
  return nodeIsRestElement(this.node) && this.parentPath && this.parentPath.isObjectExpression();
}

function isForAwaitStatement() {
  return isForOfStatement(this.node, {
    await: true
  });
}

function isExistentialTypeParam() {
  throw new Error("`path.isExistentialTypeParam` has been renamed to `path.isExistsTypeAnnotation()` in Babel 7.");
}

function isNumericLiteralTypeAnnotation() {
  throw new Error("`path.isNumericLiteralTypeAnnotation()` has been renamed to `path.isNumberLiteralTypeAnnotation()` in Babel 7.");
}

//# sourceMappingURL=virtual-types-validator.js.map
