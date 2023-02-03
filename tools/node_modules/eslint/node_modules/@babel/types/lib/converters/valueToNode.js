"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;
var _isValidIdentifier = require("../validators/isValidIdentifier");
var _generated = require("../builders/generated");
var _default = valueToNode;
exports.default = _default;
const objectToString = Function.call.bind(Object.prototype.toString);
function isRegExp(value) {
  return objectToString(value) === "[object RegExp]";
}
function isPlainObject(value) {
  if (typeof value !== "object" || value === null || Object.prototype.toString.call(value) !== "[object Object]") {
    return false;
  }
  const proto = Object.getPrototypeOf(value);
  return proto === null || Object.getPrototypeOf(proto) === null;
}
function valueToNode(value) {
  if (value === undefined) {
    return (0, _generated.identifier)("undefined");
  }

  if (value === true || value === false) {
    return (0, _generated.booleanLiteral)(value);
  }

  if (value === null) {
    return (0, _generated.nullLiteral)();
  }

  if (typeof value === "string") {
    return (0, _generated.stringLiteral)(value);
  }

  if (typeof value === "number") {
    let result;
    if (Number.isFinite(value)) {
      result = (0, _generated.numericLiteral)(Math.abs(value));
    } else {
      let numerator;
      if (Number.isNaN(value)) {
        numerator = (0, _generated.numericLiteral)(0);
      } else {
        numerator = (0, _generated.numericLiteral)(1);
      }
      result = (0, _generated.binaryExpression)("/", numerator, (0, _generated.numericLiteral)(0));
    }
    if (value < 0 || Object.is(value, -0)) {
      result = (0, _generated.unaryExpression)("-", result);
    }
    return result;
  }

  if (isRegExp(value)) {
    const pattern = value.source;
    const flags = value.toString().match(/\/([a-z]+|)$/)[1];
    return (0, _generated.regExpLiteral)(pattern, flags);
  }

  if (Array.isArray(value)) {
    return (0, _generated.arrayExpression)(value.map(valueToNode));
  }

  if (isPlainObject(value)) {
    const props = [];
    for (const key of Object.keys(value)) {
      let nodeKey;
      if ((0, _isValidIdentifier.default)(key)) {
        nodeKey = (0, _generated.identifier)(key);
      } else {
        nodeKey = (0, _generated.stringLiteral)(key);
      }
      props.push((0, _generated.objectProperty)(nodeKey, valueToNode(
      value[key])));
    }
    return (0, _generated.objectExpression)(props);
  }
  throw new Error("don't know how to turn this value into a node");
}

//# sourceMappingURL=valueToNode.js.map
