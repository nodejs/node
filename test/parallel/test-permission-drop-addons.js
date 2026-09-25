// Flags: --permission --allow-addons --allow-fs-read=*
'use strict';

const common = require('../common');
const { isMainThread } = require('worker_threads');

if (!isMainThread) {
  common.skip('This test only works on a main thread');
}

const assert = require('assert');

let bindingPath;
try {
  bindingPath = require.resolve(
    `../addons/hello-world/build/${common.buildType}/binding`);
} catch (err) {
  if (err.code !== 'MODULE_NOT_FOUND') {
    throw err;
  }
  common.skip('addon not found');
}

function openAddon() {
  process.dlopen({ exports: {} }, bindingPath);
}

assert.ok(process.permission.has('addon'));
openAddon();

process.permission.drop('addon');
assert.ok(!process.permission.has('addon'));
assert.throws(() => {
  openAddon();
}, common.expectsError({
  code: 'ERR_ACCESS_DENIED',
  permission: 'Addon',
}));
