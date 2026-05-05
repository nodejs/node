'use strict';

const {
  ArrayIsArray,
  ArrayPrototypePush,
  ObjectKeys,
  ObjectPrototypeHasOwnProperty,
} = primordials;

/**
 * Return a new value with schema defaults applied to missing or `undefined`
 * object properties, recursing into nested object and array schemas.
 *
 * Input is not mutated. Only property slots whose schema declares a
 * `default` get filled; missing intermediate objects are not created, so
 * `applyDefaults({})` against `{ a: { b: { default: 1 } } }` returns `{}`,
 * not `{ a: { b: 1 } }`. Non-object / non-array inputs are returned
 * unchanged.
 * @param {any} data Value to augment with defaults.
 * @param {object} compiled Compiled schema node produced by `compileSchema()`.
 * @returns {any} New value with defaults applied (never mutates `data`).
 */
function applyDefaultsInternal(data, compiled) {
  if (compiled.type !== 'object' || typeof data !== 'object' ||
      data === null || ArrayIsArray(data)) {
    if (compiled.type === 'array' && ArrayIsArray(data) &&
        compiled.items !== undefined) {
      const result = [];
      for (let i = 0; i < data.length; i++) {
        ArrayPrototypePush(result, applyDefaultsInternal(data[i], compiled.items));
      }
      return result;
    }
    return data;
  }

  const result = { __proto__: null };
  const dataKeys = ObjectKeys(data);
  for (let i = 0; i < dataKeys.length; i++) {
    const key = dataKeys[i];
    result[key] = data[key];
  }

  if (compiled.properties !== undefined) {
    for (let i = 0; i < compiled.propertyNames.length; i++) {
      const key = compiled.propertyNames[i];
      const propSchema = compiled.properties[key];
      if (!ObjectPrototypeHasOwnProperty(result, key) ||
          result[key] === undefined) {
        if (propSchema.hasDefault) {
          result[key] = propSchema.default;
        }
      } else if (propSchema.type === 'object' || propSchema.type === 'array') {
        result[key] = applyDefaultsInternal(result[key], propSchema);
      }
    }
  }

  return result;
}

module.exports = { applyDefaults: applyDefaultsInternal };
