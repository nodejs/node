'use strict';

const common = require('../common');
const assert = require('assert');
const path = require('path');
const spawn = require('child_process').spawn;

// FIXME(bnoordhuis) On UNIX platforms, the debugger doesn't reliably kill
// the inferior when killed by a signal.  Work around that by spawning
// the debugger in its own process group and killing the process group
// instead of just the debugger process.
const detached = !common.isWindows;

const children = [];
for (let i = 0; i < 4; i += 1) {
  const port = common.PORT + i;
  const args = [`--debug-port=${port}`, '--interactive', 'debug', __filename];
  const child = spawn(process.execPath, args, { detached, stdio: 'pipe' });
  child.test = { port: port, stdout: '' };
  child.stdout.setEncoding('utf8');
  child.stdout.on('data', function(s) { child.test.stdout += s; update(); });
  child.stdout.pipe(process.stdout);
  child.stderr.pipe(process.stderr);
  children.push(child);
}

function update() {
  // Debugger prints relative paths except on Windows.
  const filename = path.basename(__filename);

  let ready = 0;
  for (const child of children)
    ready += RegExp(`break in .*?${filename}:1`).test(child.test.stdout);

  if (ready === children.length)
    for (const child of children)
      kill(child);
}

function kill(child) {
  if (!detached)
    return child.kill();

  try {
    process.kill(-child.pid);  // Kill process group.
  } catch (e) {
    // Generally ESRCH is returned when the process group is already gone. On
    // some platforms such as OS X it may be EPERM though.
    assert.ok((e.code === 'EPERM') || (e.code === 'ESRCH'));
  }
}

process.on('exit', function() {
  for (const child of children) {
    const { port, stdout } = child.test;
    assert(stdout.includes(`Debugger listening on 127.0.0.1:${port}`));
    assert(stdout.includes(`connecting to 127.0.0.1:${port}`));
  }
});
