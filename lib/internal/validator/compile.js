'use strict';

const {
  ArrayIsArray,
  ArrayPrototypeIncludes,
  ArrayPrototypeJoin,
  ArrayPrototypePush,
  ArrayPrototypeSlice,
  NumberIsFinite,
  NumberIsInteger,
  ObjectFreeze,
  ObjectKeys,
  RegExp,
  SafeSet,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_VALIDATOR_INVALID_SCHEMA,
  },
} = require('internal/errors');

const kValidTypes = new SafeSet([
  'string', 'number', 'integer', 'boolean', 'object', 'array', 'null',
]);

const kStringConstraints = new SafeSet([
  'type', 'minLength', 'maxLength', 'pattern', 'enum', 'default',
]);

const kNumberConstraints = new SafeSet([
  'type', 'minimum', 'maximum', 'exclusiveMinimum', 'exclusiveMaximum',
  'multipleOf', 'default',
]);

const kArrayConstraints = new SafeSet([
  'type', 'items', 'minItems', 'maxItems', 'default',
]);

const kObjectConstraints = new SafeSet([
  'type', 'properties', 'required', 'additionalProperties', 'default',
]);

const kSimpleConstraints = new SafeSet([
  'type', 'default',
]);

const kConstraintsByType = {
  __proto__: null,
  string: kStringConstraints,
  number: kNumberConstraints,
  integer: kNumberConstraints,
  boolean: kSimpleConstraints,
  null: kSimpleConstraints,
  array: kArrayConstraints,
  object: kObjectConstraints,
};

function formatPath(parentPath, key) {
  if (parentPath === '') return key;
  return `${parentPath}.${key}`;
}

function validateNonNegativeInteger(value, path, name) {
  if (!NumberIsInteger(value) || value < 0) {
    throw new ERR_VALIDATOR_INVALID_SCHEMA(
      path, `"${name}" must be a non-negative integer`);
  }
}

function validateFiniteNumber(value, path, name) {
  if (typeof value !== 'number' || !NumberIsFinite(value)) {
    throw new ERR_VALIDATOR_INVALID_SCHEMA(
      path, `"${name}" must be a finite number`);
  }
}

function checkUnknownConstraints(definition, allowedSet, path) {
  const keys = ObjectKeys(definition);
  for (let i = 0; i < keys.length; i++) {
    if (!allowedSet.has(keys[i])) {
      throw new ERR_VALIDATOR_INVALID_SCHEMA(
        path,
        `unknown constraint "${keys[i]}" for type "${definition.type}"`);
    }
  }
}

function compileSchemaNode(definition, path) {
  if (typeof definition !== 'object' || definition === null ||
      ArrayIsArray(definition)) {
    throw new ERR_INVALID_ARG_TYPE(
      path || 'definition', 'a plain object', definition);
  }

  const type = definition.type;
  if (typeof type !== 'string' || !kValidTypes.has(type)) {
    const validTypes = ArrayPrototypeJoin(
      ['string', 'number', 'integer', 'boolean', 'object', 'array', 'null'],
      ', ');
    throw new ERR_VALIDATOR_INVALID_SCHEMA(
      path, `"type" must be one of: ${validTypes}`);
  }

  const allowedConstraints = kConstraintsByType[type];
  checkUnknownConstraints(definition, allowedConstraints, path);

  const compiled = { __proto__: null, type };

  if ('default' in definition) {
    compiled.default = definition.default;
    compiled.hasDefault = true;
  } else {
    compiled.hasDefault = false;
  }

  switch (type) {
    case 'string':
      compileStringConstraints(definition, compiled, path);
      break;
    case 'number':
    case 'integer':
      compileNumberConstraints(definition, compiled, path);
      break;
    case 'array':
      compileArrayConstraints(definition, compiled, path);
      break;
    case 'object':
      compileObjectConstraints(definition, compiled, path);
      break;
    default:
      break;
  }

  return ObjectFreeze(compiled);
}

function compileStringConstraints(definition, compiled, path) {
  if ('minLength' in definition) {
    validateNonNegativeInteger(definition.minLength, path, 'minLength');
    compiled.minLength = definition.minLength;
  }
  if ('maxLength' in definition) {
    validateNonNegativeInteger(definition.maxLength, path, 'maxLength');
    compiled.maxLength = definition.maxLength;
  }
  if (compiled.minLength !== undefined && compiled.maxLength !== undefined &&
      compiled.minLength > compiled.maxLength) {
    throw new ERR_VALIDATOR_INVALID_SCHEMA(
      path, '"minLength" must not be greater than "maxLength"');
  }
  if ('pattern' in definition) {
    if (typeof definition.pattern !== 'string') {
      throw new ERR_VALIDATOR_INVALID_SCHEMA(
        path, '"pattern" must be a string');
    }
    try {
      compiled.pattern = new RegExp(definition.pattern);
    } catch {
      throw new ERR_VALIDATOR_INVALID_SCHEMA(
        path, `"pattern" is not a valid regular expression: ${definition.pattern}`);
    }
    compiled.patternSource = definition.pattern;
  }
  if ('enum' in definition) {
    if (!ArrayIsArray(definition.enum) || definition.enum.length === 0) {
      throw new ERR_VALIDATOR_INVALID_SCHEMA(
        path, '"enum" must be a non-empty array');
    }
    compiled.enum = ArrayPrototypeSlice(definition.enum);
  }
}

