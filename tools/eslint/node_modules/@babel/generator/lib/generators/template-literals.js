"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.TaggedTemplateExpression = TaggedTemplateExpression;
exports.TemplateElement = TemplateElement;
exports.TemplateLiteral = TemplateLiteral;
function TaggedTemplateExpression(node) {
  this.print(node.tag);
  this.print(node.typeParameters);
  this.print(node.quasi);
}
function TemplateElement() {
  throw new Error("TemplateElement printing is handled in TemplateLiteral");
}
function TemplateLiteral(node) {
  const quasis = node.quasis;
  let partRaw = "`";
  for (let i = 0; i < quasis.length; i++) {
    partRaw += quasis[i].value.raw;
    if (i + 1 < quasis.length) {
      this.token(partRaw + "${", true);
      this.print(node.expressions[i]);
      partRaw = "}";
    }
  }
  this.token(partRaw + "`", true);
}

//# sourceMappingURL=template-literals.js.map
