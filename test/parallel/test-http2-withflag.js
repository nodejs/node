'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

require('http2');
