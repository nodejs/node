"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = _default;

var t = require("@babel/types");

function _default(node) {
  const params = node.params;

  for (let i = 0; i < params.length; i++) {
    const param = params[i];

    if (t.isAssignmentPattern(param) || t.isRestElement(param)) {
      return i;
    }
  }

  return params.length;
}