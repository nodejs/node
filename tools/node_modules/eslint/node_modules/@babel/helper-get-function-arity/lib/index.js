"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = _default;

var _t = require("@babel/types");

const {
  isAssignmentPattern,
  isRestElement
} = _t;

function _default(node) {
  const params = node.params;

  for (let i = 0; i < params.length; i++) {
    const param = params[i];

    if (isAssignmentPattern(param) || isRestElement(param)) {
      return i;
    }
  }

  return params.length;
}