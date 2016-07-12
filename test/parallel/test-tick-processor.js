'use strict';
var fs = require('fs');
var path = require('path');
var assert = require('assert');
var cp = require('child_process');
var common = require('../common');

//This test is designed test correctness of the integration of the V8
//tick processor with Node.js.
//(https://developers.google.com/v8/profiler_example)
//It is designed to test that the names of profiled functions appear in
//profiler output (as an approximation to profiler correctness)

// TODO(mhdawson) Currently the test-tick-processor functionality in V8
// depends on addresses being smaller than a full 64 bits.  Aix supports
// the full 64 bits and the result is that it does not process the
// addresses correctly and runs out of memory
// Disabling until we get a fix upstreamed into V8
if (common.isAix) {
  common.skip('Aix address range too big for scripts.');
  return;
}

common.refreshTmpDir();
process.chdir(common.tmpDir);
// Unknown checked for to prevent flakiness, if pattern is not found,
// then a large number of unknown ticks should be present
runTest(/LazyCompile: \*profiled_function|.*%  UNKNOWN/,
        `function profiled_function() {
           for (var i = 0; i < 1000000; i++) {
             i++;
           }
           setImmediate(function() { profiled_function(); });
         };
         setTimeout(function() { process.exit(0); }, 2000);
         profiled_function();`);
if (common.isWindows ||
    common.isSunOS ||
    common.isAix ||
    common.isLinuxPPCBE ||
    common.isFreeBSD) {
  common.skip('C++ symbols are not mapped for this os.');
  return;
}
runTest(/RunInDebugContext/,
        `function profiled_function() {
           require(\'vm\').runInDebugContext(\'Debug\');
           setImmediate(function() { profiled_function(); });
         };
         setTimeout(function() { process.exit(0); }, 2000);
         profiled_function();`);

function runTest(pattern, code) {
  var testCodePath = path.join(common.tmpDir, 'test.js');
  fs.writeFileSync(testCodePath, code);
  cp.execSync(process.execPath + ' -prof ' + testCodePath);
  var matches = fs.readdirSync(common.tmpDir).filter(function(file) {
    return /^isolate-/.test(file);
  });
  if (matches.length != 1) {
    assert.fail(null, null, 'There should be a single log file.');
  }
  var log = matches[0];
  var out = cp.execSync(process.execPath +
                        ' --prof-process --call-graph-size=10 ' + log,
                        {encoding: 'utf8'});
  assert(pattern.test(out));
  fs.unlinkSync(log);
}
