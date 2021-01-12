'use strict';

require('../common');
const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('fetch/api/headers');

runner.setFlags(['--expose-internals']);

runner.setInitScript(`
  const { Headers } = require('internal/fetch/headers');
  global.Headers = Headers;
`);

runner.runJsTests();
