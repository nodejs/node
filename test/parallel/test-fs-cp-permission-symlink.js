// Flags: --experimental-permission --allow-fs-read=* --allow-fs-write=*
'use strict';

const common = require('../common');

if (!common.hasCrypto) common.skip('missing crypto');

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { execFileSync } = require('child_process');

// This test verifies that fs.cpSync checks symlink target permissions
// when copying directories containing symlinks.
// Regression test for incomplete CVE-2025-55130 fix.

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const allowedDir = path.join(tmpdir.path, 'allowed');
const deniedDir = path.join(tmpdir.path, 'denied');
const srcDir = path.join(allowedDir, 'src');
const destDir = path.join(allowedDir, 'dest');
const secretFile = path.join(deniedDir, 'secret.txt');

// Setup directories
fs.mkdirSync(srcDir, { recursive: true });
fs.mkdirSync(destDir, { recursive: true });
fs.mkdirSync(deniedDir, { recursive: true });
fs.writeFileSync(secretFile, 'SECRET_DATA');

// Create symlink pointing outside allowed path
fs.symlinkSync(secretFile, path.join(srcDir, 'link'));

// Run with restricted permissions — only allowedDir is permitted
const result = execFileSync(process.execPath, [
  '--experimental-permission',
  `--allow-fs-read=${allowedDir}`,
  `--allow-fs-write=${allowedDir}`,
  '--allow-fs-read=/usr',
  '--allow-fs-read=/lib',
  '-e',
  `
    const fs = require('node:fs');
    try {
      fs.cpSync('${srcDir}/', '${destDir}/', { recursive: true });
      console.log('FAIL');
    } catch(e) {
      console.log(e.code);
    }
  `,
], { encoding: 'utf8', stdio: ['pipe', 'pipe', 'pipe'] }).trim();

// cpSync should throw ERR_ACCESS_DENIED because symlink target
// (/tmp/.../denied/secret.txt) is outside allowed paths
assert.strictEqual(
  result,
  'ERR_ACCESS_DENIED',
  `Expected ERR_ACCESS_DENIED but got: ${result}`
);

// Also test verbatimSymlinks path
const destDir2 = path.join(allowedDir, 'dest2');
fs.mkdirSync(destDir2, { recursive: true });

const result2 = execFileSync(process.execPath, [
  '--experimental-permission',
  `--allow-fs-read=${allowedDir}`,
  `--allow-fs-write=${allowedDir}`,
  '--allow-fs-read=/usr',
  '--allow-fs-read=/lib',
  '-e',
  `
    const fs = require('node:fs');
    try {
      fs.cpSync('${srcDir}/', '${destDir2}/', {
        recursive: true,
        verbatimSymlinks: true
      });
      console.log('FAIL');
    } catch(e) {
      console.log(e.code);
    }
  `,
], { encoding: 'utf8', stdio: ['pipe', 'pipe', 'pipe'] }).trim();

assert.strictEqual(
  result2,
  'ERR_ACCESS_DENIED',
  `verbatimSymlinks: Expected ERR_ACCESS_DENIED but got: ${result2}`
);

console.log('All permission checks passed.');
