'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const spawn = require('child_process').spawn;

const child = spawn(process.execPath, ['debug']);
child.stderr.setEncoding('utf8');

const expectedLines = [
  /^\(node:\d+\) \[DEP0068\] DeprecationWarning:/,
  /^Usage: .*node.* debug script\.js$/,
  /^       .*node.* debug <host>:<port>$/
];

let actualUsageMessage = '';
child.stderr.on('data', function(data) {
  actualUsageMessage += data.toString();
});

child.on('exit', common.mustCall(function(code) {
  const outputLines = actualUsageMessage.split('\n');
  assert.strictEqual(code, 1);
  for (let i = 0; i < expectedLines.length; i++)
    assert(expectedLines[i].test(outputLines[i]));
}));
