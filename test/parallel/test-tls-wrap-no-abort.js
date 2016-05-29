'use strict';

const common = require('../common');
if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

const util = require('util');
const TLSWrap = process.binding('tls_wrap').TLSWrap;

// This will abort if internal pointer is not set to nullptr.
util.inspect(new TLSWrap());
