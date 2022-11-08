"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.getDynamicImportSource = getDynamicImportSource;
var t = require("@babel/types");
var _template = require("@babel/template");

function getDynamicImportSource(node) {
  const [source] = node.arguments;
  return t.isStringLiteral(source) || t.isTemplateLiteral(source) ? source : _template.default.expression.ast`\`\${${source}}\``;
}

//# sourceMappingURL=dynamic-import.js.map
