'use strict';

const {
  ArrayIsArray,
  ArrayPrototypePush,
  MathAbs,
  NumberEPSILON,
  NumberIsFinite,
  NumberIsInteger,
  NumberIsNaN,
  ObjectKeys,
  ObjectPrototypeHasOwnProperty,
  RegExpPrototypeExec,
  String,
} = primordials;

const { codes } = require('internal/validator/errors');

function addError(errors, path, message, code) {
  ArrayPrototypePush(errors, {
    __proto__: null,
    path,
    message,
    code,
  });
}

function formatPath(parentPath, key) {
  if (parentPath === '') return String(key);
  return `${parentPath}.${key}`;
}

function formatArrayPath(parentPath, index) {
  return `${parentPath}[${index}]`;
}

function getTypeName(value) {
  if (value === null) return 'null';
  if (ArrayIsArray(value)) return 'array';
  return typeof value;
}

/**
 * Validate a value against a compiled schema node and push any failures to
 * `errors` as `{ path, message, code }` objects.
 *
 * Validation never throws; all failures accumulate in `errors`. Recursion
 * does not track visited nodes: cyclic input will stack-overflow (documented
 * limitation; callers supplying untrusted data should guard upstream).
 * @param {any} value Runtime value being validated.
 * @param {object} compiled Compiled schema node produced by `compileSchema()`.
 * @param {string} path Path to `value` from the validation root.
 * @param {Array} errors Mutated in place with any error records.
 */
function validateValue(value, compiled, path, errors) {
  const expected = compiled.type;
  const actual = getTypeName(value);

  switch (expected) {
    case 'string':
      if (actual !== 'string') {
        addError(errors, path,
                 `Expected type string, got ${actual}`,
                 codes.INVALID_TYPE);
        return;
      }
      validateStringConstraints(value, compiled, path, errors);
      break;
    case 'number':
      if (actual !== 'number' || NumberIsNaN(value)) {
        addError(errors, path,
                 `Expected type number, got ${NumberIsNaN(value) ? 'NaN' : actual}`,
                 codes.INVALID_TYPE);
        return;
      }
      validateNumberConstraints(value, compiled, path, errors);
      break;
    case 'integer':
      if (actual !== 'number' || NumberIsNaN(value)) {
        addError(errors, path,
                 `Expected type integer, got ${NumberIsNaN(value) ? 'NaN' : actual}`,
                 codes.INVALID_TYPE);
        return;
      }
      if (!NumberIsInteger(value)) {
        addError(errors, path,
                 `Expected an integer, got ${value}`,
                 codes.NOT_INTEGER);
        return;
      }
      validateNumberConstraints(value, compiled, path, errors);
      break;
    case 'boolean':
      if (actual !== 'boolean') {
        addError(errors, path,
                 `Expected type boolean, got ${actual}`,
                 codes.INVALID_TYPE);
      }
      break;
    case 'null':
      if (value !== null) {
        addError(errors, path,
                 `Expected null, got ${actual}`,
                 codes.INVALID_TYPE);
      }
      break;
    case 'array':
      if (!ArrayIsArray(value)) {
        addError(errors, path,
                 `Expected type array, got ${actual}`,
                 codes.INVALID_TYPE);
        return;
      }
      validateArrayConstraints(value, compiled, path, errors);
      break;
    case 'object':
      if (actual !== 'object' || value === null || ArrayIsArray(value)) {
        addError(errors, path,
                 `Expected type object, got ${actual}`,
                 codes.INVALID_TYPE);
        return;
      }
      validateObjectConstraints(value, compiled, path, errors);
      break;
  }
}

function validateStringConstraints(value, compiled, path, errors) {
  if (compiled.minLength !== undefined && value.length < compiled.minLength) {
    addError(errors, path,
             `String length ${value.length} is less than minimum ${compiled.minLength}`,
             codes.STRING_TOO_SHORT);
  }
  if (compiled.maxLength !== undefined && value.length > compiled.maxLength) {
    addError(errors, path,
             `String length ${value.length} exceeds maximum ${compiled.maxLength}`,
             codes.STRING_TOO_LONG);
  }
  if (compiled.pattern !== undefined) {
    if (RegExpPrototypeExec(compiled.pattern, value) === null) {
      addError(errors, path,
               `String does not match pattern "${compiled.patternSource}"`,
               codes.PATTERN_MISMATCH);
    }
  }
  if (compiled.enum !== undefined) {
    let found = false;
    for (let i = 0; i < compiled.enum.length; i++) {
      if (value === compiled.enum[i]) {
        found = true;
        break;
      }
    }
    if (!found) {
      addError(errors, path,
               `Value "${value}" is not one of the allowed values`,
               codes.ENUM_MISMATCH);
    }
  }
}

