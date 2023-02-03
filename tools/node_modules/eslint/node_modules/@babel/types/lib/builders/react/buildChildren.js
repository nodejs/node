"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = buildChildren;
var _generated = require("../../validators/generated");
var _cleanJSXElementLiteralChild = require("../../utils/react/cleanJSXElementLiteralChild");
function buildChildren(node) {
  const elements = [];
  for (let i = 0; i < node.children.length; i++) {
    let child = node.children[i];
    if ((0, _generated.isJSXText)(child)) {
      (0, _cleanJSXElementLiteralChild.default)(child, elements);
      continue;
    }
    if ((0, _generated.isJSXExpressionContainer)(child)) child = child.expression;
    if ((0, _generated.isJSXEmptyExpression)(child)) continue;
    elements.push(child);
  }
  return elements;
}

//# sourceMappingURL=buildChildren.js.map
