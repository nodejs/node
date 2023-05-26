'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { request } = require('https');

request(
  'https://nodejs.org/en',
  // Purposely set this to false because we want all connection but the last to fail
  { autoSelectFamily: true, autoSelectFamilyAttemptTimeout: 10 },
  (res) => {
    assert.strictEqual(res.statusCode, 200);
    res.resume();
  },
).end();
