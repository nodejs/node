'use strict';
var common = require('../common');
var assert = require('assert');

var constants = require('constants');
var fs = require('fs');

var O_APPEND = constants.O_APPEND || 0;
var O_CREAT = constants.O_CREAT || 0;
var O_DIRECTORY = constants.O_DIRECTORY || 0;
var O_EXCL = constants.O_EXCL || 0;
var O_NOCTTY = constants.O_NOCTTY || 0;
var O_NOFOLLOW = constants.O_NOFOLLOW || 0;
var O_RDONLY = constants.O_RDONLY || 0;
var O_RDWR = constants.O_RDWR || 0;
var O_SYMLINK = constants.O_SYMLINK || 0;
var O_SYNC = constants.O_SYNC || 0;
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
