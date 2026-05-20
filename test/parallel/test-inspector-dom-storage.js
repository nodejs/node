'use strict';

const common = require('../common');
common.skipIfSQLiteMissing();
common.skipIfInspectorDisabled();
const { spawnSyncAndExitWithoutError } = require('../common/child_process');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

spawnSyncAndExitWithoutError(process.execPath, [
  '--inspect=0',
  '--experimental-storage-inspection',
  '--localstorage-file=./localstorage.db',
  fixtures.path('test-inspector-dom-storage.mjs'),
], { cwd: tmpdir.path });
