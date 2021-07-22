"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = hoistVariables;

var t = require("@babel/types");

const visitor = {
  Scope(path, state) {
    if (state.kind === "let") path.skip();
  },

  FunctionParent(path) {
    path.skip();
  },

  VariableDeclaration(path, state) {
    if (state.kind && path.node.kind !== state.kind) return;
    const nodes = [];
    const declarations = path.get("declarations");
    let firstId;

    for (const declar of declarations) {
      firstId = declar.node.id;

      if (declar.node.init) {
        nodes.push(t.expressionStatement(t.assignmentExpression("=", declar.node.id, declar.node.init)));
      }

      for (const name of Object.keys(declar.getBindingIdentifiers())) {
        state.emit(t.identifier(name), name, declar.node.init !== null);
      }
    }

    if (path.parentPath.isFor({
      left: path.node
    })) {
      path.replaceWith(firstId);
    } else {
      path.replaceWithMultiple(nodes);
    }
  }

};

function hoistVariables(path, emit, kind = "var") {
  path.traverse(visitor, {
    kind,
    emit
  });
}