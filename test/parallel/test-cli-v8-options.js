'use strict';
const common = require('../common');
const assert = require('assert');
const execFile = require('child_process').execFile;

// test --v8-options and --v8_options
(function runTest(flags) {
  var flag = flags.pop();
  execFile(process.execPath, [flag], function(err, stdout, stderr) {
    assert.equal(err, null);
    assert.equal(stderr, '');
    assert.equal(stdout.indexOf('Usage') > -1, true);
    if (flags.length > 0)
      setImmediate(runTest, flags);
  });
})(['--v8-options', '--v8_options']);
