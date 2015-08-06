'use strict';
const common = require('../common');
const exec = require('child_process').exec;
const assert = require('assert');

var cmd = 'echo "hello world"';

exec(cmd, { maxBuffer: 5 }, function(err, stdout, stderr) {
  assert.ok(err);
  assert.ok(/maxBuffer/.test(err.message));
});
