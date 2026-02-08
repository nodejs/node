'use strict';

// Test that process.cwd() throws a descriptive error when the current
// working directory has been deleted. Regression test for
// https://github.com/nodejs/node/issues/57045

const common = require('../common');

// This test is not meaningful on Windows because Windows does not allow
// deleting a directory that is the cwd of a running process.
if (common.isWindows) {
  common.skip('Windows does not allow deleting cwd of a running process');
}

const assert = require('assert');
const { execFileSync } = require('child_process');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const dir = path.join(tmpdir.path, 'cwd-deleted');
fs.mkdirSync(dir);

// Spawn a child process that chdir's into the temp directory, deletes it,
// then attempts process.cwd(). The error should be descriptive.
const child = execFileSync(process.execPath, [
  '-e',
  `process.chdir(${JSON.stringify(dir)});
   require('fs').rmSync(${JSON.stringify(dir)}, { recursive: true });
   try {
     process.cwd();
     process.exit(1); // Should not reach here
   } catch (err) {
     process.stdout.write(JSON.stringify({
       code: err.code,
       syscall: err.syscall,
       message: err.message,
     }));
   }`,
], { encoding: 'utf8' });

const parsed = JSON.parse(child.trim());

assert.strictEqual(parsed.code, 'ENOENT');
assert.strictEqual(parsed.syscall, 'process.cwd');
assert.match(parsed.message, /current working directory/i);
assert.match(parsed.message, /does not exist/i);
