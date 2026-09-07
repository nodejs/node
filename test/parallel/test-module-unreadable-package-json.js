'use strict';

// A package.json that exists but cannot be read must not be treated as
// absent. Doing so silently drops fields such as "exports", which can resolve
// a specifier to a different file than the one the package declares.
// Refs: https://github.com/nodejs/node/issues/65220

const common = require('../common');

if (common.isWindows) {
  common.skip('chmod does not restrict reads on Windows');
}
if (process.getuid?.() === 0) {
  common.skip('cannot make a file unreadable as root');
}

const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');
const { spawnSyncAndAssert } = require('../common/child_process');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const depDir = tmpdir.resolve('node_modules/dep');
fs.mkdirSync(path.join(depDir, 'lib'), { recursive: true });
const depPackageJson = path.join(depDir, 'package.json');
fs.writeFileSync(
  depPackageJson,
  '{"name":"dep","exports":{".":"./lib/real.js"}}',
);
fs.writeFileSync(path.join(depDir, 'lib', 'real.js'), 'export const which = "real";');
// If the package config is ignored, resolution falls back to this file.
fs.writeFileSync(path.join(depDir, 'index.js'), 'export const which = "decoy";');

fs.writeFileSync(tmpdir.resolve('package.json'), '{"type":"module"}');
const entry = tmpdir.resolve('main.mjs');
fs.writeFileSync(entry, 'import { which } from "dep"; console.log(which);');

// Sanity check: the export resolves while the package config is readable.
spawnSyncAndAssert(process.execPath, [entry], { encoding: 'utf8' }, {
  stdout: 'real',
  trim: true,
});

fs.chmodSync(depPackageJson, 0o000);

try {
  const child = spawnSync(process.execPath, [entry], { encoding: 'utf8' });
  // The read failure must be reported rather than resolving to index.js.
  assert.doesNotMatch(child.stdout, /decoy/);
  assert.match(child.stderr, /Cannot read package config/);
  assert.notStrictEqual(child.status, 0);
} finally {
  fs.chmodSync(depPackageJson, 0o644);
}
