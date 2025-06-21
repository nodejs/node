'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

// Run test-tls-ticket.js with TLS1.2

const tls = require('tls');

tls.DEFAULT_MAX_VERSION = 'TLSv1.2';

require('./test-tls-ticket.js');
