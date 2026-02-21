'use strict';

// test_runner-only helpers used to preserve AssertionError actual/expected
// constructor names across process isolation boundaries.

const {
  ArrayIsArray,
  ArrayPrototype,
  ObjectDefineProperty,
  ObjectGetOwnPropertyDescriptor,
  ObjectGetPrototypeOf,
  ObjectPrototype,
  ObjectPrototypeToString,
  ObjectSetPrototypeOf,
} = primordials;

const kAssertionErrorCode = 'ERR_ASSERTION';
const kTestFailureErrorCode = 'ERR_TEST_FAILURE';
const kBaseTypeArray = 'array';
const kBaseTypeObject = 'object';
// Internal key used on test_runner item details during transport.
const kAssertionPrototypeMetadata = 'assertionPrototypeMetadata';

function getName(object) {
  const desc = ObjectGetOwnPropertyDescriptor(object, 'name');
  return desc?.value;
}

function getAssertionError(error) {
  if (error === null || typeof error !== 'object') {
    return;
  }

  if (error.code === kTestFailureErrorCode) {
    return error.cause;
  }

  return error;
}

function getAssertionPrototype(value) {
  if (value === null || typeof value !== 'object') {
    return;
  }

  const prototype = ObjectGetPrototypeOf(value);
  if (prototype === null) {
    return;
  }

  const constructor = ObjectGetOwnPropertyDescriptor(prototype, 'constructor')?.value;
  if (typeof constructor !== 'function') {
    return;
  }

  const constructorName = getName(constructor);
  if (typeof constructorName !== 'string' || constructorName.length === 0) {
    return;
  }

  // Keep the scope narrow for this regression fix: only Array/Object values
  // are currently restored for AssertionError actual/expected.
  if (ArrayIsArray(value)) {
    if (constructorName === 'Array') {
      return;
    }

    return {
      __proto__: null,
      baseType: kBaseTypeArray,
      constructorName,
    };
  }

  if (ObjectPrototypeToString(value) === '[object Object]') {
    if (constructorName === 'Object') {
      return;
    }

    return {
      __proto__: null,
      baseType: kBaseTypeObject,
      constructorName,
    };
  }
}

function createSyntheticConstructor(name) {
  function constructor() {}
  ObjectDefineProperty(constructor, 'name', {
    __proto__: null,
    value: name,
    configurable: true,
  });
  return constructor;
}

function collectAssertionPrototypeMetadata(error) {
  const assertionError = getAssertionError(error);
  if (assertionError === null || typeof assertionError !== 'object' ||
      assertionError.code !== kAssertionErrorCode) {
    return;
  }

  const actual = getAssertionPrototype(assertionError.actual);
  const expected = getAssertionPrototype(assertionError.expected);
  if (!actual && !expected) {
    return;
  }

  return {
    __proto__: null,
    actual,
    expected,
  };
}

function applyAssertionPrototypeMetadata(error, metadata) {
  if (metadata === undefined || metadata === null || typeof metadata !== 'object') {
    return;
  }

  const assertionError = getAssertionError(error);
  if (assertionError === null || typeof assertionError !== 'object' ||
      assertionError.code !== kAssertionErrorCode) {
    return;
  }

  for (const key of ['actual', 'expected']) {
    const meta = metadata[key];
    const value = assertionError[key];
    const constructorName = meta?.constructorName;

    if (meta === undefined || meta === null || typeof meta !== 'object' ||
        value === null || typeof value !== 'object' ||
        typeof constructorName !== 'string') {
      continue;
    }

    if (meta.baseType === kBaseTypeArray && !ArrayIsArray(value)) {
      continue;
    }

    if (meta.baseType === kBaseTypeObject &&
        ObjectPrototypeToString(value) !== '[object Object]') {
      continue;
    }

    if (meta.baseType !== kBaseTypeArray && meta.baseType !== kBaseTypeObject) {
      continue;
    }

    const currentPrototype = ObjectGetPrototypeOf(value);
    const currentConstructor = currentPrototype === null ? undefined :
      ObjectGetOwnPropertyDescriptor(currentPrototype, 'constructor')?.value;
    if (typeof currentConstructor === 'function' &&
        getName(currentConstructor) === constructorName) {
      continue;
    }

    const basePrototype = meta.baseType === kBaseTypeArray ?
      ArrayPrototype :
      ObjectPrototype;

    try {
      const constructor = createSyntheticConstructor(constructorName);
      const syntheticPrototype = { __proto__: basePrototype };
      ObjectDefineProperty(syntheticPrototype, 'constructor', {
        __proto__: null,
        value: constructor,
        writable: true,
        enumerable: false,
        configurable: true,
      });
      constructor.prototype = syntheticPrototype;
      ObjectSetPrototypeOf(value, syntheticPrototype);
    } catch {
      // Best-effort only. If prototype restoration fails, keep the
      // deserialized value as-is and continue reporting.
    }
  }
}

module.exports = {
  applyAssertionPrototypeMetadata,
  collectAssertionPrototypeMetadata,
  kAssertionPrototypeMetadata,
};
