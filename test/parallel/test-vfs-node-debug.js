'use strict';

const assert = require('assert');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { spawnSync } = require('child_process');

const mountPoint = path.join(os.tmpdir(), `vfs-debug-${process.pid}`);
const entryPoint = path.join(mountPoint, 'answer.js');
const dataPath = path.join(mountPoint, 'data.txt');
const escapeRegExp = (str) => str.replace(/[|\\{}()[\]^$+?.]/g, '\\$&');

const script = `
  const assert = require('assert');
  const fs = require('fs');
  const vfs = require('node:vfs');

  const mountPoint = ${JSON.stringify(mountPoint)};
  const entryPoint = ${JSON.stringify(entryPoint)};
  const dataPath = ${JSON.stringify(dataPath)};

  const myVfs = vfs.create({ emitExperimentalWarning: false });
  myVfs.writeFileSync('/answer.js', 'module.exports = 42;');
  myVfs.writeFileSync('/data.txt', 'hello from vfs');
  myVfs.mount(mountPoint);

  assert.strictEqual(fs.readFileSync(dataPath, 'utf8'), 'hello from vfs');
  assert.strictEqual(require(entryPoint), 42);

  myVfs.unmount();
`;

const child = spawnSync(process.execPath, ['-e', script], {
  env: {
    ...process.env,
    NODE_DEBUG: 'vfs',
  },
  encoding: 'utf8',
});

assert.strictEqual(child.status, 0, child.stderr);
assert.strictEqual(child.stdout, '');
assert.match(child.stderr, /VFS \d+: install hooks/);
assert.match(child.stderr, new RegExp(`VFS \\d+: mount ${escapeRegExp(mountPoint)} overlay=false moduleHooks=true virtualCwd=false`));
assert.match(child.stderr, new RegExp(`VFS \\d+: register mount=${escapeRegExp(mountPoint)} overlay=false active=1`));
assert.match(child.stderr, new RegExp(`VFS \\d+: read ${escapeRegExp(dataPath)} -> hit \\(mount=${escapeRegExp(mountPoint)} overlay=false\\)`));
assert.match(child.stderr, new RegExp(`VFS \\d+: stat ${escapeRegExp(entryPoint)} -> 0 \\(mount=${escapeRegExp(mountPoint)} overlay=false\\)`));
assert.match(child.stderr, new RegExp(`VFS \\d+: unmount ${escapeRegExp(mountPoint)}`));
assert.match(child.stderr, new RegExp(`VFS \\d+: deregister mount=${escapeRegExp(mountPoint)} active=0`));
