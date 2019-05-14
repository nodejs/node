'use strict';
require('../common');
const fs = require('fs');
const cp = require('child_process');
const path = require('path');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const LOG_FILE = path.join(tmpdir.path, 'tick-processor.log');
const RETRY_TIMEOUT = 150;

function runTest(test) {
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

  // Try to match after timeout
  setTimeout(() => {
    match(test.pattern, proc, () => ticks, test.profProcessFlags);
  }, RETRY_TIMEOUT);
}

function match(pattern, parent, ticks, flags = []) {
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
      if (!pattern.test(out))
        return setTimeout(() => match(pattern, parent, ticks), RETRY_TIMEOUT);

      parent.stdout.removeAllListeners();
      parent.kill();
    });

    proc.stdout.removeAllListeners();
    proc.kill();
  });
}

exports.runTest = runTest;
