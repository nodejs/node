'use strict';
// Original test written by Jakub Lekstan <kuebzky@gmail.com>
const common = require('../common');

// FIXME add sunos support
if (common.isSunOS)
  common.skip(`Unsupported platform [${process.platform}]`);

const assert = require('assert');
const { exec, spawn } = require('child_process');
const path = require('path');
// The title shouldn't be too long; libuv's uv_set_process_title() out of
// security considerations no longer overwrites envp, only argv, so the
// maximum title length is possibly quite short (probably just 4 chars).
// We need to use a random shared secret so zombie processes do not cause a
// false positive.
// Ref: https://github.com/nodejs/node/pull/12792
const title = String(Date.now()).substr(-4, 4);

if (process.argv[2] === 'child') {
  const childTitle = process.argv[3];
  assert.notStrictEqual(process.title, childTitle);
  process.title = childTitle;
  assert.strictEqual(process.title, childTitle);
  // Just wait a little bit before exiting.
  setTimeout(() => {}, common.platformTimeout(3000));
  return;
}

// This should run only in the parent.
assert.strictEqual(process.argv.length, 2);
let child;
const cmd = (() => {
  if (common.isWindows) {
    // On Windows it is the `window` that gets the title and not the actual
    // process, so we create a new `window` by using `shell: true` and `start`.
    child = spawn(
      'start /MIN /WAIT',
      [process.execPath, __filename, 'child', title],
      { shell: true, stdio: 'ignore' },
    );
    const PSL = 'Powershell -NoProfile -ExecutionPolicy Unrestricted -Command';
    const winTitle = '$_.mainWindowTitle';
    return `${PSL} "ps | ? {${winTitle} -eq '${title}'} | % {${winTitle}}"`;
  } else {
    // For non Windows we just change this process's own title.
    assert.notStrictEqual(process.title, title);
    process.title = title;
    assert.strictEqual(process.title, title);

    // Since Busybox's `ps` does not support `-p` switch, to pass this test on
    // alpine we use `ps -o` and `grep`.
    return common.isLinux ?
      `ps -o pid,args | grep '${process.pid} ${title}' | grep -v grep` :
      `ps -p ${child.pid} -o args=`;
  }
})();

exec(cmd, common.mustCall((error, stdout, stderr) => {
  // Don't need the child process anymore...
  if (child) {
    child.kill();
    child.unref();
  }
  assert.ifError(error);
  assert.strictEqual(stderr, '');
  // We only want the second part (remove the pid).
  const ret = stdout.trim().replace(/^\d+\s/, '');

  // `freeBSD` adds ' (procname)' to the process title.
  const procname = path.basename(process.execPath);
  const expected = title + (common.isFreeBSD ? ` (${procname})` : '');
  assert.strictEqual(ret, expected);
}));
