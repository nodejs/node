'use strict';

const { spawnPromisified } = require('../common');
const fixtures = require('../common/fixtures.js');
const assert = require('node:assert');
const { execPath } = require('node:process');
const { describe, it } = require('node:test');


describe('ESM: importing an encoded path', () => {
  it('should throw', async () => {
    const { code } = await spawnPromisified(execPath, [
      fixtures.path('es-module-url/native.mjs'),
    ]);

    assert.strictEqual(code, 1);
  });
});
