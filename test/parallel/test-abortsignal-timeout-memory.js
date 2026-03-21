// Flags: --expose-gc
'use strict';

require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');

function runMemoryChurn(script) {
  const child = spawnSync(process.execPath, [
    '--expose-gc',
    '--max-old-space-size=16',
    '-e',
    script,
  ], {
    encoding: 'utf8',
    timeout: 60_000,
  });

  assert.strictEqual(child.status, 0, child.stderr || child.stdout);
  assert.match(child.stdout, /ok/);
}

const timeoutChurnScript = `
let i = 0;
function run() {
  AbortSignal.timeout(1);
  if (++i === 100_000) {
    global.gc();
    global.gc();
    setImmediate(() => {
      global.gc();
      console.log('ok');
    });
    return;
  }
  setImmediate(run);
}
run();
`;

const anyTimeoutChurnScript = `
let i = 0;
function run() {
  AbortSignal.any([AbortSignal.timeout(1)]);
  if (++i === 100_000) {
    global.gc();
    global.gc();
    setImmediate(() => {
      global.gc();
      console.log('ok');
    });
    return;
  }
  setImmediate(run);
}
run();
`;

runMemoryChurn(timeoutChurnScript);
runMemoryChurn(anyTimeoutChurnScript);
