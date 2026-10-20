'use strict';
require('../common');
const { test, suite } = require('node:test');

suite('test runner option precedence', () => {
  test(
    'overridden test name',
    { name: 'options.name overrides test name', plan: 1 },
    (t) => {
      t.assert.strictEqual(t.name, 'options.name overrides test name');
    },
  );

  test(
    'options.fn overrides test function',
    {
      fn: (t) => {
        t.assert.ok(true);
      },
      plan: 1,
    },
    (t) => {
      t.assert.fail('should not be called');
    },
  );

  test('options.fn only', {
    plan: 1,
    fn: (t) => {
      t.assert.ok(true);
    },
  });

  test({
    name: 'single parameter options',
    plan: 1,
    fn: (t) => {
      t.assert.ok(true);
    },
  });
});
