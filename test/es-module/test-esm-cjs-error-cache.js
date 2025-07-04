'use strict';

const { spawnPromisified } = require('../common');
const fixtures = require('../common/fixtures.js');
const assert = require('node:assert');
const { execPath } = require('node:process');
const { describe, it } = require('node:test');


describe('CJS ↔︎ ESM preserve errors', () => {
  it('should preserve errors when CJS import first', async () => {
    const validEntry = fixtures.path('/es-modules/cjs-esm-runtime-error.js');
    const { code, signal, stdout } = await spawnPromisified(execPath, [validEntry]);
    assert.strictEqual(stdout, 'CJS_OK\nESM_OK\n');
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });

  it('should preserve errors when ESM import first', async () => {
    const validEntry = fixtures.path('/es-modules/esm-cjs-runtime-error.js');
    const { code, signal, stdout } = await spawnPromisified(execPath, [validEntry]);
    assert.strictEqual(stdout, 'ESM_OK\nCJS_OK\n');
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
  });
});