function validateNumberConstraints(value, compiled, path, errors) {
  if (!NumberIsFinite(value)) {
    addError(errors, path,
             `Expected a finite number, got ${value}`,
             codes.INVALID_TYPE);
    return;
  }
  if (compiled.minimum !== undefined && value < compiled.minimum) {
    addError(errors, path,
             `Value ${value} is less than minimum ${compiled.minimum}`,
             codes.NUMBER_TOO_SMALL);
  }
  if (compiled.exclusiveMinimum !== undefined &&
      value <= compiled.exclusiveMinimum) {
    addError(errors, path,
             `Value ${value} is less than or equal to exclusive minimum ${compiled.exclusiveMinimum}`,
             codes.NUMBER_TOO_SMALL);
  }
  if (compiled.maximum !== undefined && value > compiled.maximum) {
    addError(errors, path,
             `Value ${value} exceeds maximum ${compiled.maximum}`,
             codes.NUMBER_TOO_LARGE);
  }
  if (compiled.exclusiveMaximum !== undefined &&
      value >= compiled.exclusiveMaximum) {
    addError(errors, path,
             `Value ${value} is greater than or equal to exclusive maximum ${compiled.exclusiveMaximum}`,
             codes.NUMBER_TOO_LARGE);
  }
  if (compiled.multipleOf !== undefined) {
    const remainder = MathAbs(value % compiled.multipleOf);
    const threshold = NumberEPSILON *
      MathAbs(value > compiled.multipleOf ? value : compiled.multipleOf);
    if (remainder > threshold && remainder < compiled.multipleOf - threshold) {
      addError(errors, path,
               `Value ${value} is not a multiple of ${compiled.multipleOf}`,
               codes.NUMBER_NOT_MULTIPLE);
    }
  }
}

function validateArrayConstraints(value, compiled, path, errors) {
  if (compiled.minItems !== undefined && value.length < compiled.minItems) {
    addError(errors, path,
             `Array length ${value.length} is less than minimum ${compiled.minItems}`,
             codes.ARRAY_TOO_SHORT);
  }
  if (compiled.maxItems !== undefined && value.length > compiled.maxItems) {
    addError(errors, path,
             `Array length ${value.length} exceeds maximum ${compiled.maxItems}`,
             codes.ARRAY_TOO_LONG);
  }
  if (compiled.items !== undefined) {
    for (let i = 0; i < value.length; i++) {
      validateValue(value[i], compiled.items, formatArrayPath(path, i), errors);
    }
  }
}

function validateObjectConstraints(value, compiled, path, errors) {
  if (compiled.required !== undefined) {
    for (let i = 0; i < compiled.required.length; i++) {
      const key = compiled.required[i];
      if (!ObjectPrototypeHasOwnProperty(value, key)) {
        addError(errors, formatPath(path, key),
                 `Property "${key}" is required`,
                 codes.MISSING_REQUIRED);
      }
    }
  }

  if (compiled.properties !== undefined) {
    for (let i = 0; i < compiled.propertyNames.length; i++) {
      const key = compiled.propertyNames[i];
      if (ObjectPrototypeHasOwnProperty(value, key)) {
        validateValue(
          value[key], compiled.properties[key],
          formatPath(path, key), errors);
      }
    }
  }

  if (!compiled.additionalProperties) {
    const valueKeys = ObjectKeys(value);
    for (let i = 0; i < valueKeys.length; i++) {
      const key = valueKeys[i];
      if (compiled.properties === undefined ||
          !ObjectPrototypeHasOwnProperty(compiled.properties, key)) {
        addError(errors, formatPath(path, key),
                 `Additional property "${key}" is not allowed`,
                 codes.ADDITIONAL_PROPERTY);
      }
    }
  }
}

module.exports = { validateValue };
