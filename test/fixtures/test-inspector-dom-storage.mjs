import * as common from '../common/index.mjs';
import assert from 'assert';
import { DOMStorage, Session } from 'node:inspector/promises';
import { pathToFileURL } from 'node:url';
import path from 'node:path';
import process from 'node:process';

{
  const session = new Session();
  await session.connect();

  await session.post('DOMStorage.enable');

  const localStorageFileUrl =
    pathToFileURL(path.join(process.cwd(), 'localstorage.db')).href;

  const { storageKey } = await session.post('Storage.getStorageKey');
  assert.strictEqual(
    storageKey.toLowerCase(),
    localStorageFileUrl.toLowerCase(),
  );

  for (const isLocalStorage of [true, false]) {
    DOMStorage.registerStorage({
      isLocalStorage,
      storageMap: {
        key1: 'value1',
        key2: 'value2',
        [1]: 2,
        [true]: 'booleanKey',
        ['ключ']: 'значение',
      },
    });
    const result = await session.post('DOMStorage.getDOMStorageItems', {
      storageId: {
        isLocalStorage,
        securityOrigin: 'node-inspector://default-dom-storage',
      },
    });
    const sortedEntries = result.entries.sort((a, b) =>
      a[0].localeCompare(b[0]),
    );
    assert.deepStrictEqual(sortedEntries, [
      ['1', '2'],
      ['key1', 'value1'],
      ['key2', 'value2'],
      ['true', 'booleanKey'],
      ['ключ', 'значение'],
    ]);
  }
  session.disconnect();
}

{
  for (const isLocalStorage of [true, false]) {
    const webStorage = isLocalStorage ? localStorage : sessionStorage;
    const session = new Session();
    webStorage.clear();
    await session.connect();

    const storageKey = await session.post('Storage.getStorageKey');
    await session.post('DOMStorage.enable');

    webStorage.setItem('key1', 'value');
    webStorage.setItem('key2', 1);
    webStorage.setItem('key3', JSON.stringify({ a: 1 }));
    webStorage.setItem('ключ', 'значение');

    const result = await session.post('DOMStorage.getDOMStorageItems', {
      storageId: {
        isLocalStorage,
        securityOrigin: '',
        storageKey: storageKey.storageKey,
      },
    });
    assert.strictEqual(result.entries.length, 4);
    const entries = Object.fromEntries(result.entries);
    assert.strictEqual(entries.key1, 'value');
    assert.strictEqual(entries.key2, '1');
    assert.strictEqual(entries.key3, JSON.stringify({ a: 1 }));
    assert.strictEqual(entries['ключ'], 'значение');
    session.disconnect();
  }
}

{
  for (const isLocalStorage of [true, false]) {
    const webStorage = isLocalStorage ? localStorage : sessionStorage;
    webStorage.clear();
    const session = new Session();
    await session.connect();
    await session.post('DOMStorage.enable');
    const storageKey = await session.post('Storage.getStorageKey');
    session.on(
      'DOMStorage.domStorageItemAdded',
      common.mustCall(({ params }) => {
        assert.strictEqual(params.key, 'key');
        assert.strictEqual(params.newValue, 'value');
        assert.deepStrictEqual(params.storageId, {
          securityOrigin: '',
          isLocalStorage,
          storageKey: storageKey.storageKey,
        });
      }),
    );
    webStorage.setItem('key', 'value');

    session.on(
      'DOMStorage.domStorageItemUpdated',
      common.mustCall(({ params }) => {
        assert.strictEqual(params.key, 'key');
        assert.strictEqual(params.oldValue, 'value');
        assert.strictEqual(params.newValue, 'newValue');
        assert.deepStrictEqual(params.storageId, {
          securityOrigin: '',
          isLocalStorage,
          storageKey: storageKey.storageKey,
        });
      }),
    );

    webStorage.setItem('key', 'newValue');

    session.on(
      'DOMStorage.domStorageItemRemoved',
      common.mustCall(({ params }) => {
        assert.strictEqual(params.key, 'key');
        assert.deepStrictEqual(params.storageId, {
          securityOrigin: '',
          isLocalStorage,
          storageKey: storageKey.storageKey,
        });
      }),
    );

    webStorage.removeItem('key');

    session.on(
      'DOMStorage.domStorageItemsCleared',
      common.mustCall(({ params }) => {
        assert.deepStrictEqual(params.storageId, {
          securityOrigin: '',
          isLocalStorage,
          storageKey: storageKey.storageKey,
        });
      }),
    );
    webStorage.clear();
    session.disconnect();
  }
}
