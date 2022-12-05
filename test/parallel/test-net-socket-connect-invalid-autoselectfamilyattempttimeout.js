'use strict';
require('../common');
const assert = require('assert');
const net = require('net');

for (const autoSelectFamilyAttemptTimeout of [-10, 0]) {
  assert.throws(() => {
    net.connect({
      port: 8080,
      autoSelectFamily: true,
      autoSelectFamilyAttemptTimeout,
    });
  }, { code: 'ERR_OUT_OF_RANGE' });
}
