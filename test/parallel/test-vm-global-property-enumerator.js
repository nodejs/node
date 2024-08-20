'use strict';
require('../common');
const vm = require('vm');
const assert = require('assert');

// Regression of https://github.com/nodejs/node/issues/53346

const cases = [
  {
    get 1() {
      return 'value';
    },
    get key() {
      return 'value';
    },
  },
  {
    // Intentionally single setter.
    // eslint-disable-next-line accessor-pairs
    set key(value) {},
    // eslint-disable-next-line accessor-pairs
    set 1(value) {},
  },
  {},
  {
    key: 'value',
    1: 'value',
  },
  (new class GetterObject {
    get key() {
      return 'value';
    }
    get 1() {
      return 'value';
    }
  }()),
  (new class SetterObject {
    // Intentionally single setter.
    // eslint-disable-next-line accessor-pairs
    set key(value) {
      // noop
    }
    // eslint-disable-next-line accessor-pairs
    set 1(value) {
      // noop
    }
  }()),
  [],
  [['key', 'value']],
];

for (const [idx, obj] of cases.entries()) {
  const ctx = vm.createContext(obj);
  const globalObj = vm.runInContext('this', ctx);
  const keys = Object.keys(globalObj);
  assert.deepStrictEqual(keys, Object.keys(obj), `Case ${idx} failed`);
}

const specialCases = [
  [
    // Prototype named and index properties should not be enumerated.
    // FIXME(legendecas): https://github.com/nodejs/node/issues/54436
    {
      __proto__: {
        key: 'value',
        1: 'value',
      },
    },
    ['1', 'key'],
  ],
];
for (const [idx, [obj, expectedKeys]] of specialCases.entries()) {
  const ctx = vm.createContext(obj);
  const globalObj = vm.runInContext('this', ctx);
  const keys = Object.keys(globalObj);
  assert.deepStrictEqual(keys, expectedKeys, `Special case ${idx} failed`);
}
