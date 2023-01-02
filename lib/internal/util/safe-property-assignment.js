'use strict';

const {
  ObjectCreate,
  ObjectDefineProperties,
  ObjectPrototypeHasOwnProperty,
  ReflectDefineProperty,
  ReflectOwnKeys,
} = primordials;

/**
 * Mimics `obj[key] = value` but ignoring potential prototype inheritance.
 * @param {any} obj
 * @param {string} key
 * @param {any} value
 * @returns {boolean}
 */
function setOwnProperty(obj, key, value) {
  if (ObjectPrototypeHasOwnProperty(obj, key)) {
    obj[key] = value;
    return true;
  }
  return ReflectDefineProperty(obj, key, {
    __proto__: null,
    configurable: true,
    enumerable: true,
    value,
    writable: true,
  });
}

/**
 * Mimics `Object.assign` but ignoring potential prototype inheritance.
 * @template T
 * @param {T} obj
 * @param {Record<string|symbol, unknown>[]} sources
 * @returns {T}
 */
function assignOwnProperties(obj, ...sources) {
  const descriptors = ObjectCreate(null);
  for (let i = 0; i < sources.length; i++) {
    const keys = ReflectOwnKeys(sources[i]);
    for (let j = 0; j < keys.length; j++) {
      const key = keys[j];
      if (ObjectPrototypeHasOwnProperty(obj, key)) {
        obj[key] = sources[i][key];
      } else {
        descriptors[key] = {
          __proto__: null,
          configurable: true,
          enumerable: true,
          value: sources[i][key],
          writable: true,
        };
      }
    }
  }
  return ObjectDefineProperties(obj, descriptors);
}

module.exports = {
  assignOwnProperties,
  setOwnProperty,
};
