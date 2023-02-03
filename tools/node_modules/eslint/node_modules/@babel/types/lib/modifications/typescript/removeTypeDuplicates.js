"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = removeTypeDuplicates;
var _generated = require("../../validators/generated");
function getQualifiedName(node) {
  return (0, _generated.isIdentifier)(node) ? node.name : `${node.right.name}.${getQualifiedName(node.left)}`;
}

function removeTypeDuplicates(nodes) {
  const generics = new Map();
  const bases = new Map();

  const typeGroups = new Set();
  const types = [];
  for (let i = 0; i < nodes.length; i++) {
    const node = nodes[i];
    if (!node) continue;

    if (types.indexOf(node) >= 0) {
      continue;
    }

    if ((0, _generated.isTSAnyKeyword)(node)) {
      return [node];
    }

    if ((0, _generated.isTSBaseType)(node)) {
      bases.set(node.type, node);
      continue;
    }
    if ((0, _generated.isTSUnionType)(node)) {
      if (!typeGroups.has(node.types)) {
        nodes.push(...node.types);
        typeGroups.add(node.types);
      }
      continue;
    }

    if ((0, _generated.isTSTypeReference)(node) && node.typeParameters) {
      const name = getQualifiedName(node.typeName);
      if (generics.has(name)) {
        let existing = generics.get(name);
        if (existing.typeParameters) {
          if (node.typeParameters) {
            existing.typeParameters.params = removeTypeDuplicates(existing.typeParameters.params.concat(node.typeParameters.params));
          }
        } else {
          existing = node.typeParameters;
        }
      } else {
        generics.set(name, node);
      }
      continue;
    }
    types.push(node);
  }

  for (const [, baseType] of bases) {
    types.push(baseType);
  }

  for (const [, genericName] of generics) {
    types.push(genericName);
  }
  return types;
}

//# sourceMappingURL=removeTypeDuplicates.js.map
