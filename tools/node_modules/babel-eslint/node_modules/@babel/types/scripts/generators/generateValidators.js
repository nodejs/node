"use strict";
const definitions = require("../../lib/definitions");

function addIsHelper(type, aliasKeys, deprecated) {
  const targetType = JSON.stringify(type);
  let aliasSource = "";
  if (aliasKeys) {
    aliasSource =
      " || " +
      aliasKeys.map(JSON.stringify).join(" === nodeType || ") +
      " === nodeType";
  }

  return `export function is${type}(node: ?Object, opts?: Object): boolean {
    ${deprecated || ""}
    if (!node) return false;

    const nodeType = node.type;
    if (nodeType === ${targetType}${aliasSource}) {
      if (typeof opts === "undefined") {
        return true;
      } else {
        return shallowEqual(node, opts);
      }
    }

    return false;
  }
  `;
}

module.exports = function generateValidators() {
  let output = `// @flow
/*
 * This file is auto-generated! Do not modify it directly.
 * To re-generate run 'make build'
 */
import shallowEqual from "../../utils/shallowEqual";\n\n`;

  Object.keys(definitions.VISITOR_KEYS).forEach(type => {
    output += addIsHelper(type);
  });

  Object.keys(definitions.FLIPPED_ALIAS_KEYS).forEach(type => {
    output += addIsHelper(type, definitions.FLIPPED_ALIAS_KEYS[type]);
  });

  Object.keys(definitions.DEPRECATED_KEYS).forEach(type => {
    const newType = definitions.DEPRECATED_KEYS[type];
    const deprecated = `console.trace("The node type ${type} has been renamed to ${newType}");`;
    output += addIsHelper(type, null, deprecated);
  });

  return output;
};
