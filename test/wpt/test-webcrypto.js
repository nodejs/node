'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('WebCryptoAPI');

// Set Node.js flags required for the tests.
runner.setFlags(['--experimental-global-webcrypto']);

runner.runJsTests();
