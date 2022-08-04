'use strict';

const { spawnPromisified } = require('../common');
const fixtures = require('../common/fixtures.js');
const assert = require('node:assert');
const { execPath } = require('node:process');
const { describe, it } = require('node:test');


const entry = fixtures.path('/es-modules/builtin-imports-case.mjs');

describe('ESM: importing builtins & CJS', () => {
  it('should work', async () => {
    const { code, signal, stdout } = await spawnPromisified(execPath, [entry]);

    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
    assert.strictEqual(stdout, 'ok\n');
  });
});
