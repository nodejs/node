'use strict';

const common = require('../common');
const assert = require('assert');
const { registerHooks } = require('module');

// Test that using syntax works.
{
  // eslint-disable-next-line no-unused-vars
  using hook = registerHooks({
    load: common.mustCall((url, context, nextLoad) => {
      const result = nextLoad(url, context);
      assert.strictEqual(result.source, '');
      return {
        source: 'export const hello = "world"',
      };
    }),
  });

  const mod = require('../fixtures/empty.js');
  assert.strictEqual(mod.hello, 'world');
}

delete require.cache[require.resolve('../fixtures/empty.js')];
const mod = require('../fixtures/empty.js');
assert.deepStrictEqual(mod, {});
