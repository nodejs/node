'use strict';

const common = require('../common');
const { addresses } = require('../common/internet');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { request } = require('https');

request(
  `https://${addresses.INET_HOST}/en`,
  // Purposely set this to a low value because we want all connection but the last to fail
  { autoSelectFamily: true, autoSelectFamilyAttemptTimeout: 10 },
  (res) => {
    assert.strictEqual(res.statusCode, 200);
    res.resume();
  },
).end();
