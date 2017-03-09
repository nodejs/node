'use strict';
const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}
const tls = require('tls');

// neither should hang
tls.createSecurePair(null, false, false, false);
tls.createSecurePair(null, true, false, false);
