// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { internalBinding } = require('internal/test/binding');

// dlopenBinary() loads a native addon from bytes already in memory, for an
// addon that the dynamic loader cannot open by path (e.g. one served by a
// virtual file system). It is internal: process.dlopen() keeps its documented
// (module, filename[, flags]) signature and never takes the bytes.
const { dlopenBinary } = internalBinding('process_methods');
assert.strictEqual(typeof dlopenBinary, 'function');
assert.strictEqual(process.dlopen.length, 0);

const addonPath = path.join(
  __dirname, '..', 'addons', 'hello-world', 'build', 'Release', 'binding.node');
if (!fs.existsSync(addonPath)) common.skip('the hello-world addon is not built');
const bytes = fs.readFileSync(addonPath);
const flags = (os.constants.dlopen?.RTLD_LAZY) || 0;

// A path that does not exist on disk, as a VFS-resident addon would be.
const fakePath = path.join(os.tmpdir(), 'nonexistent-vfs-dir', 'binding.node');
assert.ok(!fs.existsSync(fakePath));

const m = { exports: {} };
dlopenBinary(m, fakePath, flags, bytes);
assert.strictEqual(typeof m.exports.hello, 'function');
assert.strictEqual(m.exports.hello(), 'world');

// The flags argument may be left undefined when passing bytes.
const m2 = { exports: {} };
dlopenBinary(m2, fakePath, undefined, bytes);
assert.strictEqual(m2.exports.hello(), 'world');

// Without the bytes, that nonexistent path cannot be loaded.
assert.throws(() => dlopenBinary({ exports: {} }, fakePath, flags),
              { code: 'ERR_DLOPEN_FAILED' });

// A non-buffer binary is rejected.
assert.throws(
  () => dlopenBinary({ exports: {} }, fakePath, flags, 'nope'),
  { code: 'ERR_INVALID_ARG_TYPE' });

// process.dlopen() ignores a fourth argument rather than loading from it: the
// public signature is unchanged, so this still fails on the missing path.
assert.throws(
  () => process.dlopen({ exports: {} }, fakePath, flags, bytes),
  { code: 'ERR_DLOPEN_FAILED' });
