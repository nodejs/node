var common = require('../common');
var assert = require('assert');
var spawn = require('child_process').spawn;
var N = 4 << 20;  // 4 MB

for (var big = '*'; big.length < N; big += big);

if (process.argv[2] === 'child') {
  process.stdout.write(big);
  process.exit(42);
}

var stdio = ['inherit', 'pipe', 'inherit'];
var proc = spawn(process.execPath, [__filename, 'child'], { stdio: stdio });

var chunks = [];
proc.stdout.setEncoding('utf8');
proc.stdout.on('data', chunks.push.bind(chunks));

proc.on('exit', common.mustCall(function(exitCode) {
  assert.equal(exitCode, 42);
  assert.equal(chunks.join(''), big);
}));
