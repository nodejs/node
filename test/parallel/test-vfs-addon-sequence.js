// Flags: --disable-warning=ExperimentalWarning
'use strict';
const common = require('../common');
const assert = require('node:assert');
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
