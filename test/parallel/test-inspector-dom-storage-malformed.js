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
const { malformedLocalStorage } = require('../common/inspector-dom-storage');
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
tmpdir.refresh();

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
  // A wrong-typed value is rejected while reading the rows, after the file
  // opens successfully, so there is no open error to report.
  await assert.rejects(
    getDOMStorageItems(
      malformedLocalStorage('bad-value.db', 1, 'hello')),
    { message: 'Could not read DOM storage items: the backing file is malformed' },
  );

  // A wrong-typed schema_version keeps the store from opening, and the reason
  // reaches the frontend.
  await assert.rejects(
    getDOMStorageItems(
      malformedLocalStorage(
        'bad-schema-version.db', 'one', Buffer.from('hello', 'utf16le'))),
    {
      // Formatted like a v8::Message, hence the "Uncaught Error" prefix.
      message: 'Could not read DOM storage items: Uncaught Error: ' +
        'localStorage database is malformed: expected schema_version to be ' +
        'an integer',
    },
  );
})().then(common.mustCall());
