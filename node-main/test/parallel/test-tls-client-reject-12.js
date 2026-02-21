'use strict';

// test-tls-client-reject specifically for TLS1.2.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const tls = require('tls');

tls.DEFAULT_MAX_VERSION = 'TLSv1.2';

require('./test-tls-client-reject.js');
