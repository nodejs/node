'use strict';

const assert = require('assert');
const common = require('./');

// Workaround for Windows Server 2008R2
// When CMD is used to launch a process and CMD is killed too quickly, the
// process can stay behind running in suspended state, never completing.
function cleanupStaleProcess(filename) {
  if (!common.isWindows) {
    return;
  }
  process.once('beforeExit', () => {
    const basename = filename.replace(/.*[/\\]/g, '');
    try {
      require('child_process')
        .execFileSync(`${process.env.SystemRoot}\\System32\\wbem\\WMIC.exe`, [
          'process',
          'where',
          `commandline like '%${basename}%child'`,
          'delete',
          '/nointeractive',
        ]);
    } catch {
      // Ignore failures, there might not be any stale process to clean up.
    }
  });
}

// This should keep the child process running long enough to expire
// the timeout.
const kExpiringChildRunTime = common.platformTimeout(20 * 1000);
const kExpiringParentTimer = 1;
assert(kExpiringChildRunTime > kExpiringParentTimer);

function logAfterTime(time) {
  setTimeout(() => {
    // The following console statements are part of the test.
    console.log('child stdout');
    console.error('child stderr');
  }, time);
}

module.exports = {
  cleanupStaleProcess,
  logAfterTime,
  kExpiringChildRunTime,
  kExpiringParentTimer
};