function compileNumberConstraints(definition, compiled, path) {
  if ('minimum' in definition) {
    validateFiniteNumber(definition.minimum, path, 'minimum');
    compiled.minimum = definition.minimum;
  }
  if ('maximum' in definition) {
    validateFiniteNumber(definition.maximum, path, 'maximum');
    compiled.maximum = definition.maximum;
  }
  if ('exclusiveMinimum' in definition) {
    validateFiniteNumber(definition.exclusiveMinimum, path, 'exclusiveMinimum');
    compiled.exclusiveMinimum = definition.exclusiveMinimum;
  }
  if ('exclusiveMaximum' in definition) {
    validateFiniteNumber(definition.exclusiveMaximum, path, 'exclusiveMaximum');
    compiled.exclusiveMaximum = definition.exclusiveMaximum;
  }
  if ('multipleOf' in definition) {
    validateFiniteNumber(definition.multipleOf, path, 'multipleOf');
    if (definition.multipleOf <= 0) {
      throw new ERR_VALIDATOR_INVALID_SCHEMA(
        path, '"multipleOf" must be greater than 0');
    }
    compiled.multipleOf = definition.multipleOf;
  }
}

function compileArrayConstraints(definition, compiled, path) {
  if ('minItems' in definition) {
    validateNonNegativeInteger(definition.minItems, path, 'minItems');
    compiled.minItems = definition.minItems;
  }
  if ('maxItems' in definition) {
    validateNonNegativeInteger(definition.maxItems, path, 'maxItems');
    compiled.maxItems = definition.maxItems;
  }
  if (compiled.minItems !== undefined && compiled.maxItems !== undefined &&
      compiled.minItems > compiled.maxItems) {
    throw new ERR_VALIDATOR_INVALID_SCHEMA(
      path, '"minItems" must not be greater than "maxItems"');
  }
  if ('items' in definition) {
    compiled.items = compileSchemaNode(definition.items, formatPath(path, 'items'));
  }
}

function compileObjectConstraints(definition, compiled, path) {
  if ('properties' in definition) {
    if (typeof definition.properties !== 'object' ||
        definition.properties === null ||
        ArrayIsArray(definition.properties)) {
      throw new ERR_VALIDATOR_INVALID_SCHEMA(
        path, '"properties" must be a plain object');
    }
    const propKeys = ObjectKeys(definition.properties);
    const compiledProps = { __proto__: null };
    const propNames = [];
    for (let i = 0; i < propKeys.length; i++) {
      const key = propKeys[i];
      ArrayPrototypePush(propNames, key);
      compiledProps[key] = compileSchemaNode(
        definition.properties[key],
        formatPath(path, `properties.${key}`));
    }
    compiled.properties = ObjectFreeze(compiledProps);
    compiled.propertyNames = propNames;
  }

  if ('required' in definition) {
    if (!ArrayIsArray(definition.required)) {
      throw new ERR_VALIDATOR_INVALID_SCHEMA(
        path, '"required" must be an array of strings');
    }
    for (let i = 0; i < definition.required.length; i++) {
      if (typeof definition.required[i] !== 'string') {
        throw new ERR_VALIDATOR_INVALID_SCHEMA(
          path, '"required" must be an array of strings');
      }
      if (compiled.properties &&
          !ArrayPrototypeIncludes(compiled.propertyNames, definition.required[i])) {
        throw new ERR_VALIDATOR_INVALID_SCHEMA(
          path,
          `required property "${definition.required[i]}" is not defined in "properties"`);
      }
    }
    compiled.required = ArrayPrototypeSlice(definition.required);
  }

  if ('additionalProperties' in definition) {
    if (typeof definition.additionalProperties !== 'boolean') {
      throw new ERR_VALIDATOR_INVALID_SCHEMA(
        path, '"additionalProperties" must be a boolean');
    }
    compiled.additionalProperties = definition.additionalProperties;
  } else {
    compiled.additionalProperties = true;
  }
}

/**
 * Validate and lower a user schema definition into a frozen, null-prototype
 * representation consumed by `validateValue()` and `applyDefaults()`.
 *
 * All schema errors (unknown type, bad constraints, malformed `required`,
 * non-compiling `pattern`, etc.) surface from this call as
 * `ERR_VALIDATOR_INVALID_SCHEMA` so they fail fast at `new Schema(...)`
 * rather than on each `validate(data)`.
 * @param {object} definition User schema definition.
 * @returns {object} Frozen compiled schema node.
 */
function compileSchema(definition) {
  if (typeof definition !== 'object' || definition === null ||
      ArrayIsArray(definition)) {
    throw new ERR_INVALID_ARG_TYPE('definition', 'a plain object', definition);
  }
  return compileSchemaNode(definition, '');
}

module.exports = { compileSchema };
