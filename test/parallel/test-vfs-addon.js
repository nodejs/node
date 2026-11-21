// Flags: --experimental-vfs
'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');
const vfs = require('node:vfs');

const addonPath = path.join(
  __dirname, '..', 'addons', 'hello-world', 'build', 'Release', 'binding.node');
if (!fs.existsSync(addonPath)) {
  common.skip('the hello-world addon is not built');
}

// A native addon inside a mounted VFS can be require()d: dlopen() cannot open
// the reserved mount path, so the loader hands the addon's bytes to
// process.dlopen's `binary` option, which loads them from a private temporary
// image (an in-memory memfd on Linux).
const myVfs = vfs.create();
myVfs.writeFileSync('/binding.node', fs.readFileSync(addonPath));
const mountPoint = myVfs.mount();

// Images land in TMPDIR, which other tests loading addons share. Point it at a
// directory this test owns, so their images cannot pass for a leak here.
tmpdir.refresh();
process.env.TMPDIR = tmpdir.path;

const addon = require(path.join(mountPoint, 'binding.node'));
assert.strictEqual(addon.hello(), 'world');

// On Linux the memfd path never touches the filesystem; on other POSIX the
// temp image is unlinked right after loading, so no addon temp file lingers.
// (Windows keeps a delete-on-close file until exit, so skip the check there.)
if (process.platform !== 'win32') {
  const leaked = fs.readdirSync(tmpdir.path)
    .filter((f) => f.startsWith('node-addon'));
  assert.deepStrictEqual(leaked, [], `addon temp not cleaned up: ${leaked}`);
}

// Regression check: while a VFS is mounted, process.dlopen() of a
// real-file-system addon without a flags argument must keep the default
// flags rather than forwarding `undefined`, which coerces to 0 - not a
// valid dlopen(2) mode.
const realMod = { exports: {} };
process.dlopen(realMod, addonPath);
assert.strictEqual(realMod.exports.hello(), 'world');

myVfs.unmount();
