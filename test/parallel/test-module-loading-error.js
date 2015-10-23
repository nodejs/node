'use strict';
var common = require('../common');
var assert = require('assert');

console.error('load test-module-loading-error.js');

var error_desc = {
  win32: '%1 is not a valid Win32 application',
  linux: 'file too short',
  sunos: 'unknown file type'
};

var dlerror_msg = error_desc[process.platform];

if (!dlerror_msg) {
  console.log('1..0 # Skipped: platform not supported.');
  return;
}

try {
  require('../fixtures/module-loading-error.node');
} catch (e) {
  assert.notEqual(e.toString().indexOf(dlerror_msg), -1);
}

try {
  require();
} catch (e) {
  assert.notEqual(e.toString().indexOf('missing path'), -1);
}

try {
  require({});
} catch (e) {
  assert.notEqual(e.toString().indexOf('path must be a string'), -1);
}
