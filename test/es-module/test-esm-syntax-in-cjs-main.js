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
  it('should throw SyntaxError when main module has ESM syntax in type:commonjs package', async () => {
    const mainScript = fixtures.path('es-modules/package-type-commonjs-esm-syntax/esm-script.js');
    const { code, signal, stderr } = await spawnPromisified(execPath, [mainScript]);

    // Should exit with non-zero exit code
    assert.strictEqual(code, 1, `Expected exit code 1, got ${code}`);
    assert.strictEqual(signal, null);

    // Should contain SyntaxError about import statement
    assert.match(stderr, /SyntaxError/,
      'Expected error to be a SyntaxError');
    assert.match(stderr, /Cannot use import statement outside a module/,
      'Expected error message about import statement');
    assert.match(stderr, /esm-script\.js/,
      'Expected error message to mention the script filename');
  });
});
