'use strict';

// Test that process.permission.has()/drop() accept a URL or Uint8Array reference.

require('../common');
const { spawnSync } = require('child_process');
const assert = require('assert');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  process.exit(0);
}

const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const fs = require('fs');
const path = require('path');

tmpdir.refresh();

const allowedDir = path.join(tmpdir.path, 'allowed');
const deniedDir = path.join(tmpdir.path, 'denied');
fs.mkdirSync(allowedDir);
fs.mkdirSync(deniedDir);

const allowedFile = path.join(allowedDir, 'a.txt');
const deniedFile = path.join(deniedDir, 'b.txt');
fs.writeFileSync(allowedFile, 'allowed');
fs.writeFileSync(deniedFile, 'denied');

const { status, stderr } = spawnSync(
  process.execPath,
  [
    '--permission',
    `--allow-fs-read=${allowedDir}`,
    fixtures.path('permission', 'has-reference-types.js'),
  ],
  {
    env: {
      ...process.env,
      ALLOWED_DIR: allowedDir,
      ALLOWED_FILE: allowedFile,
      DENIED_FILE: deniedFile,
    },
  },
);

assert.strictEqual(status, 0, stderr.toString());
