'use strict';
require('../common');
const assert = require('assert');
const child_process = require('child_process');
const { Worker } = require('worker_threads');

if (process.argv[2] === 'child') {
  const i32arr = new Int32Array(new SharedArrayBuffer(8));
  assert.strictEqual(Atomics.wait(i32arr, 0, 1), 'not-equal');
  assert.strictEqual(Atomics.wait(i32arr, 0, 0, 10), 'timed-out');

  new Worker(`
  const i32arr = require('worker_threads').workerData;
  Atomics.store(i32arr, 1, -1);
  Atomics.notify(i32arr, 1);
  Atomics.wait(i32arr, 1, -1);
  `, { eval: true, workerData: i32arr });

  Atomics.wait(i32arr, 1, 0);
  assert.strictEqual(Atomics.load(i32arr, 1), -1);
  Atomics.store(i32arr, 1, 0);
  Atomics.notify(i32arr, 1);
  return;
}

const proc = child_process.spawnSync(
  process.execPath,
  [ '--trace-atomics-wait', __filename, 'child' ],
  { encoding: 'utf8', stdio: [ 'inherit', 'inherit', 'pipe' ] });

if (proc.status !== 0) console.log(proc);
assert.strictEqual(proc.status, 0);

const SABAddress = proc.stderr.match(/Atomics\.wait\((?<SAB>.+) \+/).groups.SAB;
const actualTimeline = proc.stderr
  .replace(new RegExp(SABAddress, 'g'), '<address>')
  .replace(new RegExp(`\\(node:${proc.pid}\\) `, 'g'), '')
  .replace(/\binf(inity)?\b/gi, 'inf')
  .replace(/\r/g, '')
  .trim();
console.log('+++ normalized stdout +++');
console.log(actualTimeline);
console.log('--- normalized stdout ---');

const begin =
`[Thread 0] Atomics.wait(<address> + 0, 1, inf) started
[Thread 0] Atomics.wait(<address> + 0, 1, inf) did not wait because the \
values mismatched
[Thread 0] Atomics.wait(<address> + 0, 0, 10) started
[Thread 0] Atomics.wait(<address> + 0, 0, 10) timed out`;

const expectedTimelines = [
  `${begin}
[Thread 0] Atomics.wait(<address> + 4, 0, inf) started
[Thread 1] Atomics.wait(<address> + 4, -1, inf) started
[Thread 0] Atomics.wait(<address> + 4, 0, inf) was woken up by another thread
[Thread 1] Atomics.wait(<address> + 4, -1, inf) was woken up by another thread`,
  `${begin}
[Thread 1] Atomics.wait(<address> + 4, 0, inf) started
[Thread 0] Atomics.wait(<address> + 4, -1, inf) started
[Thread 0] Atomics.wait(<address> + 4, 0, inf) was woken up by another thread
[Thread 1] Atomics.wait(<address> + 4, -1, inf) was woken up by another thread`,
  `${begin}
[Thread 0] Atomics.wait(<address> + 4, 0, inf) started
[Thread 0] Atomics.wait(<address> + 4, 0, inf) was woken up by another thread
[Thread 1] Atomics.wait(<address> + 4, -1, inf) started
[Thread 1] Atomics.wait(<address> + 4, -1, inf) was woken up by another thread`,
  `${begin}
[Thread 0] Atomics.wait(<address> + 4, 0, inf) started
[Thread 0] Atomics.wait(<address> + 4, 0, inf) was woken up by another thread
[Thread 1] Atomics.wait(<address> + 4, -1, inf) started
[Thread 1] Atomics.wait(<address> + 4, -1, inf) did not wait because the \
values mismatched`,
  `${begin}
[Thread 0] Atomics.wait(<address> + 4, 0, inf) started
[Thread 1] Atomics.wait(<address> + 4, -1, inf) started
[Thread 0] Atomics.wait(<address> + 4, 0, inf) was woken up by another thread
[Thread 1] Atomics.wait(<address> + 4, -1, inf) did not wait because the \
values mismatched`,
  `${begin}
[Thread 1] Atomics.wait(<address> + 4, 0, inf) started
[Thread 0] Atomics.wait(<address> + 4, -1, inf) started
[Thread 0] Atomics.wait(<address> + 4, 0, inf) was woken up by another thread
[Thread 1] Atomics.wait(<address> + 4, -1, inf) did not wait because the \
values mismatched`,
  `${begin}
[Thread 0] Atomics.wait(<address> + 4, 0, inf) started
[Thread 0] Atomics.wait(<address> + 4, 0, inf) did not wait because the \
values mismatched
[Thread 1] Atomics.wait(<address> + 4, -1, inf) started
[Thread 1] Atomics.wait(<address> + 4, -1, inf) did not wait because the \
values mismatched`,
  `${begin}
[Thread 1] Atomics.wait(<address> + 4, -1, inf) started
[Thread 0] Atomics.wait(<address> + 4, 0, inf) started
[Thread 0] Atomics.wait(<address> + 4, 0, inf) did not wait because the \
values mismatched
[Thread 1] Atomics.wait(<address> + 4, -1, inf) was woken up by another thread`,
];

assert(expectedTimelines.includes(actualTimeline));
