'use strict';

const { mustCall } = require('../common');
const fixtures = require('../common/fixtures.js');
const assert = require('node:assert');
const { execPath } = require('node:process');
const { describe, it } = require('node:test');


const entry = fixtures.path('/es-modules/builtin-imports-case.mjs');

(async () => {
  const { default: spawn } = await import('./helper.spawnAsPromised.mjs');

  describe('ESM: importing builtins & CJS', () => {
    it('should work', async () => {
      const { code, signal, stdout } = await spawn(execPath, [entry]);

      assert.strictEqual(code, 0);
      assert.strictEqual(signal, null);
      assert.strictEqual(stdout, 'ok\n');
    });
  });
})().then(mustCall());
