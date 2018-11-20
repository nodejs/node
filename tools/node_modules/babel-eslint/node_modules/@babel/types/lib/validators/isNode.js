"use strict";

exports.__esModule = true;
exports.default = isNode;

var _definitions = require("../definitions");

function isNode(node) {
  return !!(node && _definitions.VISITOR_KEYS[node.type]);
}