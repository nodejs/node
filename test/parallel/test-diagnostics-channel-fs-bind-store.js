'use strict';

// Checks that stores bound to the fs tracing channels reach the async
// work, the user callback and nested operations.

const common = require('../common');
const assert = require('node:assert');
const { AsyncLocalStorage } = require('node:async_hooks');
const dc = require('node:diagnostics_channel');
const fs = require('node:fs');
const fsp = require('node:fs/promises');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const file = tmpdir.resolve('file.txt');
fs.writeFileSync(file, 'hello');

const startStore = new AsyncLocalStorage();
const asyncStartStore = new AsyncLocalStorage();

for (const name of ['stat', 'readFile', 'truncate']) {
  const channel = dc.tracingChannel(`fs.${name}`);
  channel.start.bindStore(startStore, (context) => ({ name, context }));
  channel.asyncStart.bindStore(asyncStartStore, (context) => ({ name, context }));
}

// The store that is active when a nested `fs.open` starts.
const openStores = [];
dc.tracingChannel('fs.open').subscribe({
  start() { openStores.push(startStore.getStore()); },
});

function testCallback() {
  startStore.run('outer', common.mustCall(() => {
    fs.stat(file, common.mustSucceed(() => {
      assert.strictEqual(startStore.getStore().name, 'stat');
      assert.strictEqual(asyncStartStore.getStore().name, 'stat');
      assert.strictEqual(startStore.getStore().context,
                         asyncStartStore.getStore().context);
      process.nextTick(testCallbackNested);
    }));
    assert.strictEqual(startStore.getStore(), 'outer');
  }));
}

function testCallbackNested() {
  openStores.length = 0;
  startStore.run('outer', common.mustCall(() => {
    fs.truncate(file, 5, common.mustSucceed(() => {
      assert.strictEqual(openStores.length, 1);
      assert.strictEqual(openStores[0].name, 'truncate');
      process.nextTick(testSync);
    }));
  }));
}

function testSync() {
  openStores.length = 0;
  startStore.run('outer', common.mustCall(() => {
    // Without an encoding, readFileSync() opens the file with
    // fs.openSync(), which runs inside the readFile scope.
    fs.readFileSync(file);
    assert.strictEqual(openStores.length, 1);
    assert.strictEqual(openStores[0].name, 'readFile');
    assert.strictEqual(startStore.getStore(), 'outer');
  }));
  testPromise().then(common.mustCall());
}

async function testPromise() {
  let asyncStartContext;
  const onAsyncStart = common.mustCall(() => {
    asyncStartContext = startStore.getStore();
  });
  const stat = dc.tracingChannel('fs.stat');
  stat.asyncStart.subscribe(onAsyncStart);

  await startStore.run('outer', common.mustCall(async () => {
    await fsp.stat(file);
    // The caller's continuation keeps its own store.
    assert.strictEqual(startStore.getStore(), 'outer');
  }));
  stat.asyncStart.unsubscribe(onAsyncStart);
  assert.strictEqual(asyncStartContext.name, 'stat');

  openStores.length = 0;
  await startStore.run('outer', common.mustCall(async () => {
    await fsp.truncate(file, 2);
    assert.strictEqual(startStore.getStore(), 'outer');
  }));
  assert.strictEqual(openStores.length, 1);
  assert.strictEqual(openStores[0].name, 'truncate');
}

testCallback();
