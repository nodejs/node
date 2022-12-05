'use strict';
const common = require('../common');
const assert = require('assert');
const net = require('net');

const autoSelectFamilyAttemptTimeout = common.platformTimeout(-10);

assert.throws(() => {
  net.connect({
    port: 8080,
    autoSelectFamily: true,
    autoSelectFamilyAttemptTimeout,
  });
}, { code: 'ERR_OUT_OF_RANGE' });
