'use strict';

const common = require('../common');
if (!common.isWindows) {
  common.skip('This test is Windows-specific.');
}

// Verify that the JavaScript realpath implementation accepts namespaced drive
// paths, including when a junction switches the walk back to a regular drive
// path, and reports a missing entry instead of treating the drive as a file.

const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');
const { test } = require('node:test');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const entry = tmpdir.resolve('entry.js');
const namespacedEntry = path.toNamespacedPath(entry);
const namespacedMissing = path.toNamespacedPath(tmpdir.resolve('missing.js'));
const targetDir = tmpdir.resolve('target');
const targetEntry = path.join(targetDir, 'entry.js');
const junctionDir = tmpdir.resolve('junction');
const namespacedJunctionEntry = path.toNamespacedPath(
  path.join(junctionDir, 'entry.js'),
);

fs.writeFileSync(entry, '');
fs.mkdirSync(targetDir);
fs.writeFileSync(targetEntry, '');
fs.symlinkSync(targetDir, junctionDir, 'junction');

function assertNamespacedRealpath(result) {
  assert.strictEqual(path.toNamespacedPath(result), namespacedEntry);
}

test('fs.realpathSync resolves a namespaced drive path', () => {
  assertNamespacedRealpath(fs.realpathSync(namespacedEntry));
});

test('fs.realpathSync reports ENOENT for a missing namespaced drive path', () => {
  assert.throws(() => fs.realpathSync(namespacedMissing), { code: 'ENOENT' });
});

test('fs.realpathSync resolves a namespaced path through a junction', () => {
  assert.strictEqual(fs.realpathSync(namespacedJunctionEntry), targetEntry);
});

test('fs.realpath resolves a namespaced drive path', (t, done) => {
  fs.realpath(namespacedEntry, common.mustSucceed((result) => {
    assertNamespacedRealpath(result);
    done();
  }));
});

test('fs.realpath resolves a namespaced path through a junction', (t, done) => {
  fs.realpath(namespacedJunctionEntry, common.mustSucceed((result) => {
    assert.strictEqual(result, targetEntry);
    done();
  }));
});
