'use strict';

// The permission model resolves paths to avoid path traversals, but in doing so
// it potentially interprets paths differently than the operating system would.
// This test demonstrates that merely enabling the permission model causes the
// application to potentially access a different file than it would without the
// permission model.

const common = require('../common');

const assert = require('assert');
const { execFileSync } = require('child_process');
const { mkdirSync, symlinkSync, writeFileSync } = require('fs');
const path = require('path');

if (common.isWindows)
  assert.fail('not applicable to Windows');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const a = path.join(tmpdir.path, 'a');
const b = path.join(tmpdir.path, 'b');
const c = path.join(tmpdir.path, 'c');
const d = path.join(tmpdir.path, 'c/d');

writeFileSync(a, 'bad');
symlinkSync('c/d', b);
mkdirSync(c);
mkdirSync(d);
writeFileSync(path.join(c, 'a'), 'good');

function run(...args) {
  const interestingPath = `${tmpdir.path}/b/../a`;
  args = [...args, '-p', `fs.readFileSync(${JSON.stringify(interestingPath)}, 'utf8')`];
  return execFileSync(process.execPath, args, { encoding: 'utf8' }).trim();
}

// Because this is a known_issues test, we cannot assert any assumptions besides
// the known issue itself. Instead, do a sanity check and report success if the
// sanity check fails.
if (run() !== 'good') {
  process.exit(0);
}

assert.strictEqual(run('--experimental-permission', `--allow-fs-read=${tmpdir.path}`), 'good');
