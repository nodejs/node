'use strict';

const { checkoutEOL, spawnPromisified } = require('../common');
const fixtures = require('../common/fixtures.js');
const assert = require('node:assert');
const { execPath } = require('node:process');
const { describe, it } = require('node:test');


describe('ESM: Package.json', { concurrency: true }, () => {
  it('should throw on invalid pson', async () => {
    const entry = fixtures.path('/es-modules/import-invalid-pjson.mjs');
    const invalidJson = fixtures.path('/node_modules/invalid-pjson/package.json');

    const { code, signal, stderr } = await spawnPromisified(execPath, [entry]);

    assert.ok(
      stderr.includes(
        `[ERR_INVALID_PACKAGE_CONFIG]: Invalid package config ${invalidJson} ` +
        `while importing "invalid-pjson" from ${entry}. ` +
        "Expected ':' after property name in JSON at position " +
        `${12 + checkoutEOL.length * 2}`
      ),
      stderr
    );
    assert.strictEqual(code, 1);
    assert.strictEqual(signal, null);
  });
});
