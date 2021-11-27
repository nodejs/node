import definitions from "../../lib/definitions/index.js";

export default function generateConstants() {
  let output = `/*
 * This file is auto-generated! Do not modify it directly.
 * To re-generate run 'make build'
 */
import { FLIPPED_ALIAS_KEYS } from "../../definitions";\n\n`;

  Object.keys(definitions.FLIPPED_ALIAS_KEYS).forEach(type => {
    output += `export const ${type.toUpperCase()}_TYPES = FLIPPED_ALIAS_KEYS["${type}"];\n`;
  });

  return output;
}
