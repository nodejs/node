// Flags: --js-float16array
// TODO(LiviaMedeiros): once `Float16Array` is unflagged in v8, remove the line above
'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('WebCryptoAPI');

runner.pretendGlobalThisAs('Window');

runner.runJsTests();
