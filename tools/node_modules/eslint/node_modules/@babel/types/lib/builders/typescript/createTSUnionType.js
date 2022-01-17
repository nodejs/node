"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = createTSUnionType;

var _generated = require("../generated");

var _removeTypeDuplicates = require("../../modifications/typescript/removeTypeDuplicates");

function createTSUnionType(typeAnnotations) {
  const types = typeAnnotations.map(type => type.typeAnnotation);
  const flattened = (0, _removeTypeDuplicates.default)(types);

  if (flattened.length === 1) {
    return flattened[0];
  } else {
    return (0, _generated.tsUnionType)(flattened);
  }
}