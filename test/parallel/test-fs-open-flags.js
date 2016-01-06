'use strict';
require('../common');
var assert = require('assert');

var constants = require('constants');
var fs = require('fs');

var O_APPEND = constants.O_APPEND || 0;
var O_CREAT = constants.O_CREAT || 0;
var O_EXCL = constants.O_EXCL || 0;
var O_RDONLY = constants.O_RDONLY || 0;
var O_RDWR = constants.O_RDWR || 0;
var O_TRUNC = constants.O_TRUNC || 0;
var O_WRONLY = constants.O_WRONLY || 0;

assert.equal(fs._stringToFlags('r'), O_RDONLY);
assert.equal(fs._stringToFlags('r+'), O_RDWR);
assert.equal(fs._stringToFlags('w'), O_TRUNC | O_CREAT | O_WRONLY);
assert.equal(fs._stringToFlags('w+'), O_TRUNC | O_CREAT | O_RDWR);
assert.equal(fs._stringToFlags('a'), O_APPEND | O_CREAT | O_WRONLY);
assert.equal(fs._stringToFlags('a+'), O_APPEND | O_CREAT | O_RDWR);

assert.equal(fs._stringToFlags('wx'), O_TRUNC | O_CREAT | O_WRONLY | O_EXCL);
assert.equal(fs._stringToFlags('xw'), O_TRUNC | O_CREAT | O_WRONLY | O_EXCL);
assert.equal(fs._stringToFlags('wx+'), O_TRUNC | O_CREAT | O_RDWR | O_EXCL);
assert.equal(fs._stringToFlags('xw+'), O_TRUNC | O_CREAT | O_RDWR | O_EXCL);
assert.equal(fs._stringToFlags('ax'), O_APPEND | O_CREAT | O_WRONLY | O_EXCL);
assert.equal(fs._stringToFlags('xa'), O_APPEND | O_CREAT | O_WRONLY | O_EXCL);
assert.equal(fs._stringToFlags('ax+'), O_APPEND | O_CREAT | O_RDWR | O_EXCL);
assert.equal(fs._stringToFlags('xa+'), O_APPEND | O_CREAT | O_RDWR | O_EXCL);

('+ +a +r +w rw wa war raw r++ a++ w++' +
 'x +x x+ rx rx+ wxx wax xwx xxx').split(' ').forEach(function(flags) {
  assert.throws(function() { fs._stringToFlags(flags); });
});

// Use of numeric flags is permitted.
assert.equal(fs._stringToFlags(O_RDONLY), O_RDONLY);

// Non-numeric/string flags are a type error.
assert.throws(function() { fs._stringToFlags(undefined); }, TypeError);
assert.throws(function() { fs._stringToFlags(null); }, TypeError);
assert.throws(function() { fs._stringToFlags(assert); }, TypeError);
assert.throws(function() { fs._stringToFlags([]); }, TypeError);
assert.throws(function() { fs._stringToFlags({}); }, TypeError);

// Numeric flags that are not int will be passed to the binding, which
// will throw a TypeError
assert.throws(function() { fs.openSync('_', O_RDONLY + 0.1); }, TypeError);
