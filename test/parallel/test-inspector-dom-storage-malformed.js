// Reading a malformed localStorage file through the DOMStorage domain should
// report a protocol error rather than abort the process. A message from a
// remote frontend is dispatched without a HandleScope on the stack, so this
// drives the protocol over the WebSocket endpoint rather than through an
// in-process inspector Session.
'use strict';

const common = require('../common');
common.skipIfSQLiteMissing();
common.skipIfInspectorDisabled();
const { NodeInstance } = require('../common/inspector-helper.js');
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const { join } = require('node:path');
const { DatabaseSync } = require('node:sqlite');
tmpdir.refresh();

// Node's own tables are STRICT, but they are created with IF NOT EXISTS, so a
// file that already contains tables of those names is adopted as-is. Declare
// the same schema without STRICT: BLOB columns have no affinity, so a TEXT
// value stays TEXT.
function malformedLocalStorage(name, schemaVersion, value) {
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
  `);
  db.prepare('INSERT INTO nodejs_webstorage (key, value) VALUES (?, ?)')
    .run(Buffer.from('greeting', 'utf16le'), value);
  db.prepare('INSERT INTO nodejs_webstorage_state (total_size, schema_version)' +
    ' VALUES (0, ?)').run(schemaVersion);
  db.close();
  return file;
}

async function getDOMStorageItems(localStorageFile) {
  const instance = new NodeInstance([
    '--inspect=0',
    '--experimental-storage-inspection',
    `--localstorage-file=${localStorageFile}`,
  ], 'setInterval(() => {}, 1000);');

  const session = await instance.connectInspectorSession();
  await session.send({ method: 'DOMStorage.enable' });
  const { storageKey } = await session.send({
    method: 'Storage.getStorageKey',
  });

  try {
    return await session.send({
      method: 'DOMStorage.getDOMStorageItems',
      params: {
        storageId: { isLocalStorage: true, securityOrigin: '', storageKey },
      },
    });
  } finally {
    await session.disconnect();
    await instance.kill();
  }
}

(async () => {
  // A wrong-typed value is rejected by Storage::GetAll() itself, which has no
  // exception to report, so the reason is not available.
  await assert.rejects(
    getDOMStorageItems(
      malformedLocalStorage('bad-value.db', 1, 'hello')),
    { message: 'Could not read DOM storage items' },
  );

  // A wrong-typed schema_version makes Storage::Open() throw, which has to be
  // caught rather than left pending on an isolate with no JavaScript running.
  // Its message reaches the frontend.
  await assert.rejects(
    getDOMStorageItems(
      malformedLocalStorage(
        'bad-schema-version.db', 'one', Buffer.from('hello', 'utf16le'))),
    {
      // The reason comes off the v8::Message, hence the "Uncaught" prefix;
      // converting the exception itself would run user JavaScript.
      message: 'Could not read DOM storage items: Uncaught Error: ' +
        'localStorage database is malformed: expected schema_version to be ' +
        'an integer',
    },
  );
})().then(common.mustCall());
