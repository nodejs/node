'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { WPTRunner } = require('../common/wpt');

// Runs each spec in its own process; this suite has crashed the runner in CI.
const runner = new WPTRunner('WebCryptoAPI', { backend: 'process' });

// Experimental warnings drown out the actual test output.
runner.setFlags(['--disable-warning=ExperimentalWarning']);

runner.pretendGlobalThisAs('Window');

runner.runJsTests();
