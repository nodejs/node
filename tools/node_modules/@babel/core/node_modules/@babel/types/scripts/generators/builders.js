"use strict";
const definitions = require("../../lib/definitions");
const formatBuilderName = require("../utils/formatBuilderName");
const lowerFirst = require("../utils/lowerFirst");

const t = require("../../");
const stringifyValidator = require("../utils/stringifyValidator");

function areAllRemainingFieldsNullable(fieldName, fieldNames, fields) {
  const index = fieldNames.indexOf(fieldName);
  return fieldNames.slice(index).every(_ => isNullable(fields[_]));
}

function hasDefault(field) {
  return field.default != null;
}

function isNullable(field) {
  return field.optional || hasDefault(field);
}

function sortFieldNames(fields, type) {
  return fields.sort((fieldA, fieldB) => {
    const indexA = t.BUILDER_KEYS[type].indexOf(fieldA);
    const indexB = t.BUILDER_KEYS[type].indexOf(fieldB);
    if (indexA === indexB) return fieldA < fieldB ? -1 : 1;
    if (indexA === -1) return 1;
    if (indexB === -1) return -1;
    return indexA - indexB;
  });
}

function generateBuilderArgs(type) {
  const fields = t.NODE_FIELDS[type];
  const fieldNames = sortFieldNames(Object.keys(t.NODE_FIELDS[type]), type);
  const builderNames = t.BUILDER_KEYS[type];

  const args = [];

  fieldNames.forEach(fieldName => {
    const field = fields[fieldName];
    // Future / annoying TODO:
    // MemberExpression.property, ObjectProperty.key and ObjectMethod.key need special cases; either:
    // - convert the declaration to chain() like ClassProperty.key and ClassMethod.key,
    // - declare an alias type for valid keys, detect the case and reuse it here,
    // - declare a disjoint union with, for example, ObjectPropertyBase,
    //   ObjectPropertyLiteralKey and ObjectPropertyComputedKey, and declare ObjectProperty
    //   as "ObjectPropertyBase & (ObjectPropertyLiteralKey | ObjectPropertyComputedKey)"
    let typeAnnotation = stringifyValidator(field.validate, "t.");

    if (isNullable(field) && !hasDefault(field)) {
      typeAnnotation += " | null";
    }

    if (builderNames.includes(fieldName)) {
      const bindingIdentifierName = t.toBindingIdentifierName(fieldName);
      if (areAllRemainingFieldsNullable(fieldName, builderNames, fields)) {
        args.push(
          `${bindingIdentifierName}${
            isNullable(field) ? "?:" : ":"
          } ${typeAnnotation}`
        );
      } else {
        args.push(
          `${bindingIdentifierName}: ${typeAnnotation}${
            isNullable(field) ? " | undefined" : ""
          }`
        );
      }
    }
  });

  return args;
}

module.exports = function generateBuilders(kind) {
  return kind === "uppercase.js"
    ? generateUppercaseBuilders()
    : generateLowercaseBuilders();
};

function generateLowercaseBuilders() {
  let output = `/*
 * This file is auto-generated! Do not modify it directly.
 * To re-generate run 'make build'
 */
import builder from "../builder";
import type * as t from "../..";

/* eslint-disable @typescript-eslint/no-unused-vars */

`;

  const reservedNames = new Set(["super", "import"]);
  Object.keys(definitions.BUILDER_KEYS).forEach(type => {
    const defArgs = generateBuilderArgs(type);
    const formatedBuilderName = formatBuilderName(type);
    const formatedBuilderNameLocal = reservedNames.has(formatedBuilderName)
      ? `_${formatedBuilderName}`
      : formatedBuilderName;
    output += `${
      formatedBuilderNameLocal === formatedBuilderName ? "export " : ""
    }function ${formatedBuilderNameLocal}(${defArgs.join(
      ", "
    )}): t.${type} { return builder("${type}", ...arguments); }\n`;
    if (formatedBuilderNameLocal !== formatedBuilderName) {
      output += `export { ${formatedBuilderNameLocal} as ${formatedBuilderName} };\n`;
    }

    // This is needed for backwards compatibility.
    // It should be removed in the next major version.
    // JSXIdentifier -> jSXIdentifier
    if (/^[A-Z]{2}/.test(type)) {
      output += `export { ${formatedBuilderNameLocal} as ${lowerFirst(
        type
      )} }\n`;
    }
  });

  Object.keys(definitions.DEPRECATED_KEYS).forEach(type => {
    const newType = definitions.DEPRECATED_KEYS[type];
    const formatedBuilderName = formatBuilderName(type);
    output += `/** @deprecated */
function ${type}(...args: Array<any>): any {
  console.trace("The node type ${type} has been renamed to ${newType}");
  return builder("${type}", ...args);
}
export { ${type} as ${formatedBuilderName} };\n`;
    // This is needed for backwards compatibility.
    // It should be removed in the next major version.
    // JSXIdentifier -> jSXIdentifier
    if (/^[A-Z]{2}/.test(type)) {
      output += `export { ${type} as ${lowerFirst(type)} }\n`;
    }
  });

  return output;
}

function generateUppercaseBuilders() {
  let output = `/*
 * This file is auto-generated! Do not modify it directly.
 * To re-generate run 'make build'
 */

/**
 * This file is written in JavaScript and not TypeScript because uppercase builders
 * conflict with AST types. TypeScript reads the uppercase.d.ts file instead.
 */

 export {\n`;

  Object.keys(definitions.BUILDER_KEYS).forEach(type => {
    const formatedBuilderName = formatBuilderName(type);
    output += `  ${formatedBuilderName} as ${type},\n`;
  });

  Object.keys(definitions.DEPRECATED_KEYS).forEach(type => {
    const formatedBuilderName = formatBuilderName(type);
    output += `  ${formatedBuilderName} as ${type},\n`;
  });

  output += ` } from './index';\n`;
  return output;
}
