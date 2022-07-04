import * as virtualTypes from "../../lib/path/lib/virtual-types.js";

export default function generateValidators() {
  let output = `/*
 * This file is auto-generated! Do not modify it directly.
 * To re-generate run 'make build'
 */
import * as t from "@babel/types";

export interface VirtualTypeAliases {
`;

  for (const type of Object.keys(virtualTypes)) {
    // TODO: Remove this check once we stop compiling to CJS
    if (type === "default" || type === "__esModule") continue;

    output += `  ${type}: ${(virtualTypes[type].types || ["Node"])
      .map(t => `t.${t}`)
      .join(" | ")};`;
  }

  output += `
}
`;

  return output;
}
