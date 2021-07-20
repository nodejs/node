'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const { spawn } = require('child_process');
const assert = require('assert');

// In a "type": "module" package scope, files with no extensions should
// not throw; both when used as a main entry point and also when
// referenced via `import`.

[
  '/es-modules/package-type-module/noext-esm',
  '/es-modules/package-type-module/imports-noext.mjs',
].forEach((fixturePath) => {
  const entry = fixtures.path(fixturePath);
  const child = spawn(process.execPath, [entry]);
  let stdout = '';
  let stderr = '';
  child.stderr.setEncoding('utf8');
  child.stdout.setEncoding('utf8');
  child.stdout.on('data', (data) => {
    stdout += data;
  });
  child.stderr.on('data', (data) => {
    stderr += data;
  });
  child.on('close', common.mustCall((code, signal) => {
    assert.strictEqual(code, 0);
    assert.strictEqual(signal, null);
    assert.strictEqual(stdout, 'executed\n');
    assert.ok(stderr.indexOf('ERR_UNKNOWN_FILE_EXTENSION') === -1);
  }));
});
