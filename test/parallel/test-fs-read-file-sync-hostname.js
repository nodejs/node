'use strict';
const common = require('../common');
const assert = require('assert');
const fs = require('fs');

// test reading from hostname
if (process.platform === 'linux2') {
  var hostname = fs.readFileSync('/proc/sys/kernel/hostname');
  assert.ok(hostname.length > 0);
}
