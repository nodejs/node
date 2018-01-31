"use strict";

exports.__esModule = true;
exports.default = createUnionTypeAnnotation;

var _generated = require("../generated");

var _removeTypeDuplicates = _interopRequireDefault(require("../../modifications/flow/removeTypeDuplicates"));

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function createUnionTypeAnnotation(types) {
  var flattened = (0, _removeTypeDuplicates.default)(types);

  if (flattened.length === 1) {
    return flattened[0];
  } else {
    return (0, _generated.unionTypeAnnotation)(flattened);
  }
}