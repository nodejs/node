'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const crypto = require('crypto');

common.expectWarning({
  DeprecationWarning: {
    DEP0181: 'crypto.Hmac constructor is deprecated.',
  },
});

const Hmac = crypto.Hmac;
const instance = crypto.Hmac('sha256', 'Node');
assert(instance instanceof Hmac, 'Hmac is expected to return a new instance' +
                                 ' when called without `new`');
