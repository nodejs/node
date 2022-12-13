'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('WebCryptoAPI');

runner.setInitScript(`
  global.location = {};
`);

runner.runJsTests();
