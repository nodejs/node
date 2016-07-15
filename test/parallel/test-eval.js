'use strict';
const common = require('../common');
var util = require('util');
var assert = require('assert');
var exec = require('child_process').exec;

var cmd = ['"' + process.execPath + '"', '-e', '"console.error(process.argv)"',
           'foo', 'bar'].join(' ');
var expected = util.format([process.execPath, 'foo', 'bar']) + '\n';
exec(cmd, common.mustCall(function(err, stdout, stderr) {
  assert.ifError(err);
  assert.equal(stderr, expected);
}));
