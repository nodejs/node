"use strict";

exports.__esModule = true;
exports.evaluateTruthy = evaluateTruthy;
exports.evaluate = evaluate;
var VALID_CALLEES = ["String", "Number", "Math"];
var INVALID_METHODS = ["random"];

function evaluateTruthy() {
  var res = this.evaluate();
  if (res.confident) return !!res.value;
}

function deopt(path, state) {
  if (!state.confident) return;
  state.deoptPath = path;
  state.confident = false;
}

function evaluateCached(path, state) {
  var node = path.node;
  var seen = state.seen;

  if (seen.has(node)) {
    var existing = seen.get(node);

    if (existing.resolved) {
      return existing.value;
    } else {
      deopt(path, state);
      return;
    }
  } else {
    var item = {
      resolved: false
    };
    seen.set(node, item);

    var val = _evaluate(path, state);

    if (state.confident) {
      item.resolved = true;
      item.value = val;
    }

    return val;
  }
}

function _evaluate(path, state) {
  if (!state.confident) return;
  var node = path.node;

  if (path.isSequenceExpression()) {
    var exprs = path.get("expressions");
    return evaluateCached(exprs[exprs.length - 1], state);
  }

  if (path.isStringLiteral() || path.isNumericLiteral() || path.isBooleanLiteral()) {
    return node.value;
  }

  if (path.isNullLiteral()) {
    return null;
  }

  if (path.isTemplateLiteral()) {
    return evaluateQuasis(path, node.quasis, state);
  }

  if (path.isTaggedTemplateExpression() && path.get("tag").isMemberExpression()) {
    var object = path.get("tag.object");
    var name = object.node.name;
    var property = path.get("tag.property");

    if (object.isIdentifier() && name === "String" && !path.scope.getBinding(name, true) && property.isIdentifier && property.node.name === "raw") {
      return evaluateQuasis(path, node.quasi.quasis, state, true);
    }
  }

  if (path.isConditionalExpression()) {
    var testResult = evaluateCached(path.get("test"), state);
    if (!state.confident) return;

    if (testResult) {
      return evaluateCached(path.get("consequent"), state);
    } else {
      return evaluateCached(path.get("alternate"), state);
    }
  }

  if (path.isExpressionWrapper()) {
    return evaluateCached(path.get("expression"), state);
  }

  if (path.isMemberExpression() && !path.parentPath.isCallExpression({
    callee: node
  })) {
    var _property = path.get("property");

    var _object = path.get("object");

    if (_object.isLiteral() && _property.isIdentifier()) {
      var value = _object.node.value;
      var type = typeof value;

      if (type === "number" || type === "string") {
        return value[_property.node.name];
      }
    }
  }

  if (path.isReferencedIdentifier()) {
    var binding = path.scope.getBinding(node.name);

    if (binding && binding.constantViolations.length > 0) {
      return deopt(binding.path, state);
    }

    if (binding && path.node.start < binding.path.node.end) {
      return deopt(binding.path, state);
    }

    if (binding && binding.hasValue) {
      return binding.value;
    } else {
      if (node.name === "undefined") {
        return binding ? deopt(binding.path, state) : undefined;
      } else if (node.name === "Infinity") {
        return binding ? deopt(binding.path, state) : Infinity;
      } else if (node.name === "NaN") {
        return binding ? deopt(binding.path, state) : NaN;
      }

      var resolved = path.resolve();

      if (resolved === path) {
        return deopt(path, state);
      } else {
        return evaluateCached(resolved, state);
      }
    }
  }

  if (path.isUnaryExpression({
    prefix: true
  })) {
    if (node.operator === "void") {
      return undefined;
    }

    var argument = path.get("argument");

    if (node.operator === "typeof" && (argument.isFunction() || argument.isClass())) {
      return "function";
    }

    var arg = evaluateCached(argument, state);
    if (!state.confident) return;

    switch (node.operator) {
      case "!":
        return !arg;

      case "+":
        return +arg;

      case "-":
        return -arg;

      case "~":
        return ~arg;

      case "typeof":
        return typeof arg;
    }
  }

  if (path.isArrayExpression()) {
    var arr = [];
    var elems = path.get("elements");

    for (var _iterator = elems, _isArray = Array.isArray(_iterator), _i = 0, _iterator = _isArray ? _iterator : _iterator[Symbol.iterator]();;) {
      var _ref;

      if (_isArray) {
        if (_i >= _iterator.length) break;
        _ref = _iterator[_i++];
      } else {
        _i = _iterator.next();
        if (_i.done) break;
        _ref = _i.value;
      }

      var _elem = _ref;

      var elemValue = _elem.evaluate();

      if (elemValue.confident) {
        arr.push(elemValue.value);
      } else {
        return deopt(_elem, state);
      }
    }

    return arr;
  }

  if (path.isObjectExpression()) {
    var obj = {};
    var props = path.get("properties");

    for (var _iterator2 = props, _isArray2 = Array.isArray(_iterator2), _i2 = 0, _iterator2 = _isArray2 ? _iterator2 : _iterator2[Symbol.iterator]();;) {
      var _ref2;

      if (_isArray2) {
        if (_i2 >= _iterator2.length) break;
        _ref2 = _iterator2[_i2++];
      } else {
        _i2 = _iterator2.next();
        if (_i2.done) break;
        _ref2 = _i2.value;
      }

      var _prop = _ref2;

      if (_prop.isObjectMethod() || _prop.isSpreadElement()) {
        return deopt(_prop, state);
      }

      var keyPath = _prop.get("key");

      var key = keyPath;

      if (_prop.node.computed) {
        key = key.evaluate();

        if (!key.confident) {
          return deopt(keyPath, state);
        }

        key = key.value;
      } else if (key.isIdentifier()) {
        key = key.node.name;
      } else {
        key = key.node.value;
      }

      var valuePath = _prop.get("value");

      var _value2 = valuePath.evaluate();

      if (!_value2.confident) {
        return deopt(valuePath, state);
      }

      _value2 = _value2.value;
      obj[key] = _value2;
    }

    return obj;
  }

  if (path.isLogicalExpression()) {
    var wasConfident = state.confident;
    var left = evaluateCached(path.get("left"), state);
    var leftConfident = state.confident;
    state.confident = wasConfident;
    var right = evaluateCached(path.get("right"), state);
    var rightConfident = state.confident;
    state.confident = leftConfident && rightConfident;

    switch (node.operator) {
      case "||":
        if (left && leftConfident) {
          state.confident = true;
          return left;
        }

        if (!state.confident) return;
        return left || right;

      case "&&":
        if (!left && leftConfident || !right && rightConfident) {
          state.confident = true;
        }

        if (!state.confident) return;
        return left && right;
    }
  }

  if (path.isBinaryExpression()) {
    var _left = evaluateCached(path.get("left"), state);

    if (!state.confident) return;

    var _right = evaluateCached(path.get("right"), state);

    if (!state.confident) return;

    switch (node.operator) {
      case "-":
        return _left - _right;

      case "+":
        return _left + _right;

      case "/":
        return _left / _right;

      case "*":
        return _left * _right;

      case "%":
        return _left % _right;

      case "**":
        return Math.pow(_left, _right);

      case "<":
        return _left < _right;

      case ">":
        return _left > _right;

      case "<=":
        return _left <= _right;

      case ">=":
        return _left >= _right;

      case "==":
        return _left == _right;

      case "!=":
        return _left != _right;

      case "===":
        return _left === _right;

      case "!==":
        return _left !== _right;

      case "|":
        return _left | _right;

      case "&":
        return _left & _right;

      case "^":
        return _left ^ _right;

      case "<<":
        return _left << _right;

      case ">>":
        return _left >> _right;

      case ">>>":
        return _left >>> _right;
    }
  }

  if (path.isCallExpression()) {
    var callee = path.get("callee");
    var context;
    var func;

    if (callee.isIdentifier() && !path.scope.getBinding(callee.node.name, true) && VALID_CALLEES.indexOf(callee.node.name) >= 0) {
      func = global[node.callee.name];
    }

    if (callee.isMemberExpression()) {
      var _object2 = callee.get("object");

      var _property2 = callee.get("property");

      if (_object2.isIdentifier() && _property2.isIdentifier() && VALID_CALLEES.indexOf(_object2.node.name) >= 0 && INVALID_METHODS.indexOf(_property2.node.name) < 0) {
        context = global[_object2.node.name];
        func = context[_property2.node.name];
      }

      if (_object2.isLiteral() && _property2.isIdentifier()) {
        var _type = typeof _object2.node.value;

        if (_type === "string" || _type === "number") {
          context = _object2.node.value;
          func = context[_property2.node.name];
        }
      }
    }

    if (func) {
      var args = path.get("arguments").map(function (arg) {
        return evaluateCached(arg, state);
      });
      if (!state.confident) return;
      return func.apply(context, args);
    }
  }

  deopt(path, state);
}

function evaluateQuasis(path, quasis, state, raw) {
  if (raw === void 0) {
    raw = false;
  }

  var str = "";
  var i = 0;
  var exprs = path.get("expressions");

  for (var _iterator3 = quasis, _isArray3 = Array.isArray(_iterator3), _i3 = 0, _iterator3 = _isArray3 ? _iterator3 : _iterator3[Symbol.iterator]();;) {
    var _ref3;

    if (_isArray3) {
      if (_i3 >= _iterator3.length) break;
      _ref3 = _iterator3[_i3++];
    } else {
      _i3 = _iterator3.next();
      if (_i3.done) break;
      _ref3 = _i3.value;
    }

    var _elem2 = _ref3;
    if (!state.confident) break;
    str += raw ? _elem2.value.raw : _elem2.value.cooked;
    var expr = exprs[i++];
    if (expr) str += String(evaluateCached(expr, state));
  }

  if (!state.confident) return;
  return str;
}

function evaluate() {
  var state = {
    confident: true,
    deoptPath: null,
    seen: new Map()
  };
  var value = evaluateCached(this, state);
  if (!state.confident) value = undefined;
  return {
    confident: state.confident,
    deopt: state.deoptPath,
    value: value
  };
}