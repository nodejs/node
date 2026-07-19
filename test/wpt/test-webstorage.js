'use strict';
const { skipIfSQLiteMissing } = require('../common');
skipIfSQLiteMissing();
const tmpdir = require('../common/tmpdir');
const { WPTRunner } = require('../common/wpt');
const { join } = require('node:path');
const runner = new WPTRunner('webstorage', { concurrency: 1 });

tmpdir.refresh();

runner.setFlags([
  '--localstorage-file', join(tmpdir.path, 'wpt-tests.localstorage'),
]);
runner.setInitScript(`
  globalThis.window = globalThis;
`);
runner.runJsTests();
