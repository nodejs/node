'use strict';

const {
  isWindows,
  isSunOS,
  isAIX,
  isLinuxPPCBE,
  isFreeBSD,
  skip
} = require('../common');

const fs = require('fs');
const cp = require('child_process');
const path = require('path');

const tmpdir = require('./tmpdir');
tmpdir.refresh();

const LOG_FILE = path.join(tmpdir.path, 'tick-processor.log');
const START_PROF_PROCESS_TIMEOUT = 400;

let running = 0;
let ran = 0;

// The tick processor is not supported on all platforms.
function isCPPSymbolsNotMapped() {
  if (isWindows || isSunOS || isAIX || isLinuxPPCBE || isFreeBSD) {
    skip('C++ symbols are not mapped for this OS.');
    return true;
  }
  return false;
}

function runTest(test) {
  if (isCPPSymbolsNotMapped()) {
    return;
  }

  ran++;
  running = 0;
  const proc = cp.spawn(process.execPath, [
    '--no_logfile_per_isolate',
    '--logfile=-',
    '--prof',
    '-pe', test.code
  ], {
    stdio: [ 'ignore', 'pipe', 'inherit' ]
  });

  let ticks = '';
  proc.stdout.on('data', (chunk) => ticks += chunk);

  // Check if the pattern is already there or not.
  setTimeout(() => {
    if (running === 0) {
      match(test, proc, () => ticks);
    }
  }, START_PROF_PROCESS_TIMEOUT);

  proc.on('exit', () => {
    running++;
    if (running === 1) {
      match(test, proc, () => ticks);
    }
  });
}

function match(test, parent, ticks) {
  running++;

  const { pattern, profProcessFlags: flags = [] } = test;
  // Store current ticks log
  fs.writeFileSync(LOG_FILE, ticks());

  const proc = cp.spawn(process.execPath, [
    '--prof-process',
    '--call-graph-size=10',
    ...flags,
    LOG_FILE
  ], {
    stdio: [ 'ignore', 'pipe', 'inherit' ]
  });

  let out = '';

  proc.stdout.on('data', (chunk) => out += chunk);
  proc.stdout.once('end', () => {
    proc.once('exit', () => {
      fs.unlinkSync(LOG_FILE);

      // Retry after timeout
      if (!pattern.test(out)) {
        running--;
        // If the profiling is done without match, try up to ten times again
        // and then fail.
        if (running === 1) {
          if (ran < 10) {
            return runTest(test);
          }
          console.error(out);
          throw new Error(`${pattern} Failed`);
        }
        return;
      }

      parent.stdout.removeAllListeners();
      parent.kill();
    });

    proc.stdout.removeAllListeners();
    proc.kill();
  });
}

module.exports = {
  runTest,
  isCPPSymbolsNotMapped
};
