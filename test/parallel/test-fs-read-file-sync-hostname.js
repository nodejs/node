'use strict';
var common = require('../common');
var assert = require('assert');
var fs = require('fs');

// test reading from hostname
if (process.platform === 'linux2') {
  var hostname = fs.readFileSync('/proc/sys/kernel/hostname');
  assert.ok(hostname.length > 0);
}
