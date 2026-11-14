'use strict';

const assert = require('assert');
const { DatabaseSync } = require('node:sqlite');
const common = require('./');
const { NodeInstance } = require('./inspector-helper.js');
const tmpdir = require('./tmpdir');

// While JavaScript runs, a message from a remote frontend is dispatched from a
// V8 API interrupt, where JavaScript must not run. Reading a store has to work
// from there, whether or not the application has touched it yet. The script
// keeps the main thread in a loop while the reads arrive, counts calls to the
// storage getters and to a setter that a JavaScript error created by Node.js
// would hit, and reports the count once the reads are done. If the loop ends
// first, e.g. on a slow machine, the interval keeps the process alive and the
// reads are dispatched from the event loop instead, where the count still
// applies.
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
  globalThis.interval = setInterval(() => {}, 1000);
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
    params: {
      expression:
        'globalThis.done = true; clearInterval(globalThis.interval); ' +
        'globalThis.jsCalls',
    },
  });
  assert.strictEqual(result.value, 0);
  await session.waitForDisconnect();
  assert.strictEqual((await child.expectShutdown()).exitCode, 0);
  return results;
}

// Node's own tables are STRICT, but they are created with IF NOT EXISTS, so a
// file that already contains tables of those names is adopted as-is. Declare
// the same schema without STRICT: BLOB columns have no affinity, so a TEXT
// value stays TEXT.
function malformedLocalStorage(name, schemaVersion, value) {
  const file = tmpdir.resolve(name);
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
  `);
  db.prepare('INSERT INTO nodejs_webstorage (key, value) VALUES (?, ?)')
    .run(Buffer.from('greeting', 'utf16le'), value);
  db.prepare('INSERT INTO nodejs_webstorage_state (total_size, schema_version)' +
    ' VALUES (0, ?)').run(schemaVersion);
  db.close();
  return file;
}

module.exports = {
  getItemsWhileBusy,
  malformedLocalStorage,
};
