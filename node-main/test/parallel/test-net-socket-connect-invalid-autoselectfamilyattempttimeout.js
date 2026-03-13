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

  assert.throws(() => {
    net.setDefaultAutoSelectFamilyAttemptTimeout(autoSelectFamilyAttemptTimeout);
  }, { code: 'ERR_OUT_OF_RANGE' });
}

// Check the default value of autoSelectFamilyAttemptTimeout is 10
// if passed number is less than 10
for (const autoSelectFamilyAttemptTimeout of [1, 9]) {
  net.setDefaultAutoSelectFamilyAttemptTimeout(autoSelectFamilyAttemptTimeout);
  assert.strictEqual(net.getDefaultAutoSelectFamilyAttemptTimeout(), 10);
}
