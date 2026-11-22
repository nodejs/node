'use strict';

// Helpers shared by util.debounce() and util.throttle().

const {
  ObjectDefineProperties,
} = primordials;

const {
  AbortError,
} = require('internal/errors');

/**
 * Creates the AbortError used to reject calls that were cancelled or aborted.
 * @param {any} reason
 * @returns {AbortError}
 */
function createAbortError(reason) {
  return reason === undefined ?
    new AbortError() :
    new AbortError(undefined, { __proto__: null, cause: reason });
}

/**
 * Defines the control methods, state getters, `length`, and `name` on a
 * function returned by util.debounce() or util.throttle().
 * @param {Function} wrapper The function returned to the user.
 * @param {Function} fn The wrapped user function.
 * @param {{ [name: string]: Function }} methods Enumerable, writable methods.
 * @param {{ [name: string]: Function }} getters Non-enumerable getters.
 * @returns {Function} `wrapper`.
 */
function defineWrapperProperties(wrapper, fn, methods, getters) {
  const descriptors = {
    __proto__: null,
    length: {
      __proto__: null,
      configurable: true,
      value: fn.length,
    },
    name: {
      __proto__: null,
      configurable: true,
      value: fn.name,
    },
  };
  for (const name in methods) {
    descriptors[name] = {
      __proto__: null,
      configurable: true,
      enumerable: true,
      writable: true,
      value: methods[name],
    };
  }
  for (const name in getters) {
    descriptors[name] = {
      __proto__: null,
      enumerable: false,
      get: getters[name],
    };
  }
  return ObjectDefineProperties(wrapper, descriptors);
}

module.exports = {
  createAbortError,
  defineWrapperProperties,
};
