'use strict';
const common = require('../common');
common.skipIfInspectorDisabled();

// Regression test for https://github.com/nodejs/node/issues/27518.

const assert = require('assert');
const inspector = require('inspector');
const vm = require('vm');

const session = new inspector.Session();
session.connect();

const context = vm.createContext({
  a: 100
});

session.post('Runtime.evaluate', {
  expression: 'a',
  throwOnSideEffect: true,
  contextId: 2 // context's id
}, common.mustSucceed((res) => {
  assert.deepStrictEqual(res, {
    result: {
      type: 'number',
      value: context.a,
      description: '100'
    }
  });
}));
