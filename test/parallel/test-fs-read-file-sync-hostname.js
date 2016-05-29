'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');

if (!common.isLinux) {
  common.skip('Test is linux specific.');
  return;
}

// Test to make sure reading a file under the /proc directory works. See:
// https://groups.google.com/forum/#!topic/nodejs-dev/rxZ_RoH1Gn0
const hostname = fs.readFileSync('/proc/sys/kernel/hostname');
assert.ok(hostname.length > 0);
