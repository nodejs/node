"use strict";

exports.__esModule = true;
exports.default = _default;

var t = _interopRequireWildcard(require("@babel/types"));

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) { var desc = Object.defineProperty && Object.getOwnPropertyDescriptor ? Object.getOwnPropertyDescriptor(obj, key) : {}; if (desc.get || desc.set) { Object.defineProperty(newObj, key, desc); } else { newObj[key] = obj[key]; } } } } newObj.default = obj; return newObj; } }

function _default(node) {
  if (!this.isReferenced()) return;
  var binding = this.scope.getBinding(node.name);

  if (binding) {
    if (binding.identifier.typeAnnotation) {
      return binding.identifier.typeAnnotation;
    } else {
      return getTypeAnnotationBindingConstantViolations(binding, this, node.name);
    }
  }

  if (node.name === "undefined") {
    return t.voidTypeAnnotation();
  } else if (node.name === "NaN" || node.name === "Infinity") {
    return t.numberTypeAnnotation();
  } else if (node.name === "arguments") {}
}

function getTypeAnnotationBindingConstantViolations(binding, path, name) {
  var types = [];
  var functionConstantViolations = [];
  var constantViolations = getConstantViolationsBefore(binding, path, functionConstantViolations);
  var testType = getConditionalAnnotation(binding, path, name);

  if (testType) {
    var testConstantViolations = getConstantViolationsBefore(binding, testType.ifStatement);
    constantViolations = constantViolations.filter(function (path) {
      return testConstantViolations.indexOf(path) < 0;
    });
    types.push(testType.typeAnnotation);
  }

  if (constantViolations.length) {
    constantViolations = constantViolations.concat(functionConstantViolations);
    var _arr = constantViolations;

    for (var _i = 0; _i < _arr.length; _i++) {
      var violation = _arr[_i];
      types.push(violation.getTypeAnnotation());
    }
  }

  if (types.length) {
    return t.createUnionTypeAnnotation(types);
  }
}

function getConstantViolationsBefore(binding, path, functions) {
  var violations = binding.constantViolations.slice();
  violations.unshift(binding.path);
  return violations.filter(function (violation) {
    violation = violation.resolve();

    var status = violation._guessExecutionStatusRelativeTo(path);

    if (functions && status === "function") functions.push(violation);
    return status === "before";
  });
}

function inferAnnotationFromBinaryExpression(name, path) {
  var operator = path.node.operator;
  var right = path.get("right").resolve();
  var left = path.get("left").resolve();
  var target;

  if (left.isIdentifier({
    name: name
  })) {
    target = right;
  } else if (right.isIdentifier({
    name: name
  })) {
    target = left;
  }

  if (target) {
    if (operator === "===") {
      return target.getTypeAnnotation();
    }

    if (t.BOOLEAN_NUMBER_BINARY_OPERATORS.indexOf(operator) >= 0) {
      return t.numberTypeAnnotation();
    }

    return;
  }

  if (operator !== "===" && operator !== "==") return;
  var typeofPath;
  var typePath;

  if (left.isUnaryExpression({
    operator: "typeof"
  })) {
    typeofPath = left;
    typePath = right;
  } else if (right.isUnaryExpression({
    operator: "typeof"
  })) {
    typeofPath = right;
    typePath = left;
  }

  if (!typeofPath) return;
  if (!typeofPath.get("argument").isIdentifier({
    name: name
  })) return;
  typePath = typePath.resolve();
  if (!typePath.isLiteral()) return;
  var typeValue = typePath.node.value;
  if (typeof typeValue !== "string") return;
  return t.createTypeAnnotationBasedOnTypeof(typeValue);
}

function getParentConditionalPath(binding, path, name) {
  var parentPath;

  while (parentPath = path.parentPath) {
    if (parentPath.isIfStatement() || parentPath.isConditionalExpression()) {
      if (path.key === "test") {
        return;
      }

      return parentPath;
    }

    if (parentPath.isFunction()) {
      if (parentPath.parentPath.scope.getBinding(name) !== binding) return;
    }

    path = parentPath;
  }
}

function getConditionalAnnotation(binding, path, name) {
  var ifStatement = getParentConditionalPath(binding, path, name);
  if (!ifStatement) return;
  var test = ifStatement.get("test");
  var paths = [test];
  var types = [];

  for (var i = 0; i < paths.length; i++) {
    var _path = paths[i];

    if (_path.isLogicalExpression()) {
      if (_path.node.operator === "&&") {
        paths.push(_path.get("left"));
        paths.push(_path.get("right"));
      }
    } else if (_path.isBinaryExpression()) {
      var type = inferAnnotationFromBinaryExpression(name, _path);
      if (type) types.push(type);
    }
  }

  if (types.length) {
    return {
      typeAnnotation: t.createUnionTypeAnnotation(types),
      ifStatement: ifStatement
    };
  }

  return getConditionalAnnotation(ifStatement, name);
}