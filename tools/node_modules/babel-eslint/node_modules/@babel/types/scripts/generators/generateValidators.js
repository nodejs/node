"use strict";
const definitions = require("../../lib/definitions");

function addIsHelper(type) {
  return `export function is${type}(node: Object, opts?: Object): boolean {
    return is("${type}", node, opts) }
  `;
}

module.exports = function generateValidators() {
  let output = `// @flow
/*
 * This file is auto-generated! Do not modify it directly.
 * To re-generate run 'make build'
 */
import is from "../is";\n\n`;

  Object.keys(definitions.VISITOR_KEYS).forEach(type => {
    output += addIsHelper(type);
  });

  Object.keys(definitions.FLIPPED_ALIAS_KEYS).forEach(type => {
    output += addIsHelper(type);
  });

  Object.keys(definitions.DEPRECATED_KEYS).forEach(type => {
    const newType = definitions.DEPRECATED_KEYS[type];
    output += `export function is${type}(node: Object, opts: Object): boolean {
  console.trace("The node type ${type} has been renamed to ${newType}");
  return is("${type}", node, opts);
}\n`;
  });

  return output;
};
