'use strict';
const common = require('../common');
const fs = require('fs');
const assert = require('assert');
const cp = require('child_process');

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
  common.skip('C++ symbols are not mapped for this os.');
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
    common.fail('There should be a single log file.');
  }
  var log = matches[0];
  var out = cp.execSync(process.execPath +
                        ' --prof-process --call-graph-size=10 ' + log,
                        {encoding: 'utf8'});
  assert(pattern.test(out), `${pattern} not matching ${out}`);
  fs.unlinkSync(log);
}
