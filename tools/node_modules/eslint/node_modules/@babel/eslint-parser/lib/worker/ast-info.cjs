const ESLINT_VISITOR_KEYS = require("eslint-visitor-keys").KEYS;
const babel = require("./babel-core.cjs");
let visitorKeys;
exports.getVisitorKeys = function getVisitorKeys() {
  if (!visitorKeys) {
    const newTypes = {
      ChainExpression: ESLINT_VISITOR_KEYS.ChainExpression,
      ImportExpression: ESLINT_VISITOR_KEYS.ImportExpression,
      Literal: ESLINT_VISITOR_KEYS.Literal,
      MethodDefinition: ["decorators"].concat(ESLINT_VISITOR_KEYS.MethodDefinition),
      Property: ["decorators"].concat(ESLINT_VISITOR_KEYS.Property),
      PropertyDefinition: ["decorators", "typeAnnotation"].concat(ESLINT_VISITOR_KEYS.PropertyDefinition)
    };
    const conflictTypes = {
      ClassPrivateMethod: ["decorators"].concat(ESLINT_VISITOR_KEYS.MethodDefinition),
      ExportAllDeclaration: ESLINT_VISITOR_KEYS.ExportAllDeclaration
    };
    visitorKeys = Object.assign({}, newTypes, babel.types.VISITOR_KEYS, conflictTypes);
  }
  return visitorKeys;
};
let tokLabels;
exports.getTokLabels = function getTokLabels() {
  return tokLabels || (tokLabels = (p => p.reduce((o, [k, v]) => Object.assign({}, o, {
    [k]: v
  }), {}))(Object.entries(babel.tokTypes).map(([key, tok]) => [key, tok.label])));
};

//# sourceMappingURL=ast-info.cjs.map
