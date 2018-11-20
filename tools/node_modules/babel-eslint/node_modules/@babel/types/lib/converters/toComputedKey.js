"use strict";

exports.__esModule = true;
exports.default = toComputedKey;

var _generated = require("../validators/generated");

var _generated2 = require("../builders/generated");

function toComputedKey(node, key) {
  if (key === void 0) {
    key = node.key || node.property;
  }

  if (!node.computed && (0, _generated.isIdentifier)(key)) key = (0, _generated2.stringLiteral)(key.name);
  return key;
}