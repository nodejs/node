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
// maximum title length is possibly quite short.
const title = String(Date.now()).substr(-4, 4);

if (process.argv[2] === 'child') {
  const childTitle = process.argv[3];
  assert.notStrictEqual(process.title, childTitle);
  process.title = childTitle;
  assert.strictEqual(process.title, childTitle);
  // Just wait a little bit before exiting
  setTimeout(() => {}, common.platformTimeout(300));
  return;
}

// This should run only in the parent
assert.strictEqual(process.argv.length, 2);
let child;
const cmd = (() => {
  if (common.isWindows) {
    // For windows we need to spawn a new `window` so it will get the title.
    // For that we need a `shell`, and to use `start`
    child = spawn(
      'start /MIN /WAIT',
      [process.execPath, __filename, 'child', title],
      { shell: true, stdio: 'ignore' },
    );
    const PSL = 'Powershell -NoProfile -ExecutionPolicy Unrestricted -Command';
    const winTitle = '$_.mainWindowTitle';
    return `${PSL} "ps | ? {${winTitle} -eq '${title}'} | % {${winTitle}}"`;
  } else {
    // for non Windows we change our own title
    assert.notStrictEqual(process.title, title);
    process.title = title;
    assert.strictEqual(process.title, title);

    // To pass this test on alpine, since Busybox `ps` does not
    // support `-p` switch, use `ps -o` and `grep` instead.
    return common.isLinux ?
      `ps -o pid,args | grep '${process.pid} ${title}' | grep -v grep` :
      `ps -p ${child.pid} -o args=`;
  }
})();

exec(cmd, common.mustCall((error, stdout, stderr) => {
  // We don't need the child anymore...
  if (child) {
    child.kill();
    child.unref();
  }
  assert.ifError(error);
  assert.strictEqual(stderr, '');
  const ret = stdout.trim();

  // freeBSD adds ' (procname)' to the process title
  const bsdSuffix = ` (${path.basename(process.execPath)})`;
  const expected = title + (common.isFreeBSD ? bsdSuffix : '');
  assert.strictEqual(ret.endsWith(expected), true);
  assert.strictEqual(ret.endsWith(expected), true);
}));
