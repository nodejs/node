"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = isScope;

var _generated = require("./generated");

function isScope(node, parent) {
  if ((0, _generated.isBlockStatement)(node) && ((0, _generated.isFunction)(parent) || (0, _generated.isCatchClause)(parent))) {
    return false;
  }

  if ((0, _generated.isPattern)(node) && ((0, _generated.isFunction)(parent) || (0, _generated.isCatchClause)(parent))) {
    return true;
  }

  return (0, _generated.isScopable)(node);
}