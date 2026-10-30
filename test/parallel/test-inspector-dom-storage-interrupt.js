'use strict';
const common = require('../common');
common.skipIfSQLiteMissing();
common.skipIfInspectorDisabled();

const assert = require('assert');
const { spawnSync } = require('child_process');
const { join } = require('path');
const { DatabaseSync } = require('node:sqlite');
const { NodeInstance } = require('../common/inspector-helper.js');
const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

// While JavaScript runs, a message from a remote frontend is dispatched from a
// V8 API interrupt, where JavaScript must not run. Reading a store has to work
// from there, whether or not the application has touched it yet. The script
// counts calls to the storage getters and to a setter that a JavaScript error
// created by Node.js would hit, and reports the count once the reads are done.
async function getItemsWhileBusy(localStorageFile, prelude, storageIds) {
  const script = `
  globalThis.jsCalls = 0;
  for (const name of ['localStorage', 'sessionStorage']) {
    const desc = Object.getOwnPropertyDescriptor(globalThis, name);
    Object.defineProperty(globalThis, name, {
      ...desc,
      get() { globalThis.jsCalls++; return desc.get.call(this); },
    });
  }
  Object.defineProperty(Error.prototype, 'code', {
    set() { globalThis.jsCalls++; }, configurable: true,
  });
  ${prelude}
  globalThis.jsCalls = 0;
  console.log('Ready');
  const deadline = Date.now() + ${common.platformTimeout(10000)};
  while (!globalThis.done && Date.now() < deadline);
  `;
  const child = new NodeInstance([
    '--inspect-brk=0',
    '--experimental-storage-inspection',
    `--localstorage-file=${localStorageFile}`,
  ], script);
  const session = await child.connectInspectorSession();
  await session.send({ method: 'NodeRuntime.enable' });
  await session.waitForNotification('NodeRuntime.waitingForDebugger');
  await session.send([
    { 'method': 'Runtime.enable' },
    { 'method': 'Debugger.enable' },
  ]);
  await session.send({ method: 'Runtime.runIfWaitingForDebugger' });
  await session.send({ method: 'NodeRuntime.disable' });
  await session.waitForNotification('Debugger.paused', 'Break on start');
  await session.send({ 'method': 'Debugger.resume' });
  await session.waitForConsoleOutput('log', ['Ready']);
  // Enabled late, so items set by the prelude have to be read from the stores.
  await session.send({ method: 'DOMStorage.enable' });

  const { storageKey } = await session.send({ method: 'Storage.getStorageKey' });
  const results = [];
  for (const isLocalStorage of storageIds) {
    results.push(await session.send({
      method: 'DOMStorage.getDOMStorageItems',
      params: { storageId: { isLocalStorage, securityOrigin: '', storageKey } },
    }).catch((err) => err));
  }

  const { result } = await session.send({
    method: 'Runtime.evaluate',
    params: { expression: 'globalThis.done = true; globalThis.jsCalls' },
  });
  assert.strictEqual(result.value, 0);
  await session.waitForDisconnect();
  assert.strictEqual((await child.expectShutdown()).exitCode, 0);
  return results;
}

// Node's own tables are created with IF NOT EXISTS, so a file that already
// holds tables of those names is adopted as-is, wrong types included.
function malformedLocalStorage(name) {
  const file = join(tmpdir.path, name);
  const db = new DatabaseSync(file);
  db.exec(`
    CREATE TABLE nodejs_webstorage(
      key BLOB NOT NULL, value BLOB NOT NULL, PRIMARY KEY(key)
    );
    CREATE TABLE nodejs_webstorage_state(
      max_size INTEGER NOT NULL DEFAULT 10485760,
      total_size INTEGER NOT NULL,
      schema_version INTEGER NOT NULL DEFAULT 1,
      single_row_ INTEGER NOT NULL DEFAULT 1 CHECK(single_row_ = 1),
      PRIMARY KEY(single_row_)
    );
    INSERT INTO nodejs_webstorage_state (total_size, schema_version)
      VALUES (0, 'one');
  `);
  db.close();
  return file;
}

async function runTest() {
  // Stores the application never touched, with localStorage written by an
  // earlier run.
  const file = join(tmpdir.path, 'storage.db');
  const { status } = spawnSync(process.execPath, [
    `--localstorage-file=${file}`,
    '-e', 'localStorage.setItem("l", "local")',
  ]);
  assert.strictEqual(status, 0);
  const [local, session] = await getItemsWhileBusy(file, '', [true, false]);
  assert.deepStrictEqual(local.entries, [['l', 'local']]);
  assert.deepStrictEqual(session.entries, []);

  // Both stores in memory and never touched.
  const untouched = await getItemsWhileBusy(':memory:', '', [true, false]);
  assert.deepStrictEqual(untouched.map((r) => r.entries), [[], []]);

  // Both stores in memory, sessionStorage created first.
  const [memLocal, memSession] = await getItemsWhileBusy(':memory:', `
    sessionStorage.setItem('s', 'session');
    localStorage.setItem('l', 'local');
  `, [true, false]);
  assert.deepStrictEqual(memLocal.entries, [['l', 'local']]);
  assert.deepStrictEqual(memSession.entries, [['s', 'session']]);

  // A file that cannot be opened is reported without creating a JavaScript
  // error.
  for (const [name, touch] of [['untouched.db', ''],
                               ['touched.db', 'globalThis.localStorage;']]) {
    const [err] = await getItemsWhileBusy(
      malformedLocalStorage(name), touch, [true]);
    assert.strictEqual(
      err.message,
      'Could not read DOM storage items: Uncaught Error: localStorage ' +
      'database is malformed: expected schema_version to be an integer');
  }
}

runTest().then(common.mustCall());
