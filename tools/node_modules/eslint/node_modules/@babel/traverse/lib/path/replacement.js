"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports._replaceWith = _replaceWith;
exports.replaceExpressionWithStatements = replaceExpressionWithStatements;
exports.replaceInline = replaceInline;
exports.replaceWith = replaceWith;
exports.replaceWithMultiple = replaceWithMultiple;
exports.replaceWithSourceString = replaceWithSourceString;

var _codeFrame = require("@babel/code-frame");

var _index = require("../index");

var _index2 = require("./index");

var _cache = require("../cache");

var _parser = require("@babel/parser");

var _t = require("@babel/types");

var _helperHoistVariables = require("@babel/helper-hoist-variables");

const {
  FUNCTION_TYPES,
  arrowFunctionExpression,
  assignmentExpression,
  awaitExpression,
  blockStatement,
  callExpression,
  cloneNode,
  expressionStatement,
  identifier,
  inheritLeadingComments,
  inheritTrailingComments,
  inheritsComments,
  isExpression,
  isProgram,
  isStatement,
  removeComments,
  returnStatement,
  toSequenceExpression,
  validate,
  yieldExpression
} = _t;

function replaceWithMultiple(nodes) {
  var _pathCache$get;

  this.resync();
  nodes = this._verifyNodeList(nodes);
  inheritLeadingComments(nodes[0], this.node);
  inheritTrailingComments(nodes[nodes.length - 1], this.node);
  (_pathCache$get = _cache.path.get(this.parent)) == null ? void 0 : _pathCache$get.delete(this.node);
  this.node = this.container[this.key] = null;
  const paths = this.insertAfter(nodes);

  if (this.node) {
    this.requeue();
  } else {
    this.remove();
  }

  return paths;
}

function replaceWithSourceString(replacement) {
  this.resync();
  let ast;

  try {
    replacement = `(${replacement})`;
    ast = (0, _parser.parse)(replacement);
  } catch (err) {
    const loc = err.loc;

    if (loc) {
      err.message += " - make sure this is an expression.\n" + (0, _codeFrame.codeFrameColumns)(replacement, {
        start: {
          line: loc.line,
          column: loc.column + 1
        }
      });
      err.code = "BABEL_REPLACE_SOURCE_ERROR";
    }

    throw err;
  }

  const expressionAST = ast.program.body[0].expression;

  _index.default.removeProperties(expressionAST);

  return this.replaceWith(expressionAST);
}

function replaceWith(replacementPath) {
  this.resync();

  if (this.removed) {
    throw new Error("You can't replace this node, we've already removed it");
  }

  let replacement = replacementPath instanceof _index2.default ? replacementPath.node : replacementPath;

  if (!replacement) {
    throw new Error("You passed `path.replaceWith()` a falsy node, use `path.remove()` instead");
  }

  if (this.node === replacement) {
    return [this];
  }

  if (this.isProgram() && !isProgram(replacement)) {
    throw new Error("You can only replace a Program root node with another Program node");
  }

  if (Array.isArray(replacement)) {
    throw new Error("Don't use `path.replaceWith()` with an array of nodes, use `path.replaceWithMultiple()`");
  }

  if (typeof replacement === "string") {
    throw new Error("Don't use `path.replaceWith()` with a source string, use `path.replaceWithSourceString()`");
  }

  let nodePath = "";

  if (this.isNodeType("Statement") && isExpression(replacement)) {
    if (!this.canHaveVariableDeclarationOrExpression() && !this.canSwapBetweenExpressionAndStatement(replacement) && !this.parentPath.isExportDefaultDeclaration()) {
      replacement = expressionStatement(replacement);
      nodePath = "expression";
    }
  }

  if (this.isNodeType("Expression") && isStatement(replacement)) {
    if (!this.canHaveVariableDeclarationOrExpression() && !this.canSwapBetweenExpressionAndStatement(replacement)) {
      return this.replaceExpressionWithStatements([replacement]);
    }
  }

  const oldNode = this.node;

  if (oldNode) {
    inheritsComments(replacement, oldNode);
    removeComments(oldNode);
  }

  this._replaceWith(replacement);

  this.type = replacement.type;
  this.setScope();
  this.requeue();
  return [nodePath ? this.get(nodePath) : this];
}

function _replaceWith(node) {
  var _pathCache$get2;

  if (!this.container) {
    throw new ReferenceError("Container is falsy");
  }

  if (this.inList) {
    validate(this.parent, this.key, [node]);
  } else {
    validate(this.parent, this.key, node);
  }

  this.debug(`Replace with ${node == null ? void 0 : node.type}`);
  (_pathCache$get2 = _cache.path.get(this.parent)) == null ? void 0 : _pathCache$get2.set(node, this).delete(this.node);
  this.node = this.container[this.key] = node;
}

function replaceExpressionWithStatements(nodes) {
  this.resync();
  const nodesAsSequenceExpression = toSequenceExpression(nodes, this.scope);

  if (nodesAsSequenceExpression) {
    return this.replaceWith(nodesAsSequenceExpression)[0].get("expressions");
  }

  const functionParent = this.getFunctionParent();
  const isParentAsync = functionParent == null ? void 0 : functionParent.is("async");
  const isParentGenerator = functionParent == null ? void 0 : functionParent.is("generator");
  const container = arrowFunctionExpression([], blockStatement(nodes));
  this.replaceWith(callExpression(container, []));
  const callee = this.get("callee");
  (0, _helperHoistVariables.default)(callee.get("body"), id => {
    this.scope.push({
      id
    });
  }, "var");
  const completionRecords = this.get("callee").getCompletionRecords();

  for (const path of completionRecords) {
    if (!path.isExpressionStatement()) continue;
    const loop = path.findParent(path => path.isLoop());

    if (loop) {
      let uid = loop.getData("expressionReplacementReturnUid");

      if (!uid) {
        uid = callee.scope.generateDeclaredUidIdentifier("ret");
        callee.get("body").pushContainer("body", returnStatement(cloneNode(uid)));
        loop.setData("expressionReplacementReturnUid", uid);
      } else {
        uid = identifier(uid.name);
      }

      path.get("expression").replaceWith(assignmentExpression("=", cloneNode(uid), path.node.expression));
    } else {
      path.replaceWith(returnStatement(path.node.expression));
    }
  }

  callee.arrowFunctionToExpression();
  const newCallee = callee;

  const needToAwaitFunction = isParentAsync && _index.default.hasType(this.get("callee.body").node, "AwaitExpression", FUNCTION_TYPES);

  const needToYieldFunction = isParentGenerator && _index.default.hasType(this.get("callee.body").node, "YieldExpression", FUNCTION_TYPES);

  if (needToAwaitFunction) {
    newCallee.set("async", true);

    if (!needToYieldFunction) {
      this.replaceWith(awaitExpression(this.node));
    }
  }

  if (needToYieldFunction) {
    newCallee.set("generator", true);
    this.replaceWith(yieldExpression(this.node, true));
  }

  return newCallee.get("body.body");
}

function replaceInline(nodes) {
  this.resync();

  if (Array.isArray(nodes)) {
    if (Array.isArray(this.container)) {
      nodes = this._verifyNodeList(nodes);

      const paths = this._containerInsertAfter(nodes);

      this.remove();
      return paths;
    } else {
      return this.replaceWithMultiple(nodes);
    }
  } else {
    return this.replaceWith(nodes);
  }
}

//# sourceMappingURL=replacement.js.map
