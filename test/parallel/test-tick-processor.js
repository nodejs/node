'use strict';
var fs = require('fs');
var assert = require('assert');
var path = require('path');
var cp = require('child_process');
var common = require('../common');

common.refreshTmpDir();

process.chdir(common.tmpDir);
cp.execFileSync(process.execPath, ['-prof', '-pe',
    'function foo(n) {' +
    'require(\'vm\').runInDebugContext(\'Debug\');' +
    'return n < 2 ? n : setImmediate(function() { foo(n-1) + foo(n-2);}); };' +
    'setTimeout(function() { process.exit(0); }, 2000);' +
    'foo(40);']);
var matches = fs.readdirSync(common.tmpDir).filter(function(file) {
  return /^isolate-/.test(file);
});
if (matches.length != 1) {
  assert.fail('There should be a single log file.');
}
var log = matches[0];
var processor =
    path.join(common.testDir, '..', 'tools', 'v8-prof', getScriptName());
var out = cp.execSync(processor + ' ' + log, {encoding: 'utf8'});
assert(out.match(/LazyCompile.*foo/));
if (process.platform === 'win32' ||
    process.platform === 'sunos' ||
    process.platform === 'freebsd') {
  console.log('1..0 # Skipped: C++ symbols are not mapped for this os.');
  return;
}
assert(out.match(/RunInDebugContext/));

function getScriptName() {
  switch (process.platform) {
    case 'darwin':
      return 'mac-tick-processor';
    case 'win32':
      return 'windows-tick-processor.bat';
    default:
      return 'linux-tick-processor';
  }
}
