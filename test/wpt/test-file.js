'use strict';

require('../common');
const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('FileAPI/file');

runner.setInitScript(`
  const { File } = require('buffer');
  globalThis.File = File;
`);

runner.runJsTests();
