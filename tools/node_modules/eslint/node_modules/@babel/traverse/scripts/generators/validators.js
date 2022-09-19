import * as t from "@babel/types";

export default function generateValidators() {
  let output = `/*
 * This file is auto-generated! Do not modify it directly.
 * To re-generate run 'make build'
 */
import type * as t from "@babel/types";
import type NodePath from "../index";
import type { VirtualTypeNodePathValidators } from "../lib/virtual-types-validator";

interface BaseNodePathValidators {
`;

  for (const type of [...t.TYPES].sort()) {
    output += `is${type}<T extends t.Node>(this: NodePath<T>, opts?: object): this is NodePath<T & t.${type}>;`;
  }

  output += `
}

export interface NodePathValidators
  extends BaseNodePathValidators, VirtualTypeNodePathValidators {}
`;

  return output;
}
