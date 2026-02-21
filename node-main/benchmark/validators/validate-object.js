'use strict';

const common = require('../common');
const assert = require('assert');

const bench = common.createBenchmark(main, {
  n: [1e5],
  objectToTest: [
    'object',
    'null',
    'array',
    'function',
  ],
}, {
  flags: ['--expose-internals'],
});

function getObjectToTest(objectToTest) {
  switch (objectToTest) {
    case 'object':
      return { foo: 'bar' };
    case 'null':
      return null;
    case 'array':
      return ['foo', 'bar'];
    case 'function':
      return () => 'foo';
    default:
      throw new Error(`Value ${objectToTest} is not a valid objectToTest.`);
  }
}

function getOptions(objectToTest) {
  const {
    kValidateObjectAllowNullable,
    kValidateObjectAllowArray,
    kValidateObjectAllowFunction,
  } = require('internal/validators');

  switch (objectToTest) {
    case 'object':
      return 0;
    case 'null':
      return kValidateObjectAllowNullable;
    case 'array':
      return kValidateObjectAllowArray;
    case 'function':
      return kValidateObjectAllowFunction;
    default:
      throw new Error(`Value ${objectToTest} is not a valid objectToTest.`);
  }
}

let _validateResult;

function main({ n, objectToTest }) {
  const {
    validateObject,
  } = require('internal/validators');

  const value = getObjectToTest(objectToTest);
  const options = getOptions(objectToTest);

  bench.start();
  for (let i = 0; i < n; ++i) {
    try {
      _validateResult = validateObject(value, 'Object', options);
    } catch {
      _validateResult = undefined;
    }
  }
  bench.end(n);

  assert.ok(!_validateResult);
}
