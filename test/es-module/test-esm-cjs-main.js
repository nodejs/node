'use strict';

const { spawnPromisified } = require('../common');
const fixtures = require('../common/fixtures.js');
const assert = require('node:assert');
const { execPath } = require('node:process');
const { describe, it } = require('node:test');


describe('ESM: importing CJS', () => {
  it('should work', async () => {
    const { code, signal, stdout } = await spawnPromisified(execPath, [
      fixtures.path('/es-modules/cjs.js'),
    ]);

    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
    assert.strictEqual(stdout, 'executed\n');
  });
});
