'use strict';
const common = require('../common');
common.skipIfSQLiteMissing();
common.skipIfInspectorDisabled();

// Stores the application never touched, read from a V8 API interrupt, with
// localStorage written by an earlier run.
const assert = require('assert');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');
const { getItemsWhileBusy } = require('../common/inspector-dom-storage');
const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

(async () => {
  const file = tmpdir.resolve('storage.db');
  spawnSyncAndExitWithoutError(process.execPath, [
    `--localstorage-file=${file}`,
    '-e', 'localStorage.setItem("l", "local")',
  ]);
  const [local, session] = await getItemsWhileBusy(file, '', [true, false]);
  assert.deepStrictEqual(local.entries, [['l', 'local']]);
  assert.deepStrictEqual(session.entries, []);
})().then(common.mustCall());
