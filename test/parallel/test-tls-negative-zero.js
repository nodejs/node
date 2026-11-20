'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const tls = require('tls');

tls.createSecureContext({ sessionTimeout: -0 });
