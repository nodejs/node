import '../common/index.mjs';
import assert from 'assert';
import { Session } from 'node:inspector/promises';

// getDOMStorageItems only looks at isLocalStorage, so the storage key is not
// needed to address a store. Storage.getStorageKey is deliberately not used
// here: without --localstorage-file it has no path to resolve, and what it
// does then differs between platforms.
const storageKey = '';

// Without --localstorage-file, globalThis.localStorage is undefined, so the
// agent cannot read the store. Reading its items must report an error instead
// of answering successfully with an empty list, which is indistinguishable
// from a store that exists and happens to be empty.
{
  const session = new Session();
  await session.connect();
  await session.post('DOMStorage.enable');

  await assert.rejects(
    session.post('DOMStorage.getDOMStorageItems', {
      storageId: {
        isLocalStorage: true,
        securityOrigin: '',
        storageKey,
      },
    }),
    {
      code: 'ERR_INSPECTOR_COMMAND',
      message: /Could not read DOM storage items/,
    },
  );

  session.disconnect();
}

// sessionStorage is always backed by an in-memory store, so it stays readable
// and answers with an empty list until items are added.
{
  const session = new Session();
  await session.connect();
  await session.post('DOMStorage.enable');

  const storageId = { isLocalStorage: false, securityOrigin: '', storageKey };

  const empty = await session.post('DOMStorage.getDOMStorageItems', {
    storageId,
  });
  assert.deepStrictEqual(empty.entries, []);

  sessionStorage.setItem('key', 'value');
  const result = await session.post('DOMStorage.getDOMStorageItems', {
    storageId,
  });
  assert.deepStrictEqual(result.entries, [['key', 'value']]);

  session.disconnect();
}
