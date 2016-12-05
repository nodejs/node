'use strict';

const assert = require('assert');
const common = require('../common');
const spawn = require('child_process').spawn;

const timeoutId = setTimeout(function() {
  common.fail('test-debug-prompt timeout failure');
}, common.platformTimeout(5000));

const proc = spawn(process.execPath, ['debug', 'foo']);
proc.stdout.setEncoding('utf8');

proc.stdout.once('data', common.mustCall((data) => {
  clearTimeout(timeoutId);
  assert.strictEqual(data, 'debug> ');
  proc.kill();
}));
