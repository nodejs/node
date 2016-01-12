'use strict';
var common = require('../common');
var assert = require('assert');
var exec = require('child_process').exec;

var cmd = '"' + process.execPath + '" ' +
          '"' + common.fixturesDir + '/test-regress-GH-4015.js"';

exec(cmd, function(err, stdout, stderr) {
  const expectedError = common.engineSpecificMessage({
    v8 : /RangeError: Maximum call stack size exceeded/,
    chakracore : /Error: Out of stack space/
  });
  assert(expectedError.test(stderr));
});
