//todo

'use strict';
const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');

if (!common.hasCrypto)
  common.skip('missing crypto');

const serverOptions = {
  key: fixtures.readKey('agent1-key.pem'),
  cert: fixtures.readKey('agent1-cert.pem')
};
