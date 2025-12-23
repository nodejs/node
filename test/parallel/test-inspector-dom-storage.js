// Flags: --inspect=0 --experimental-inspector-storage-key=node-inspector://default-dom-storage
'use strict';

const common = require('../common');
const assert = require('assert');
common.skipIfInspectorDisabled();
const { DOMStorage, Session } = require('node:inspector/promises');


async function test() {
  const session = new Session();
  await session.connect();

  // Check disabled before enable
  await session
    .post('DOMStorage.getDOMStorageItems', {
      storageId: {
        isLocalStorage: true,
        securityOrigin: 'node-inspector://default-dom-storage',
      },
    })
    .catch(
      common.mustCall((err) => {
        assert.ok(err.message.includes('DOMStorage domain is not enabled'));
      })
    );

  await session.post('DOMStorage.enable');

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
