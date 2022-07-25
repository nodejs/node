"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = isBlockScoped;

var _generated = require("./generated");

var _isLet = require("./isLet");

function isBlockScoped(node) {
  return (0, _generated.isFunctionDeclaration)(node) || (0, _generated.isClassDeclaration)(node) || (0, _isLet.default)(node);
}