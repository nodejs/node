'use strict';

// This test verifies that when reading a package's `package.json` fails with a
// non-ENOENT error (e.g. EACCES, or an anti-malware/EDR deny that surfaces as a
// blocked read), the module loader fails closed with ERR_ACCESS_DENIED instead
// of treating the manifest as absent and silently falling back to `index.js`.
//
// It relies on chmod(0) to make the file unreadable, which only produces EACCES
// for a non-root user on POSIX systems, so it is skipped on Windows and when
// running as root.

const common = require('../common');

if (common.isWindows) {
  common.skip('chmod(0) does not produce EACCES on Windows');
}
if (!process.getuid || process.getuid() === 0) {
  common.skip('cannot produce EACCES as root');
}

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const pkgDir = path.join(tmpdir.path, 'node_modules', 'evil');
fs.mkdirSync(pkgDir, { recursive: true });

// A default-`index.js` package: before the fix, a denied manifest was swallowed
// (index.js fallback) and this code would run anyway.
const indexPath = path.join(pkgDir, 'index.js');
const markerPath = path.join(tmpdir.path, 'loaded.marker');
fs.writeFileSync(
  indexPath,
  `require('fs').writeFileSync(${JSON.stringify(markerPath)}, 'loaded');\n`);

const pkgJsonPath = path.join(pkgDir, 'package.json');
fs.writeFileSync(pkgJsonPath, JSON.stringify({ name: 'evil', version: '1.0.0' }));

// Deny reads of the manifest to simulate a quarantine/anti-malware block.
fs.chmodSync(pkgJsonPath, 0o000);

// Sanity check: the deny actually took effect (otherwise the test is moot).
try {
  fs.readFileSync(pkgJsonPath);
  common.skip('environment allowed reading a chmod(0) file');
} catch (err) {
  assert.strictEqual(err.code, 'EACCES');
}

assert.throws(
  () => require(pkgDir),
  (err) => {
    assert.strictEqual(err.code, 'ERR_ACCESS_DENIED');
    return true;
  },
  'requiring a package whose package.json read is denied must fail closed');

// The index.js fallback must NOT have executed.
assert.strictEqual(
  fs.existsSync(markerPath), false,
  'index.js fallback ran despite the manifest being denied');

// Restore perms so tmpdir cleanup can remove the file.
fs.chmodSync(pkgJsonPath, 0o644);
