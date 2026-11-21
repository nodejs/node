'use strict';
const common = require('../common');
common.skipIfSQLiteMissing();
common.skipIfInspectorDisabled();

// A relative --localstorage-file is resolved when options are parsed, so a
// later process.chdir() changes neither the file localStorage uses nor the one
// the inspector reads and names in its storage key.
const { spawnSyncAndExitWithoutError } = require('../common/child_process');
const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

spawnSyncAndExitWithoutError(process.execPath, [
  '--localstorage-file=storage.db',
  '-e', 'localStorage.setItem("k", "v")',
], { cwd: tmpdir.path });

spawnSyncAndExitWithoutError(process.execPath, [
  '--experimental-storage-inspection',
  '--localstorage-file=storage.db',
  '--input-type=module',
  '-e', `
  import assert from 'node:assert';
  import { mkdirSync } from 'node:fs';
  import { Session } from 'node:inspector/promises';
  import { join } from 'node:path';
  import { pathToFileURL } from 'node:url';

  const file = join(process.cwd(), 'storage.db');
  mkdirSync('elsewhere');
  process.chdir('elsewhere');

  const session = new Session();
  session.connect();
  await session.post('DOMStorage.enable');
  const { storageKey } = await session.post('Storage.getStorageKey');
  assert.strictEqual(
    storageKey.toLowerCase(), pathToFileURL(file).href.toLowerCase());
  // localStorage is still untouched, so the inspector opens the file itself.
  const { entries } = await session.post('DOMStorage.getDOMStorageItems', {
    storageId: { isLocalStorage: true, securityOrigin: '', storageKey },
  });
  assert.deepStrictEqual(entries, [['k', 'v']]);
  assert.strictEqual(localStorage.getItem('k'), 'v');
  `,
], { cwd: tmpdir.path });
