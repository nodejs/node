import * as t from "@babel/types";
import * as virtualTypes from "../../lib/path/lib/virtual-types.js";

export default function generateValidators() {
  let output = `/*
 * This file is auto-generated! Do not modify it directly.
 * To re-generate run 'make build'
 */
import * as t from "@babel/types";
import NodePath from "../index";
import type { VirtualTypeAliases } from "./virtual-types";

export interface NodePathValidators {
`;

  for (const type of [...t.TYPES].sort()) {
    output += `is${type}(opts?: object): this is NodePath<t.${type}>;`;
  }

  for (const type of Object.keys(virtualTypes)) {
    // TODO: Remove this check once we stop compiling to CJS
    if (type === "default" || type === "__esModule") continue;

    const { types } = virtualTypes[type];
    if (type[0] === "_") continue;
    if (t.NODE_FIELDS[type] || t.FLIPPED_ALIAS_KEYS[type]) {
      output += `is${type}(opts?: object): this is NodePath<t.${type}>;`;
    } else if (types /* in VirtualTypeAliases */) {
      output += `is${type}(opts?: object): this is NodePath<VirtualTypeAliases["${type}"]>;`;
    } else if (type === "Pure") {
      output += `isPure(constantsOnly?: boolean): boolean;`;
    } else {
      // if it don't have types, then VirtualTypeAliases[type] is t.Node
      // which TS marked as always true
      // eg. if (path.isBlockScope()) return;
      //     path resolved to `never` here
      // so we have to return boolean instead of this is NodePath<t.Node> here
      output += `is${type}(opts?: object): boolean;`;
    }
  }

  output += `
}
`;

  return output;
}
