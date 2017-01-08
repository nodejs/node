'use strict';
const common = require('../common');
const assert = require('assert');
const exec = require('child_process').exec;

const cmd = '"' + process.execPath + '" ' +
            '"' + common.fixturesDir + '/test-regress-GH-4015.js"';

exec(cmd, function(err, stdout, stderr) {
  assert(/RangeError: Maximum call stack size exceeded/.test(stderr));
});
