"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;
exports.skipAllButComputedKey = skipAllButComputedKey;

var _t = require("@babel/types");

const {
  VISITOR_KEYS,
  staticBlock
} = _t;

function skipAllButComputedKey(path) {
  if (!path.node.computed) {
    path.skip();
    return;
  }

  const keys = VISITOR_KEYS[path.type];

  for (const key of keys) {
    if (key !== "key") path.skipKey(key);
  }
}

const skipKey = (staticBlock ? "StaticBlock|" : "") + "ClassPrivateProperty|TypeAnnotation|FunctionDeclaration|FunctionExpression";
var _default = {
  [skipKey]: path => path.skip(),

  "Method|ClassProperty"(path) {
    skipAllButComputedKey(path);
  }

};
exports.default = _default;