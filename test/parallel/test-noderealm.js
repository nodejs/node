// Flags: --experimental-noderealm
'use strict';

const common = require('../common');
const { strictEqual } = require('node:assert');

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
  let called = false;

  function cb() {
    called = true;
  }

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
  strictEqual(called, true);
})().then(common.mustCall());

(async function() {
  const w = new NodeRealm();
  let called = false;

  function cb() {
    called = true;
  }
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
  strictEqual(called, true);
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
