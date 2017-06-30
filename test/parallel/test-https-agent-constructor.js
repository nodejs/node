'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const https = require('https');

assert.doesNotThrow(() => { https.Agent(); });
assert.ok(https.Agent() instanceof https.Agent);
