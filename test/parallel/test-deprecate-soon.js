'use strict';

// Tests the --deprecate-soon command line option and warning emission.

const common = require('../common');
const assert = require('assert');
const spawnSync = require('child_process').spawnSync;
const deprecateSoon = process.binding('config').deprecateSoon;

if (process.argv[2] === 'child') {
  const util = require('internal/util');
  const fn = util.deprecateSoon(() => {}, 'this will be deprecated soon');

  const count = deprecateSoon ? 1 : 0;
  process.on('warning', common.mustCall((warning) => {
    assert.strictEqual(warning.name, 'DeprecateSoonWarning');
    assert.strictEqual(warning.message, 'this will be deprecated soon');
  }, count));
  fn();
} else {
  const options = [
    '--expose-internals',
    __filename,
    'child'
  ];

  // Do not emit the deprecate soon warning
  assert.strictEqual(
    spawnSync(process.execPath, options).status, 0,
    'deprecation warning was printed');

  options.unshift('--deprecate-soon');

  assert.strictEqual(
    spawnSync(process.execPath, options).status, 0,
    'deprecation warning was not printed properly');
}
