'use strict';

// This test verifies that when reading a package's `package.json` fails with a
// non-ENOENT/ENOTDIR error (e.g. EACCES, EISDIR, or an anti-malware/EDR deny),
// the module loader fails closed with ERR_ACCESS_DENIED instead
// of treating the manifest as absent and silently falling back to `index.js`.
//
// The unreadable-file case relies on chmod(0), which only produces EACCES
// for a non-root user on POSIX systems.

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

{
  const dirPkg = path.join(tmpdir.path, 'node_modules', 'directory-manifest');
  const manifest = path.join(dirPkg, 'package.json');
  const marker = path.join(tmpdir.path, 'directory-loaded.marker');
  fs.mkdirSync(manifest, { recursive: true });
  fs.writeFileSync(
    path.join(dirPkg, 'index.js'),
    `require('fs').writeFileSync(${JSON.stringify(marker)}, 'loaded');\n`);

  // Some platforms can read directories, so only test this error when the
  // filesystem actually reports EISDIR.
  let directoryReadError;
  try {
    fs.readFileSync(manifest);
  } catch (err) {
    directoryReadError = err;
  }
  if (directoryReadError) {
    assert.strictEqual(directoryReadError.code, 'EISDIR');
    assert.throws(() => require(dirPkg), { code: 'ERR_ACCESS_DENIED' });
    assert.strictEqual(fs.existsSync(marker), false);

    // A failed read must not be cached as an absent package config.
    fs.rmdirSync(manifest);
    fs.writeFileSync(manifest, '{"main":"real.js"}');
    fs.writeFileSync(path.join(dirPkg, 'real.js'), 'module.exports = 42;\n');
    assert.strictEqual(require(dirPkg), 42);
    assert.strictEqual(fs.existsSync(marker), false);
  }
}

if (common.isWindows) {
  common.skip('chmod(0) does not produce EACCES on Windows');
}
if (!process.getuid || process.getuid() === 0) {
  common.skip('cannot produce EACCES as root');
}

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

try {
  assert.throws(
    () => require(pkgDir),
    { code: 'ERR_ACCESS_DENIED' },
    'requiring a package whose package.json read is denied must fail closed');

  assert.strictEqual(fs.existsSync(markerPath), false);
} finally {
  fs.chmodSync(pkgJsonPath, 0o644);
}
