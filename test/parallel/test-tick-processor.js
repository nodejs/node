'use strict';

const fs = require('fs');
const assert = require('assert');
const cp = require('child_process');
const common = require('../common');

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
   f();`, () => {

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
});

function runTest(pattern, code, callback) {
  cp.execFile(process.execPath, ['-prof', '-pe', code], (err, stdout, stderr) => {
    assert.ifError(err)
    const matches = fs.readdirSync(common.tmpDir).filter((file) => /^isolate-/.test(file));

    if (matches.length != 1)
      return assert.fail(null, null, 'There should be a single log file.');

    const log = matches[0];

    cp.execFile(process.execPath, ['--prof-process', '--call-graph-size=10', log],
                          {encoding: 'utf8'}, (err, stdout, stderr) => {
      assert.ifError(err)
      assert(pattern.test(stdout.toString()));
      fs.unlinkSync(log);

      callback && callback()
    });
  });
}
