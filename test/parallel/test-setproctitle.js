'use strict';
// Original test written by Jakub Lekstan <kuebzky@gmail.com>
const common = require('../common');

// FIXME add sunos support
if (!(common.isFreeBSD || common.isOSX || common.isLinux)) {
  console.log(`1..0 # Skipped: Unsupported platform [${process.platform}]`);
  return;
}

const assert = require('assert');
const exec = require('child_process').exec;
const path = require('path');

// The title shouldn't be too long; libuv's uv_set_process_title() out of
// security considerations no longer overwrites envp, only argv, so the
// maximum title length is possibly quite short.
let title = 'testme';

assert.notStrictEqual(process.title, title);
process.title = title;
assert.strictEqual(process.title, title);

exec(`ps -p ${process.pid} -o args=`, function callback(error, stdout, stderr) {
  assert.ifError(error);
  assert.strictEqual(stderr, '');

  // freebsd always add ' (procname)' to the process title
  if (common.isFreeBSD)
    title += ` (${path.basename(process.execPath)})`;

  // omitting trailing whitespace and \n
  assert.strictEqual(stdout.replace(/\s+$/, ''), title);
});
