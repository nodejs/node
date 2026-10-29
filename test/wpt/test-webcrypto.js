'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('WebCryptoAPI');

// Experimental warnings drown out the actual test output.
runner.setFlags(['--disable-warning=ExperimentalWarning']);

runner.pretendGlobalThisAs('Window');

runner.runJsTests();
