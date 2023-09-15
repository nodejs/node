'use strict';
const tmpdir = require('../common/tmpdir');
const { WPTRunner } = require('../common/wpt');
const runner = new WPTRunner('webstorage', { concurrency: 1 });

tmpdir.refresh();
process.chdir(tmpdir.path);

runner.setFlags(['--experimental-webstorage']);
runner.setInitScript(`
  globalThis.window = globalThis;
`);
runner.runJsTests();
