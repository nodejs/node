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
  {
    __proto__: {
      key: 'value',
      1: 'value',
    },
  },
];

for (const [idx, obj] of cases.entries()) {
  const ctx = vm.createContext(obj);
  const globalObj = vm.runInContext('this', ctx);
  const keys = Object.keys(globalObj);
  assert.deepStrictEqual(keys, Object.keys(obj), `Case ${idx} failed`);
}
