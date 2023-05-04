// Flags: --experimental-localworker
'use strict';

const common = require('../common');
const { strictEqual } = require('node:assert');

const {
  LocalWorker,
} = require('node:vm');

// Properly handles timers that are about to expire when FreeEnvironment() is called on
// a shared event loop
(async function() {
  const w = new LocalWorker();

  setImmediate(() => {
    setTimeout(() => {}, 20);
    const now = Date.now();
    while (Date.now() - now < 30);
  });
  await w.stop();
})().then(common.mustCall());

(async function() {
  const w = new LocalWorker();

  setImmediate(() => {
    setImmediate(() => {
      setImmediate(() => {});
    });
  });
  await w.stop();
})().then(common.mustCall());

(async function() {
  const w = new LocalWorker();

  setImmediate(() => {
    setTimeout(() => {}, 20);
    const now = Date.now();
    while (Date.now() - now < 30);
  });
  await w.stop();
})().then(common.mustCall());

(async function() {
  const w = new LocalWorker();
  let called = false;

  function cb() {
    called = true;
  }
  w.runInWorkerScope(() => {
    const req = w.createRequire(__filename);
    const vm = req('vm');
    const fs = req('fs');

    vm.runInThisContext(`({ fs, cb }) => {
      const stream = fs.createReadStream('${__filename}');
      stream.on('open', () => {
        cb()
      })

      setTimeout(() => {}, 200000);
    }`)({ fs, cb });
  });

  await w.stop();
  strictEqual(called, true);
})().then(common.mustCall());

(async function() {
  const w = new LocalWorker();
  let called = false;

  function cb() {
    called = true;
  }
  const _import = await w.createImport(__filename);
  const vm = await _import('vm');
  const fs = await _import('fs');

  w.runInWorkerScope(async () => {
    vm.runInThisContext(`({ fs, cb }) => {
      const stream = fs.createReadStream('${__filename}');
      stream.on('open', () => {
        cb()
      })

      setTimeout(() => {}, 200000);
    }`)({ fs, cb });
  });

  await w.stop();
  strictEqual(called, true);
})().then(common.mustCall());
