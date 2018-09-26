'use strict';
const { skip } = require('../common');

const { isCPPSymbolsNotMapped } = require('../common/tick-processor');

if (isCPPSymbolsNotMapped) {
  skip('C++ symbols are not mapped for this OS.');
}

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

// This test will produce a broken profile log.
// ensure prof-polyfill not stuck in infinite loop
// and success process

const assert = require('assert');
const { spawn, spawnSync } = require('child_process');
const path = require('path');
const { writeFileSync } = require('fs');

const LOG_FILE = path.join(tmpdir.path, 'tick-processor.log');
const RETRY_TIMEOUT = 250;
const BROKEN_PART = 'tick,';
const WARN_REG_EXP = /\(node:\d+\) \[BROKEN_PROFILE_FILE] Warning: Profile file .* is broken/;
const WARN_DETAIL_REG_EXP = /".*tick," at the file end is broken/;

const code = `
  let add = 0;
  function f() {
    for (var i = 0; i < 1e3; i++) {
      add += Date.now();
    }
    return add;
  };
  f();`;

const proc = spawn(process.execPath, [
  '--no_logfile_per_isolate',
  '--logfile=-',
  '--prof',
  '-pe', code
], {
  stdio: ['ignore', 'pipe', 'inherit']
});

let ticks = '';
proc.stdout.on('data', (chunk) => ticks += chunk);

function runPolyfill(content) {
  proc.kill();
  content += BROKEN_PART;
  writeFileSync(LOG_FILE, content);
  const child = spawnSync(
    `${process.execPath}`,
    [
      '--prof-process', LOG_FILE
    ]);
  assert(WARN_REG_EXP.test(child.stderr.toString()));
  assert(WARN_DETAIL_REG_EXP.test(child.stderr.toString()));
  assert.strictEqual(child.status, 0);
}

setTimeout(() => runPolyfill(ticks), RETRY_TIMEOUT);
