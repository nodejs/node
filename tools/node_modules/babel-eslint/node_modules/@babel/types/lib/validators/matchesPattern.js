"use strict";

exports.__esModule = true;
exports.default = matchesPattern;

var _generated = require("./generated");

function matchesPattern(member, match, allowPartial) {
  if (!(0, _generated.isMemberExpression)(member)) return false;
  var parts = Array.isArray(match) ? match : match.split(".");
  var nodes = [];
  var node;

  for (node = member; (0, _generated.isMemberExpression)(node); node = node.object) {
    nodes.push(node.property);
  }

  nodes.push(node);
  if (nodes.length < parts.length) return false;
  if (!allowPartial && nodes.length > parts.length) return false;

  for (var i = 0, j = nodes.length - 1; i < parts.length; i++, j--) {
    var _node = nodes[j];
    var value = void 0;

    if ((0, _generated.isIdentifier)(_node)) {
      value = _node.name;
    } else if ((0, _generated.isStringLiteral)(_node)) {
      value = _node.value;
    } else {
      return false;
    }

    if (parts[i] !== value) return false;
  }

  return true;
}