// Flags: --inspect=0 --experimental-storage-inspection --localstorage-file=./localstorage.db
'use strict';

const common = require('../common');
const assert = require('assert');
common.skipIfInspectorDisabled();
const { DOMStorage, Session } = require('node:inspector/promises');
const { pathToFileURL } = require('node:url');
const path = require('node:path');


async function test() {
  const session = new Session();
  await session.connect();

  await session.post('DOMStorage.enable');

  const localStorageFileUrl =
    pathToFileURL(path.join(process.cwd(), 'localstorage.db')).href;

  const { storageKey } = await session.post('Storage.getStorageKey');
  assert.strictEqual(
    storageKey.toLowerCase(),
    localStorageFileUrl.toLowerCase()
  );

  await checkStorage(true);
  await checkStorage(false);

  async function checkStorage(isLocalStorage) {
    DOMStorage.registerStorage({
      isLocalStorage,
      storageMap: {
        key1: 'value1',
        key2: 'value2',
        [1]: 2,
        [true]: 'booleanKey',
      },
    });
    const result = await session.post('DOMStorage.getDOMStorageItems', {
      storageId: {
        isLocalStorage,
        securityOrigin: 'node-inspector://default-dom-storage',
      },
    });
    const sortedEntries = result.entries.sort((a, b) => a[0].localeCompare(b[0]));
    assert.deepStrictEqual(sortedEntries, [
      ['1', '2'],
      ['key1', 'value1'],
      ['key2', 'value2'],
      ['true', 'booleanKey'],
    ]);
  }
}

test().then(common.mustCall());
