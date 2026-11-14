'use strict';
const common = require('../common');
common.skipIfSQLiteMissing();
common.skipIfInspectorDisabled();
if (common.isWindows) {
  common.skip('Windows resolves .. before it follows a symlink');
}

// A relative --localstorage-file is only made absolute when options are
// parsed, so a symlink in it is still followed when the file is opened, and
// link/../linked.db is the file next to the link's target, where the OS
// resolves it.
const fs = require('fs');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');
const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

fs.mkdirSync(tmpdir.resolve('target', 'nested'), { recursive: true });
fs.symlinkSync('target/nested', tmpdir.resolve('link'));

spawnSyncAndExitWithoutError(process.execPath, [
  '--localstorage-file=target/linked.db',
  '-e', 'localStorage.setItem("k", "v")',
], { cwd: tmpdir.path });

spawnSyncAndExitWithoutError(process.execPath, [
  '--experimental-storage-inspection',
  '--localstorage-file=link/../linked.db',
  '--input-type=module',
  '-e', `
  import assert from 'node:assert';
  import { Session } from 'node:inspector/promises';

  const session = new Session();
  session.connect();
  await session.post('DOMStorage.enable');
  const { storageKey } = await session.post('Storage.getStorageKey');
  // localStorage is still untouched, so the inspector opens the file itself.
  const { entries } = await session.post('DOMStorage.getDOMStorageItems', {
    storageId: { isLocalStorage: true, securityOrigin: '', storageKey },
  });
  assert.deepStrictEqual(entries, [['k', 'v']]);
  assert.strictEqual(localStorage.getItem('k'), 'v');
  `,
], { cwd: tmpdir.path });
