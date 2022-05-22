import t from "../../lib/index.js";
import definitions from "../../lib/definitions/index.js";
import formatBuilderName from "../utils/formatBuilderName.js";
import lowerFirst from "../utils/lowerFirst.js";
import stringifyValidator from "../utils/stringifyValidator.js";

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
      const field = definitions.NODE_FIELDS[type][fieldName];
      const def = JSON.stringify(field.default);
      const bindingIdentifierName = t.toBindingIdentifierName(fieldName);
      let arg;
      if (areAllRemainingFieldsNullable(fieldName, builderNames, fields)) {
        arg = `${bindingIdentifierName}${
          isNullable(field) && !def ? "?:" : ":"
        } ${typeAnnotation}`;
      } else {
        arg = `${bindingIdentifierName}: ${typeAnnotation}${
          isNullable(field) ? " | undefined" : ""
        }`;
      }
      if (def !== "null" || isNullable(field)) {
        arg += `= ${def}`;
      }
      args.push(arg);
    }
  });

  return args;
}

export default function generateBuilders(kind) {
  return kind === "uppercase.js"
    ? generateUppercaseBuilders()
    : generateLowercaseBuilders();
}

function generateLowercaseBuilders() {
  let output = `/*
 * This file is auto-generated! Do not modify it directly.
 * To re-generate run 'make build'
 */
import validateNode from "../validateNode";
import type * as t from "../..";
`;

  const reservedNames = new Set(["super", "import"]);
  Object.keys(definitions.BUILDER_KEYS).forEach(type => {
    const defArgs = generateBuilderArgs(type);
    const formatedBuilderName = formatBuilderName(type);
    const formatedBuilderNameLocal = reservedNames.has(formatedBuilderName)
      ? `_${formatedBuilderName}`
      : formatedBuilderName;

    const fieldNames = sortFieldNames(
      Object.keys(definitions.NODE_FIELDS[type]),
      type
    );
    const builderNames = definitions.BUILDER_KEYS[type];
    const objectFields = [["type", JSON.stringify(type)]];
    fieldNames.forEach(fieldName => {
      const field = definitions.NODE_FIELDS[type][fieldName];
      if (builderNames.includes(fieldName)) {
        const bindingIdentifierName = t.toBindingIdentifierName(fieldName);
        objectFields.push([fieldName, bindingIdentifierName]);
      } else if (!field.optional) {
        const def = JSON.stringify(field.default);
        objectFields.push([fieldName, def]);
      }
    });

    output += `${
      formatedBuilderNameLocal === formatedBuilderName ? "export " : ""
    }function ${formatedBuilderNameLocal}(${defArgs.join(", ")}): t.${type} {`;

    const nodeObjectExpression = `{\n${objectFields
      .map(([k, v]) => (k === v ? `    ${k},` : `    ${k}: ${v},`))
      .join("\n")}\n  }`;

    if (builderNames.length > 0) {
      output += `\n  return validateNode<t.${type}>(${nodeObjectExpression});`;
    } else {
      output += `\n  return ${nodeObjectExpression};`;
    }
    output += `\n}\n`;

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
    const formatedNewBuilderName = formatBuilderName(newType);
    output += `/** @deprecated */
function ${type}(${generateBuilderArgs(newType).join(", ")}) {
  console.trace("The node type ${type} has been renamed to ${newType}");
  return ${formatedNewBuilderName}(${t.BUILDER_KEYS[newType].join(", ")});
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
