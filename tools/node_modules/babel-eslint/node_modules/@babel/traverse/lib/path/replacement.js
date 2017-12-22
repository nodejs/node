"use strict";

exports.__esModule = true;
exports.replaceWithMultiple = replaceWithMultiple;
exports.replaceWithSourceString = replaceWithSourceString;
exports.replaceWith = replaceWith;
exports._replaceWith = _replaceWith;
exports.replaceExpressionWithStatements = replaceExpressionWithStatements;
exports.replaceInline = replaceInline;

var _codeFrame = require("@babel/code-frame");

var _index = _interopRequireDefault(require("../index"));

var _index2 = _interopRequireDefault(require("./index"));

var _babylon = require("babylon");

var t = _interopRequireWildcard(require("@babel/types"));

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

var hoistVariablesVisitor = {
  Function: function Function(path) {
    path.skip();
  },
  VariableDeclaration: function VariableDeclaration(path) {
    if (path.node.kind !== "var") return;
    var bindings = path.getBindingIdentifiers();

    for (var key in bindings) {
      path.scope.push({
        id: bindings[key]
      });
    }

    var exprs = [];
    var _arr = path.node.declarations;

    for (var _i = 0; _i < _arr.length; _i++) {
      var declar = _arr[_i];

      if (declar.init) {
        exprs.push(t.expressionStatement(t.assignmentExpression("=", declar.id, declar.init)));
      }
    }

    path.replaceWithMultiple(exprs);
  }
};

function replaceWithMultiple(nodes) {
  this.resync();
  nodes = this._verifyNodeList(nodes);
  t.inheritLeadingComments(nodes[0], this.node);
  t.inheritTrailingComments(nodes[nodes.length - 1], this.node);
  this.node = this.container[this.key] = null;
  var paths = this.insertAfter(nodes);

  if (this.node) {
    this.requeue();
  } else {
    this.remove();
  }

  return paths;
}

function replaceWithSourceString(replacement) {
  this.resync();

  try {
    replacement = "(" + replacement + ")";
    replacement = (0, _babylon.parse)(replacement);
  } catch (err) {
    var loc = err.loc;

    if (loc) {
      err.loc = null;
      err.message += " - make sure this is an expression.\n" + (0, _codeFrame.codeFrameColumns)(replacement, {
        start: {
          line: loc.line,
          column: loc.column + 1
        }
      });
    }

    throw err;
  }

  replacement = replacement.program.body[0].expression;

  _index.default.removeProperties(replacement);

  return this.replaceWith(replacement);
}

function replaceWith(replacement) {
  this.resync();

  if (this.removed) {
    throw new Error("You can't replace this node, we've already removed it");
  }

  if (replacement instanceof _index2.default) {
    replacement = replacement.node;
  }

  if (!replacement) {
    throw new Error("You passed `path.replaceWith()` a falsy node, use `path.remove()` instead");
  }

  if (this.node === replacement) {
    return [this];
  }

  if (this.isProgram() && !t.isProgram(replacement)) {
    throw new Error("You can only replace a Program root node with another Program node");
  }

  if (Array.isArray(replacement)) {
    throw new Error("Don't use `path.replaceWith()` with an array of nodes, use `path.replaceWithMultiple()`");
  }

  if (typeof replacement === "string") {
    throw new Error("Don't use `path.replaceWith()` with a source string, use `path.replaceWithSourceString()`");
  }

  var nodePath = "";

  if (this.isNodeType("Statement") && t.isExpression(replacement)) {
    if (!this.canHaveVariableDeclarationOrExpression() && !this.canSwapBetweenExpressionAndStatement(replacement) && !this.parentPath.isExportDefaultDeclaration()) {
      replacement = t.expressionStatement(replacement);
      nodePath = "expression";
    }
  }

  if (this.isNodeType("Expression") && t.isStatement(replacement)) {
    if (!this.canHaveVariableDeclarationOrExpression() && !this.canSwapBetweenExpressionAndStatement(replacement)) {
      return this.replaceExpressionWithStatements([replacement]);
    }
  }

  var oldNode = this.node;

  if (oldNode) {
    t.inheritsComments(replacement, oldNode);
    t.removeComments(oldNode);
  }

  this._replaceWith(replacement);

  this.type = replacement.type;
  this.setScope();
  this.requeue();
  return [nodePath ? this.get(nodePath) : this];
}

function _replaceWith(node) {
  if (!this.container) {
    throw new ReferenceError("Container is falsy");
  }

  if (this.inList) {
    t.validate(this.parent, this.key, [node]);
  } else {
    t.validate(this.parent, this.key, node);
  }

  this.debug("Replace with " + (node && node.type));
  this.node = this.container[this.key] = node;
}

function replaceExpressionWithStatements(nodes) {
  this.resync();
  var toSequenceExpression = t.toSequenceExpression(nodes, this.scope);

  if (toSequenceExpression) {
    return this.replaceWith(toSequenceExpression)[0].get("expressions");
  }

  var container = t.arrowFunctionExpression([], t.blockStatement(nodes));
  this.replaceWith(t.callExpression(container, []));
  this.traverse(hoistVariablesVisitor);
  var completionRecords = this.get("callee").getCompletionRecords();

  for (var _iterator = completionRecords, _isArray = Array.isArray(_iterator), _i2 = 0, _iterator = _isArray ? _iterator : _iterator[Symbol.iterator]();;) {
    var _ref;

    if (_isArray) {
      if (_i2 >= _iterator.length) break;
      _ref = _iterator[_i2++];
    } else {
      _i2 = _iterator.next();
      if (_i2.done) break;
      _ref = _i2.value;
    }

    var _path = _ref;
    if (!_path.isExpressionStatement()) continue;

    var loop = _path.findParent(function (path) {
      return path.isLoop();
    });

    if (loop) {
      var uid = loop.getData("expressionReplacementReturnUid");

      if (!uid) {
        var _callee = this.get("callee");

        uid = _callee.scope.generateDeclaredUidIdentifier("ret");

        _callee.get("body").pushContainer("body", t.returnStatement(uid));

        loop.setData("expressionReplacementReturnUid", uid);
      } else {
        uid = t.identifier(uid.name);
      }

      _path.get("expression").replaceWith(t.assignmentExpression("=", uid, _path.node.expression));
    } else {
      _path.replaceWith(t.returnStatement(_path.node.expression));
    }
  }

  var callee = this.get("callee");
  callee.arrowFunctionToExpression();
  return callee.get("body.body");
}

function replaceInline(nodes) {
  this.resync();

  if (Array.isArray(nodes)) {
    if (Array.isArray(this.container)) {
      nodes = this._verifyNodeList(nodes);

      var paths = this._containerInsertAfter(nodes);

      this.remove();
      return paths;
    } else {
      return this.replaceWithMultiple(nodes);
    }
  } else {
    return this.replaceWith(nodes);
  }
}