'use strict';

// Tests the --deprecated-in-docs command line option and warning emission.

const common = require('../common');
const assert = require('assert');
const spawnSync = require('child_process').spawnSync;
const deprecateInDocs = process.binding('config').deprecateInDocs;

if (process.argv[2] === 'child') {
  const util = require('internal/util');
  const fn = util.deprecatedInDocs(() => {}, 'this is deprecated in the docs');

  process.on('warning', common.mustCall((warning) => {
    assert.strictEqual(warning.name, 'DeprecatedInDocsWarning');
    assert.strictEqual(warning.message, 'this is deprecated in the docs');
  }, deprecateInDocs ? 1 : 0));
  fn();
} else {
  const options = [
    '--expose-internals',
    __filename,
    'child'
  ];

  // Do not emit the deprecated in docs warning
  assert.strictEqual(
    spawnSync(process.execPath, options).status, 0,
    'deprecation warning was printed');

  options.unshift('--deprecated-in-docs');

  // Emit the deprecated in docs warning
  assert.strictEqual(
    spawnSync(process.execPath, options).status, 0,
    'deprecation warning was not printed properly');
}
