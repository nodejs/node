"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.buildDynamicImport = buildDynamicImport;
var t = require("@babel/types");
var _template = require("@babel/template");
{
  {
    {
      exports.getDynamicImportSource = function getDynamicImportSource(node) {
        const [source] = node.arguments;
        return t.isStringLiteral(source) || t.isTemplateLiteral(source) ? source : _template.default.expression.ast`\`\${${source}}\``;
      };
    }
  }
}
function buildDynamicImport(node, deferToThen, wrapWithPromise, builder) {
  const [specifier] = node.arguments;
  if (t.isStringLiteral(specifier) || t.isTemplateLiteral(specifier) && specifier.quasis.length === 0) {
    if (deferToThen) {
      return _template.default.expression.ast`
        Promise.resolve().then(() => ${builder(specifier)})
      `;
    } else return builder(specifier);
  }
  const specifierToString = t.isTemplateLiteral(specifier) ? t.identifier("specifier") : t.templateLiteral([t.templateElement({
    raw: ""
  }), t.templateElement({
    raw: ""
  })], [t.identifier("specifier")]);
  if (deferToThen) {
    return _template.default.expression.ast`
      (specifier =>
        new Promise(r => r(${specifierToString}))
          .then(s => ${builder(t.identifier("s"))})
      )(${specifier})
    `;
  } else if (wrapWithPromise) {
    return _template.default.expression.ast`
      (specifier =>
        new Promise(r => r(${builder(specifierToString)}))
      )(${specifier})
    `;
  } else {
    return _template.default.expression.ast`
      (specifier => ${builder(specifierToString)})(${specifier})
    `;
  }
}

//# sourceMappingURL=dynamic-import.js.map
