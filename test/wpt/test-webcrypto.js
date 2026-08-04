'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { WPTRunner } = require('../common/wpt');

// Runs each spec in its own process; this suite has crashed the runner in CI.
const runner = new WPTRunner('WebCryptoAPI', { backend: 'process' });

runner.pretendGlobalThisAs('Window');

runner.runJsTests();
