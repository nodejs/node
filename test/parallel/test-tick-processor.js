'use strict';
var fs = require('fs');
var assert = require('assert');
var path = require('path');
var cp = require('child_process');
var common = require('../common');

common.refreshTmpDir();
process.chdir(common.tmpDir);
var processor =
    path.join(common.testDir, '..', 'tools', 'v8-prof', 'tick-processor.js');
// Unknown checked for to prevent flakiness, if pattern is not found,
// then a large number of unknown ticks should be present
runTest(/LazyCompile.*\[eval\]:1|.*%  UNKNOWN/,
  `function f() {
     for (var i = 0; i < 1000000; i++) {
       i++;
     }
     setImmediate(function() { f(); });
   };
   setTimeout(function() { process.exit(0); }, 2000);
   f();`);
if (common.isWindows ||
    common.isSunOS ||
    common.isAix ||
    common.isLinuxPPCBE ||
    common.isFreeBSD) {
  console.log('1..0 # Skipped: C++ symbols are not mapped for this os.');
  return;
}
runTest(/RunInDebugContext/,
  `function f() {
     require(\'vm\').runInDebugContext(\'Debug\');
     setImmediate(function() { f(); });
   };
   setTimeout(function() { process.exit(0); }, 2000);
   f();`);

function runTest(pattern, code) {
  cp.execFileSync(process.execPath, ['-prof', '-pe', code]);
  var matches = fs.readdirSync(common.tmpDir).filter(function(file) {
    return /^isolate-/.test(file);
  });
  if (matches.length != 1) {
    assert.fail(null, null, 'There should be a single log file.');
  }
  var log = matches[0];
  var out = cp.execSync(process.execPath + ' ' + processor +
                        ' --call-graph-size=10 ' + log,
                        {encoding: 'utf8'});
  assert(out.match(pattern));
  fs.unlinkSync(log);
}
