'use strict';

require('../common');
const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('FileAPI/file');

runner.setInitScript(`
  const { File, Blob } = require('buffer');
  // Global Blob is needed for some tests:
  globalThis.Blob = Blob;
  globalThis.File = File;
`);

runner.runJsTests();
