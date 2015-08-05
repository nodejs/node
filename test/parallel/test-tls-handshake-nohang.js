'use strict';
const common = require('../common');
const assert = require('assert');

if (!common.hasCrypto) {
  console.log('1..0 # Skipped: missing crypto');
  return;
}
const tls = require('tls');

// neither should hang
tls.createSecurePair(null, false, false, false);
tls.createSecurePair(null, true, false, false);
