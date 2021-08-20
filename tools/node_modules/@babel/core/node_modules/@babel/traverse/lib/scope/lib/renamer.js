"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

var _binding = require("../binding");

var _helperSplitExportDeclaration = require("@babel/helper-split-export-declaration");

var t = require("@babel/types");

const renameVisitor = {
  ReferencedIdentifier({
    node
  }, state) {
    if (node.name === state.oldName) {
      node.name = state.newName;
    }
  },

  Scope(path, state) {
    if (!path.scope.bindingIdentifierEquals(state.oldName, state.binding.identifier)) {
      skipAllButComputedMethodKey(path);
    }
  },

  "AssignmentExpression|Declaration|VariableDeclarator"(path, state) {
    if (path.isVariableDeclaration()) return;
    const ids = path.getOuterBindingIdentifiers();

    for (const name in ids) {
      if (name === state.oldName) ids[name].name = state.newName;
    }
  }

};

class Renamer {
  constructor(binding, oldName, newName) {
    this.newName = newName;
    this.oldName = oldName;
    this.binding = binding;
  }

  maybeConvertFromExportDeclaration(parentDeclar) {
    const maybeExportDeclar = parentDeclar.parentPath;

    if (!maybeExportDeclar.isExportDeclaration()) {
      return;
    }

    if (maybeExportDeclar.isExportDefaultDeclaration() && !maybeExportDeclar.get("declaration").node.id) {
      return;
    }

    (0, _helperSplitExportDeclaration.default)(maybeExportDeclar);
  }

  maybeConvertFromClassFunctionDeclaration(path) {
    return;
    if (!path.isFunctionDeclaration() && !path.isClassDeclaration()) return;
    if (this.binding.kind !== "hoisted") return;
    path.node.id = t.identifier(this.oldName);
    path.node._blockHoist = 3;
    path.replaceWith(t.variableDeclaration("let", [t.variableDeclarator(t.identifier(this.newName), t.toExpression(path.node))]));
  }

  maybeConvertFromClassFunctionExpression(path) {
    return;
    if (!path.isFunctionExpression() && !path.isClassExpression()) return;
    if (this.binding.kind !== "local") return;
    path.node.id = t.identifier(this.oldName);
    this.binding.scope.parent.push({
      id: t.identifier(this.newName)
    });
    path.replaceWith(t.assignmentExpression("=", t.identifier(this.newName), path.node));
  }

  rename(block) {
    const {
      binding,
      oldName,
      newName
    } = this;
    const {
      scope,
      path
    } = binding;
    const parentDeclar = path.find(path => path.isDeclaration() || path.isFunctionExpression() || path.isClassExpression());

    if (parentDeclar) {
      const bindingIds = parentDeclar.getOuterBindingIdentifiers();

      if (bindingIds[oldName] === binding.identifier) {
        this.maybeConvertFromExportDeclaration(parentDeclar);
      }
    }

    const blockToTraverse = block || scope.block;

    if ((blockToTraverse == null ? void 0 : blockToTraverse.type) === "SwitchStatement") {
      blockToTraverse.cases.forEach(c => {
        scope.traverse(c, renameVisitor, this);
      });
    } else {
      scope.traverse(blockToTraverse, renameVisitor, this);
    }

    if (!block) {
      scope.removeOwnBinding(oldName);
      scope.bindings[newName] = binding;
      this.binding.identifier.name = newName;
    }

    if (parentDeclar) {
      this.maybeConvertFromClassFunctionDeclaration(parentDeclar);
      this.maybeConvertFromClassFunctionExpression(parentDeclar);
    }
  }

}

exports.default = Renamer;

function skipAllButComputedMethodKey(path) {
  if (!path.isMethod() || !path.node.computed) {
    path.skip();
    return;
  }

  const keys = t.VISITOR_KEYS[path.type];

  for (const key of keys) {
    if (key !== "key") path.skipKey(key);
  }
}