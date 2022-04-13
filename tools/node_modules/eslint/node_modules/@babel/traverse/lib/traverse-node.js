"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.traverseNode = traverseNode;

var _context = require("./context");

var _t = require("@babel/types");

const {
  VISITOR_KEYS
} = _t;

function traverseNode(node, opts, scope, state, path, skipKeys) {
  const keys = VISITOR_KEYS[node.type];
  if (!keys) return false;
  const context = new _context.default(scope, opts, state, path);

  for (const key of keys) {
    if (skipKeys && skipKeys[key]) continue;

    if (context.visit(node, key)) {
      return true;
    }
  }

  return false;
}