'use strict';

require('../common');
const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('urlpattern');

// Set Node.js flags required for the tests.
runner.setFlags(['--expose-internals']);

// Set a script that will be executed in the worker before running the tests.
runner.setInitScript(`
  const {URLPattern} = require('url');
  global.URLPattern = URLPattern;
`);

runner.runJsTests();
