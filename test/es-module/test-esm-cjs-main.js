'use strict';

const { mustCall } = require('../common');
const fixtures = require('../common/fixtures.js');
const assert = require('node:assert');
const { execPath } = require('node:process');
const { describe, it } = require('node:test');


(async () => {
  const { default: spawn } = await import('./helper.spawnAsPromised.mjs');

  describe('ESM: importing CJS', () => {
    it('should work', async () => {
      const { code, signal, stdout } = await spawn(execPath, [
        fixtures.path('/es-modules/cjs.js'),
      ]);

      assert.strictEqual(code, 0);
      assert.strictEqual(signal, null);
      assert.strictEqual(stdout, 'executed\n');
    });
  });
})().then(mustCall());
