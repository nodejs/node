'use strict';
const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;

if (process.argv[2] === 'child') {
  process.stdout.destroy();
  process.stderr.destroy();
  console.log('stdout');
  process.stdout.write('rocks\n');
  console.error('stderr');
  setTimeout(function() {
    process.stderr.write('rocks too\n');
  }, 10);
  return;
}

const proc = spawn(process.execPath, [__filename, 'child'], { stdio: 'pipe' });

let stdout = '';
proc.stdout.setEncoding('utf8');
proc.stdout.on('data', common.mustCallAtLeast(function(chunk) {
  stdout += chunk;
}, 1));

let stderr = '';
proc.stderr.setEncoding('utf8');
proc.stderr.on('data', common.mustCallAtLeast(function(chunk) {
  stderr += chunk;
}, 1));

proc.on('exit', common.mustCall(function(exitCode) {
  assert.strictEqual(exitCode, 0);
  assert.strictEqual(stdout, 'stdout\nrocks\n');
  assert.strictEqual(stderr, 'stderr\nrocks too\n');
}));
