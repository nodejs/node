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

const expectedLines = [
  { threadId: 0, offset: 0, value: 1, timeout: 'inf',
    message: 'started' },
  { threadId: 0, offset: 0, value: 1, timeout: 'inf',
    message: 'did not wait because the values mismatched' },
  { threadId: 0, offset: 0, value: 0, timeout: '10',
    message: 'started' },
  { threadId: 0, offset: 0, value: 0, timeout: '10',
    message: 'timed out' },
  { threadId: 0, offset: 4, value: 0, timeout: 'inf',
    message: 'started' },
  { threadId: 1, offset: 4, value: -1, timeout: 'inf',
    message: 'started' },
  { threadId: 0, offset: 4, value: 0, timeout: 'inf',
    message: 'was woken up by another thread' },
  { threadId: 1, offset: 4, value: -1, timeout: 'inf',
    message: 'was woken up by another thread' }
];

let SABAddress;
const re = /^\[Thread (?<threadId>\d+)\] Atomics\.wait\((?<SAB>(?:0x)?[0-9a-f]+) \+ (?<offset>\d+), (?<value>-?\d+), (?<timeout>inf|infinity|[0-9.]+)\) (?<message>.+)$/;
for (const line of proc.stderr.split('\n').map((line) => line.trim())) {
  if (!line) continue;
  console.log('Matching', { line });
  const actual = line.match(re).groups;
  const expected = expectedLines.shift();

  if (SABAddress === undefined)
    SABAddress = actual.SAB;
  else
    assert.strictEqual(actual.SAB, SABAddress);

  assert.strictEqual(+actual.threadId, expected.threadId);
  assert.strictEqual(+actual.offset, expected.offset);
  assert.strictEqual(+actual.value, expected.value);
  assert.strictEqual(actual.message, expected.message);
  if (expected.timeout === 'inf')
    assert.match(actual.timeout, /inf(inity)?/);
  else
    assert.strictEqual(actual.timeout, expected.timeout);
}
