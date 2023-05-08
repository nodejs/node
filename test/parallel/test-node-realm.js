// Flags: --experimental-node-realm
'use strict';

const common = require('../common');
const assert = require('node:assert');
const { strictEqual } = assert;
const { pathToFileURL } = require('node:url');

const {
  NodeRealm,
} = require('node:vm');

// Properly handles timers that are about to expire when FreeEnvironment() is called on
// a shared event loop
(async function() {
  const w = new NodeRealm();

  setImmediate(() => {
    setTimeout(() => {}, 20);
    const now = Date.now();
    while (Date.now() - now < 30);
  });
  await w.stop();
})().then(common.mustCall());

(async function() {
  const w = new NodeRealm();

  setImmediate(() => {
    setImmediate(() => {
      setImmediate(() => {});
    });
  });
  await w.stop();
})().then(common.mustCall());

(async function() {
  const w = new NodeRealm();

  assert.throws(
    () => {
      w.createImport(undefined);
    },
    {
      name: 'Error',
      message: 'createImport() must be called with a string or URL; received "undefined"',
      code: 'ERR_VM_NODE_REALM_INVALID_PARENT',
    },
  );

  await w.stop();
})().then(common.mustCall());

(async function() {
  const w = new NodeRealm();
  const cb = common.mustCall();
  const _import = w.createImport(__filename);
  const vm = await _import('vm');
  const fs = await _import('fs');

  vm.runInThisContext(`({ fs, cb }) => {
    const stream = fs.createReadStream('${__filename}');
    stream.on('open', () => {
      cb()
    })

    setTimeout(() => {}, 200000);
  }`)({ fs, cb });

  await w.stop();
})().then(common.mustCall());

(async function() {
  const w = new NodeRealm();
  const cb = common.mustCall();
  const _import = await w.createImport(__filename);
  const vm = await _import('vm');
  const fs = await _import('fs');

  vm.runInThisContext(`({ fs, cb }) => {
    const stream = fs.createReadStream('${__filename}');
    stream.on('open', () => {
      cb()
    })

    setTimeout(() => {}, 200000);
  }`)({ fs, cb });

  await w.stop();
})().then(common.mustCall());

// Globals are isolated
(async function() {
  const w = new NodeRealm();
  const _import = w.createImport(__filename);
  const vm = await _import('vm');

  strictEqual(globalThis.foo, undefined);
  vm.runInThisContext('globalThis.foo = 42');
  strictEqual(globalThis.foo, undefined);
  strictEqual(w.globalThis.foo, 42);

  await w.stop();
})().then(common.mustCall());

(async function() {
  const w = new NodeRealm();
  const cb = common.mustCall();
  const _import = await w.createImport(pathToFileURL(__filename));
  const vm = await _import('vm');
  const fs = await _import('fs');

  vm.runInThisContext(`({ fs, cb }) => {
    const stream = fs.createReadStream('${__filename}');
    stream.on('open', () => {
      cb()
    })

    setTimeout(() => {}, 200000);
  }`)({ fs, cb });

  await w.stop();
})().then(common.mustCall());

(async function() {
  const w = new NodeRealm();
  const cb = common.mustCall();
  const _import = await w.createImport(pathToFileURL(__filename).toString());
  const vm = await _import('vm');
  const fs = await _import('fs');

  vm.runInThisContext(`({ fs, cb }) => {
    const stream = fs.createReadStream('${__filename}');
    stream.on('open', () => {
      cb()
    })

    setTimeout(() => {}, 200000);
  }`)({ fs, cb });

  await w.stop();
})().then(common.mustCall());
