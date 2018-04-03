"use strict";

exports.__esModule = true;
exports.default = gatherSequenceExpressions;

var _getBindingIdentifiers = _interopRequireDefault(require("../retrievers/getBindingIdentifiers"));

var _generated = require("../validators/generated");

var _generated2 = require("../builders/generated");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function gatherSequenceExpressions(nodes, scope, declars) {
  var exprs = [];
  var ensureLastUndefined = true;

  for (var _iterator = nodes, _isArray = Array.isArray(_iterator), _i = 0, _iterator = _isArray ? _iterator : _iterator[Symbol.iterator]();;) {
    var _ref;

    if (_isArray) {
      if (_i >= _iterator.length) break;
      _ref = _iterator[_i++];
    } else {
      _i = _iterator.next();
      if (_i.done) break;
      _ref = _i.value;
    }

    var _node = _ref;
    ensureLastUndefined = false;

    if ((0, _generated.isExpression)(_node)) {
      exprs.push(_node);
    } else if ((0, _generated.isExpressionStatement)(_node)) {
      exprs.push(_node.expression);
    } else if ((0, _generated.isVariableDeclaration)(_node)) {
      if (_node.kind !== "var") return;
      var _arr = _node.declarations;

      for (var _i2 = 0; _i2 < _arr.length; _i2++) {
        var declar = _arr[_i2];
        var bindings = (0, _getBindingIdentifiers.default)(declar);

        for (var key in bindings) {
          declars.push({
            kind: _node.kind,
            id: bindings[key]
          });
        }

        if (declar.init) {
          exprs.push((0, _generated2.assignmentExpression)("=", declar.id, declar.init));
        }
      }

      ensureLastUndefined = true;
    } else if ((0, _generated.isIfStatement)(_node)) {
      var consequent = _node.consequent ? gatherSequenceExpressions([_node.consequent], scope, declars) : scope.buildUndefinedNode();
      var alternate = _node.alternate ? gatherSequenceExpressions([_node.alternate], scope, declars) : scope.buildUndefinedNode();
      if (!consequent || !alternate) return;
      exprs.push((0, _generated2.conditionalExpression)(_node.test, consequent, alternate));
    } else if ((0, _generated.isBlockStatement)(_node)) {
      var body = gatherSequenceExpressions(_node.body, scope, declars);
      if (!body) return;
      exprs.push(body);
    } else if ((0, _generated.isEmptyStatement)(_node)) {
      ensureLastUndefined = true;
    } else {
      return;
    }
  }

  if (ensureLastUndefined) {
    exprs.push(scope.buildUndefinedNode());
  }

  if (exprs.length === 1) {
    return exprs[0];
  } else {
    return (0, _generated2.sequenceExpression)(exprs);
  }
}