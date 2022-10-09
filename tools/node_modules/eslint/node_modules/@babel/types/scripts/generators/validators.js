import {
  DEPRECATED_KEYS,
  FLIPPED_ALIAS_KEYS,
  NODE_FIELDS,
  PLACEHOLDERS,
  PLACEHOLDERS_FLIPPED_ALIAS,
  VISITOR_KEYS,
} from "../../lib/index.js";

const has = Function.call.bind(Object.prototype.hasOwnProperty);

function joinComparisons(leftArr, right) {
  return (
    leftArr.map(JSON.stringify).join(` === ${right} || `) + ` === ${right}`
  );
}

function addIsHelper(type, aliasKeys, deprecated) {
  const targetType = JSON.stringify(type);
  let aliasSource = "";
  if (aliasKeys) {
    aliasSource = joinComparisons(aliasKeys, "nodeType");
  }

  let placeholderSource = "";
  const placeholderTypes = [];
  if (PLACEHOLDERS.includes(type) && has(FLIPPED_ALIAS_KEYS, type)) {
    placeholderTypes.push(type);
  }
  if (has(PLACEHOLDERS_FLIPPED_ALIAS, type)) {
    placeholderTypes.push(...PLACEHOLDERS_FLIPPED_ALIAS[type]);
  }
  if (placeholderTypes.length > 0) {
    placeholderSource =
      ' || nodeType === "Placeholder" && (' +
      joinComparisons(
        placeholderTypes,
        "(node as t.Placeholder).expectedNode"
      ) +
      ")";
  }

  const result =
    NODE_FIELDS[type] || FLIPPED_ALIAS_KEYS[type]
      ? `node is t.${type}`
      : "boolean";

  return `export function is${type}(node: object | null | undefined, opts?: object | null): ${result} {
    ${deprecated || ""}
    if (!node) return false;

    const nodeType = (node as t.Node).type;
    if (${
      aliasSource ? aliasSource : `nodeType === ${targetType}`
    }${placeholderSource}) {
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

export default function generateValidators() {
  let output = `/*
 * This file is auto-generated! Do not modify it directly.
 * To re-generate run 'make build'
 */
import shallowEqual from "../../utils/shallowEqual";
import type * as t from "../..";\n\n`;

  Object.keys(VISITOR_KEYS).forEach(type => {
    output += addIsHelper(type);
  });

  Object.keys(FLIPPED_ALIAS_KEYS).forEach(type => {
    output += addIsHelper(type, FLIPPED_ALIAS_KEYS[type]);
  });

  Object.keys(DEPRECATED_KEYS).forEach(type => {
    const newType = DEPRECATED_KEYS[type];
    const deprecated = `console.trace("The node type ${type} has been renamed to ${newType}");`;
    output += addIsHelper(type, null, deprecated);
  });

  return output;
}
