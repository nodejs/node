import t from "@babel/types";
import virtualTypes from "../../lib/path/lib/virtual-types.js";
import definitions from "@babel/types/lib/definitions/index.js";

export default function generateValidators() {
  let output = `/*
 * This file is auto-generated! Do not modify it directly.
 * To re-generate run 'make build'
 */
import * as t from "@babel/types";
import NodePath from "../index";

export interface NodePathValidators {
`;

  for (const type of [...t.TYPES].sort()) {
    output += `is${type}(opts?: object): this is NodePath<t.${type}>;`;
  }

  for (const type of Object.keys(virtualTypes)) {
    if (type[0] === "_") continue;
    if (definitions.NODE_FIELDS[type] || definitions.FLIPPED_ALIAS_KEYS[type]) {
      output += `is${type}(opts?: object): this is NodePath<t.${type}>;`;
    } else {
      output += `is${type}(opts?: object): boolean;`;
    }
  }

  output += `
}
`;

  return output;
}
