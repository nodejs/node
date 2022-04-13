"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

var _generated = require("../generated");

var _default = createTypeAnnotationBasedOnTypeof;
exports.default = _default;

function createTypeAnnotationBasedOnTypeof(type) {
  switch (type) {
    case "string":
      return (0, _generated.stringTypeAnnotation)();

    case "number":
      return (0, _generated.numberTypeAnnotation)();

    case "undefined":
      return (0, _generated.voidTypeAnnotation)();

    case "boolean":
      return (0, _generated.booleanTypeAnnotation)();

    case "function":
      return (0, _generated.genericTypeAnnotation)((0, _generated.identifier)("Function"));

    case "object":
      return (0, _generated.genericTypeAnnotation)((0, _generated.identifier)("Object"));

    case "symbol":
      return (0, _generated.genericTypeAnnotation)((0, _generated.identifier)("Symbol"));

    case "bigint":
      return (0, _generated.anyTypeAnnotation)();
  }

  throw new Error("Invalid typeof value: " + type);
}