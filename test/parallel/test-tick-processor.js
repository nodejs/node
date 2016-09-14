'use strict';
const common = require('../common');
const fs = require('fs');
const assert = require('assert');
const cp = require('child_process');
const path = require('path');

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

const tests = [];

const LOG_FILE = path.join(common.tmpDir, 'tick-processor.log');
const RETRY_TIMEOUT = 750;

// Unknown checked for to prevent flakiness, if pattern is not found,
// then a large number of unknown ticks should be present
tests.push({
  pattern: /LazyCompile.*\[eval\]:1|.*%  UNKNOWN/,
  code: `function f() {
           for (var i = 0; i < 1000000; i++) {
             i++;
           }
           setImmediate(function() { f(); });
         };
         f();`
});

if (common.isWindows ||
    common.isSunOS ||
    common.isAix ||
    common.isLinuxPPCBE ||
    common.isFreeBSD) {
  common.skip('C++ symbols are not mapped for this os.');
} else {
}
tests.push({
  pattern: /RunInDebugContext/,
  code: `function f() {
           require(\'vm\').runInDebugContext(\'Debug\');
           setImmediate(function() { f(); });
         };
         f();`
});

tests.push({
  pattern: /Builtin_DateNow/,
  code: `function f() {
           this.ts = Date.now();
           setImmediate(function() { new f(); });
         };
         f();`
});

runTest();

function runTest() {
  if (tests.length === 0)
    return;

  const test = tests.shift();

  const proc = cp.spawn(process.execPath, [
    '--no_logfile_per_isolate',
    `--logfile=${LOG_FILE}`,
    '--prof',
    '-pe', test.code
  ], {
    stdio: [ null, null, 'inherit' ]
  });

  // Try to match after timeout
  setTimeout(() => {
    match(test.pattern, proc, runTest);
  }, RETRY_TIMEOUT);
}

function match(pattern, parent, cb) {
  const proc = cp.spawn(process.execPath, [
    '--prof-process',
    '--call-graph-size=10',
    LOG_FILE
  ], {
    stdio: [ null, 'pipe', 'inherit' ]
  });

  let out = '';
  proc.stdout.on('data', (chunk) => {
    out += chunk;
  });
  proc.stdout.on('end', () => {
    // Retry after timeout
    if (!pattern.test(out))
      return setTimeout(() => match(pattern, parent, cb), RETRY_TIMEOUT);

    parent.kill('SIGTERM');

    fs.unlinkSync(LOG_FILE);
    cb(null);
  });
}
