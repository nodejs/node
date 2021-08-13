'use strict';

require('../common');
const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('html/webappapis/structured-clone');

runner.setInitScript(`
  const { Blob } = require('buffer');
  global.Blob = Blob;
`);

runner.runJsTests();
