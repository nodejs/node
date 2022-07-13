'use strict';

const { mustCall } = require('../common');
const fixtures = require('../common/fixtures.js');
const assert = require('node:assert');
const { execPath } = require('node:process');
const { describe, it } = require('node:test');


(async () => {
  const { default: spawn } = await import('./helper.spawnAsPromised.mjs');

  describe('ESM: importing CJS', { concurrency: true }, () => {
    it('should support valid CJS exports', async () => {
      const validEntry = fixtures.path('/es-modules/cjs-exports.mjs');
      const { code, signal, stdout } = await spawn(execPath, [validEntry]);

      assert.strictEqual(code, 0);
      assert.strictEqual(signal, null);
      assert.strictEqual(stdout, 'ok\n');
    });

    it('should eror on invalid CJS exports', async () => {
      const invalidEntry = fixtures.path('/es-modules/cjs-exports-invalid.mjs');
      const { code, signal, stderr } = await spawn(execPath, [invalidEntry]);

      assert.strictEqual(code, 1);
      assert.strictEqual(signal, null);
      assert.ok(stderr.includes('Warning: To load an ES module'));
      assert.ok(stderr.includes('Unexpected token \'export\''));
    });
  });
})().then(mustCall());
