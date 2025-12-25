// Flags: --no-warnings
'use strict';

// Test that running a main entry point with ESM syntax in a "type": "commonjs"
// package throws an error instead of silently failing with exit code 0.
// Regression test for https://github.com/nodejs/node/issues/61104

const { spawnPromisified } = require('../common');
const fixtures = require('../common/fixtures.js');
const assert = require('node:assert');
const { execPath } = require('node:process');
const { describe, it } = require('node:test');

describe('ESM syntax in explicit CommonJS main entry point', { concurrency: !process.env.TEST_PARALLEL }, () => {
  it('should throw ERR_REQUIRE_ESM when main module has ESM syntax in type:commonjs package', async () => {
    const mainScript = fixtures.path('es-modules/package-type-commonjs-esm-syntax/esm-script.js');
    const { code, signal, stderr } = await spawnPromisified(execPath, [mainScript]);

    // Should exit with non-zero exit code
    assert.strictEqual(code, 1, `Expected exit code 1, got ${code}`);
    assert.strictEqual(signal, null);

    // Should contain error about ESM in CommonJS context
    assert.match(stderr, /ERR_REQUIRE_ESM/,
      'Expected error message to contain ERR_REQUIRE_ESM');
    assert.match(stderr, /esm-script\.js/,
      'Expected error message to mention the script filename');
  });

  it('should include helpful message about package.json type field', async () => {
    const mainScript = fixtures.path('es-modules/package-type-commonjs-esm-syntax/esm-script.js');
    const { stderr } = await spawnPromisified(execPath, [mainScript]);

    // Error message should mention the package.json path for helpful debugging
    assert.match(stderr, /package\.json/,
      'Expected error message to mention package.json');
  });
});
