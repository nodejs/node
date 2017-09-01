'use strict';
const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const crypto = require('crypto');

common.expectsError(
  () => {
    crypto.setEngine(true);
  },
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError
  }
);

common.expectsError(
  () => {
    crypto.setEngine('/path/to/engine', 'notANumber');
  },
  {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError
  }
);
