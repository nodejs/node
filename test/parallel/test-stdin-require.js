'use strict';

const common = require('../common');
const assert = require('assert');
const execSync = require('child_process').execSync;
const fs = require('fs');
const path = require('path');

// Prevents a regression where redirecting stderr and receiving input script
// via stdin works properly.

common.refreshTmpDir();
const filename = path.join(common.fixturesDir, 'baz.js');
const out = path.join(common.tmpDir, 'js.out');
const bin = process.execPath;
const input = `require('${filename}'); console.log('PASS');`;
const cmd = common.isWindows ?
  `echo "${input}" | ${bin} > ${out} 2>&1` :
  `echo "${input}" | ${bin} &> ${out}`;

// This will throw if internal/fs has a circular dependency.
assert.strictEqual(execSync(cmd).toString(), '');

const result = fs.readFileSync(out, 'utf8').trim();
assert.strictEqual(result, 'PASS');

fs.unlinkSync(out);
