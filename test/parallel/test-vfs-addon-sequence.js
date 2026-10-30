// Flags: --disable-warning=ExperimentalWarning
'use strict';
const common = require('../common');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const fs = require('node:fs');
const path = require('node:path');
const vfs = require('node:vfs');

const addonPath = (name) => path.join(
  __dirname, '..', 'addons', name, 'build', common.buildType, 'binding.node');
const helloPath = addonPath('hello-world');
const functionPath = addonPath('hello-world-function-export');
if (!fs.existsSync(helloPath) || !fs.existsSync(functionPath)) {
  common.skip('the hello-world addons are not built');
}

const myVfs = vfs.create();
myVfs.writeFileSync('/hello.node', fs.readFileSync(helloPath));
myVfs.writeFileSync('/function.node', fs.readFileSync(functionPath));
const mountPoint = myVfs.mount();

const hello = require(path.join(mountPoint, 'hello.node'));
assert.strictEqual(typeof hello, 'object');
assert.strictEqual(hello.hello(), 'world');

// hello-world-function-export replaces its exports with a function. Resolved
// to the first image, this would be another { hello } object instead.
const fn = require(path.join(mountPoint, 'function.node'));
assert.strictEqual(typeof fn, 'function');
assert.strictEqual(fn(), 'world');
assert.notStrictEqual(fn, hello);

myVfs.unmount();

// Every image takes an fd number no earlier image had, so a low limit runs out
// of numbers long before it runs out of fds. Loading has to carry on regardless.
if (common.isLinux) {
  const limit = 128;
  const child = `
    const assert = require('node:assert');
    const fs = require('node:fs');
    const path = require('node:path');
    const vfs = require('node:vfs');
    const myVfs = vfs.create();
    myVfs.writeFileSync('/function.node', fs.readFileSync(${JSON.stringify(functionPath)}));
    const mountPoint = myVfs.mount();
    for (let i = 0; i < ${limit * 2}; i++) {
      const addonPath = path.join(mountPoint, 'function.node');
      assert.strictEqual(require(addonPath)(), 'world');
      delete require.cache[addonPath];
    }
  `;
  const { status, stderr } = spawnSync('/bin/sh', [
    '-c', `ulimit -n ${limit} && exec "$0" "$@"`,
    process.execPath, '--disable-warning=ExperimentalWarning', '-e', child,
  ], { encoding: 'utf8' });
  assert.strictEqual(status, 0, stderr);
}
