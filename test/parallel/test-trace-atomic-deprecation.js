'use strict';

const common = require('../common');
const assert = require('node:assert');
const { test } = require('node:test');

test('should emit deprecation warning DEP0165', async () => {
  const { code, stdout, stderr } = await common.spawnPromisified(
    process.execPath, ['--trace-atomics-wait', '-e', '{}']
  );
  assert.match(stderr, /\[DEP0165\] DeprecationWarning:/);
  assert.strictEqual(stdout, '');
  assert.strictEqual(code, 0);
});
