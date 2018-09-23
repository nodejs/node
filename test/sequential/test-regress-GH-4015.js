'use strict';
var common = require('../common');
var assert = require('assert');
var exec = require('child_process').exec;

var cmd = '"' + process.execPath + '" ' +
          '"' + common.fixturesDir + '/test-regress-GH-4015.js"';

exec(cmd, function(err, stdout, stderr) {
  assert(/RangeError: Maximum call stack size exceeded/.test(stderr));
});
