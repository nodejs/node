'use strict';

const { mustCall } = require('../common');
const fixtures = require('../common/fixtures.js');
const assert = require('node:assert');
const { execPath } = require('node:process');
const { describe, it } = require('node:test');


(async () => {
  const { default: spawn } = await import('./helper.spawnAsPromised.mjs');

  describe('ESM: importing an encoded path', () => {
    it('should throw', async () => {
      const { code } = await spawn(execPath, [
        fixtures.path('es-module-url/native.mjs'),
      ]);

      assert.strictEqual(code, 1);
    });
  });
})().then(mustCall());
