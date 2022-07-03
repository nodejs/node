import * as definitions from "../../lib/definitions/index.js";

function addAssertHelper(type) {
  const result =
    definitions.NODE_FIELDS[type] || definitions.FLIPPED_ALIAS_KEYS[type]
      ? `node is t.${type}`
      : "boolean";

  return `export function assert${type}(node: object | null | undefined, opts?: object | null): asserts ${
    result === "boolean" ? "node" : result
  } {
    assert("${type}", node, opts) }
  `;
}

export default function generateAsserts() {
  let output = `/*
 * This file is auto-generated! Do not modify it directly.
 * To re-generate run 'make build'
 */
import is from "../../validators/is";
import type * as t from "../..";

function assert(type: string, node: any, opts?: any): void {
  if (!is(type, node, opts)) {
    throw new Error(
      \`Expected type "\${type}" with option \${JSON.stringify(opts)}, \` +
        \`but instead got "\${node.type}".\`,
    );
  }
}\n\n`;

  Object.keys(definitions.VISITOR_KEYS).forEach(type => {
    output += addAssertHelper(type);
  });

  Object.keys(definitions.FLIPPED_ALIAS_KEYS).forEach(type => {
    output += addAssertHelper(type);
  });

  Object.keys(definitions.DEPRECATED_KEYS).forEach(type => {
    const newType = definitions.DEPRECATED_KEYS[type];
    output += `export function assert${type}(node: any, opts: any): void {
  console.trace("The node type ${type} has been renamed to ${newType}");
  assert("${type}", node, opts);
}\n`;
  });

  return output;
}
