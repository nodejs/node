'use strict';
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const fixtures = require('../common/fixtures');
const assert = require('node:assert');
const { spawnSync } = require('node:child_process');
const { readFileSync, writeFileSync } = require('node:fs');
const { test } = require('node:test');

test('NODE_TEST_WORKER_ID is set for concurrent test files', async () => {
  const args = [
    '--test',
    fixtures.path('test-runner', 'worker-id', 'test-1.mjs'),
    fixtures.path('test-runner', 'worker-id', 'test-2.mjs'),
    fixtures.path('test-runner', 'worker-id', 'test-3.mjs'),
  ];
  const result = spawnSync(process.execPath, args, {
    cwd: fixtures.path(),
    env: { ...process.env }
  });

  assert.strictEqual(result.status, 0, `Test failed: ${result.stderr.toString()}`);
});

test('NODE_TEST_WORKER_ID is set with explicit concurrency', async () => {
  const args = [
    '--test',
    '--test-concurrency=2',
    fixtures.path('test-runner', 'worker-id', 'test-1.mjs'),
    fixtures.path('test-runner', 'worker-id', 'test-2.mjs'),
  ];
  const result = spawnSync(process.execPath, args, {
    cwd: fixtures.path(),
    env: { ...process.env }
  });

  assert.strictEqual(result.status, 0, `Test failed: ${result.stderr.toString()}`);
});

test('NODE_TEST_WORKER_ID is 1 with concurrency=1', async () => {
  const args = ['--test', '--test-concurrency=1', fixtures.path('test-runner', 'worker-id', 'test-1.mjs')];
  const result = spawnSync(process.execPath, args, {
    cwd: fixtures.path(),
    env: { ...process.env }
  });

  assert.strictEqual(result.status, 0, `Test failed: ${result.stderr.toString()}`);
});

test('NODE_TEST_WORKER_ID with explicit isolation=process', async () => {
  const args = [
    '--test',
    '--test-isolation=process',
    fixtures.path('test-runner', 'worker-id', 'test-1.mjs'),
    fixtures.path('test-runner', 'worker-id', 'test-2.mjs'),
  ];
  const result = spawnSync(process.execPath, args, {
    cwd: fixtures.path(),
    env: { ...process.env }
  });

  assert.strictEqual(result.status, 0, `Test failed: ${result.stderr.toString()}`);
});

test('NODE_TEST_WORKER_ID is 1 with isolation=none', async () => {
  const args = [
    '--test',
    '--test-isolation=none',
    fixtures.path('test-runner', 'worker-id', 'test-1.mjs'),
    fixtures.path('test-runner', 'worker-id', 'test-2.mjs'),
  ];
  const result = spawnSync(process.execPath, args, {
    cwd: fixtures.path(),
    env: { ...process.env }
  });

  assert.strictEqual(result.status, 0, `Test failed: ${result.stderr.toString()}`);
});

test('context.workerId matches NODE_TEST_WORKER_ID', async () => {
  const args = ['--test', fixtures.path('test-runner', 'worker-id', 'test-1.mjs')];
  const result = spawnSync(process.execPath, args, {
    cwd: fixtures.path(),
    env: { ...process.env }
  });

  // The fixture tests already verify that context.workerId matches the env var
  assert.strictEqual(result.status, 0, `Test failed: ${result.stderr.toString()}`);
});

test('worker IDs are reused when more tests than concurrency', async () => {
  tmpdir.refresh();

  // Create 9 separate test files dynamically
  const testFiles = [];
  const usageFile = tmpdir.resolve('worker-usage.txt');
  for (let i = 1; i <= 9; i++) {
    const testFile = tmpdir.resolve(`reuse-test-${i}.mjs`);
    writeFileSync(
      testFile,
      `import { test } from 'node:test';
import { appendFileSync } from 'node:fs';

test('track worker ${i}', () => {
  const workerId = process.env.NODE_TEST_WORKER_ID;
  const usageFile = process.env.WORKER_USAGE_FILE;
  appendFileSync(usageFile, workerId + '\\n');
});
`,
    );
    testFiles.push(testFile);
  }

  const args = ['--test', '--test-concurrency=3', ...testFiles];
  const result = spawnSync(process.execPath, args, {
    env: { ...process.env, WORKER_USAGE_FILE: usageFile }
  });

  assert.strictEqual(result.status, 0, `Test failed: ${result.stderr.toString()}`);

  // Read and analyze worker IDs used
  const workerIds = readFileSync(usageFile, 'utf8').trim().split('\n');

  // Count occurrences of each worker ID
  const workerCounts = {};
  workerIds.forEach((id) => {
    workerCounts[id] = (workerCounts[id] || 0) + 1;
  });

  const uniqueWorkers = Object.keys(workerCounts);
  assert.strictEqual(workerIds.length, 9,
                     `Expected 9 worker IDs, got ${workerIds.length}: ${workerIds.join(', ')}`);
  assert.ok(uniqueWorkers.length <= 3,
            `Should use at most 3 worker IDs, got ${uniqueWorkers.length}: ${uniqueWorkers.join(', ')}`);

  uniqueWorkers.forEach((id) => {
    assert.ok(Number(id) >= 1 && Number(id) <= 3, `Worker ID outside 1..3: ${uniqueWorkers.join(', ')}`);
  });
});

// Generates a test file that appends `<kind> <name> <workerId>` lines to a
// shared log, which is replayed below to find IDs held by two live files at
// once.
function testFile(name, { block = false, release = false } = {}) {
  let body = '';

  if (release) {
    body += "writeFileSync(process.env.MARKER_FILE, '');\n";
  }
  if (block) {
    const deadline = common.platformTimeout(10_000);
    body += `const buf = new Int32Array(new SharedArrayBuffer(4));
  const deadline = Date.now() + ${deadline};
  while (!existsSync(process.env.MARKER_FILE) && Date.now() < deadline) {
    Atomics.wait(buf, 0, 0, 20);
  }\n`;
  }

  return `
import { test } from 'node:test';
import { appendFileSync, writeFileSync, existsSync } from 'node:fs';

const log = (kind) => appendFileSync(process.env.WORKER_LOG,
  kind + ' ${name} ' + process.env.NODE_TEST_WORKER_ID + '\\n');

test('${name}', () => {
  log('start');
  ${body}
  log('end');
});
`;
}

// Replays the log and reports every ID that was held by two files at once.
function findConflicts(events) {
  const live = new Map();
  const conflicts = [];

  for (const event of events) {
    const [kind, name, id] = event.split(' ');
    if (kind === 'start') {
      if (live.has(id)) {
        conflicts.push(`worker ID ${id} held by both '${live.get(id)}' and '${name}'`);
      }
      live.set(id, name);
    } else {
      live.delete(id);
    }
  }

  return conflicts;
}

test('worker IDs are exclusive to concurrently running test files', () => {
  tmpdir.refresh();

  // `slow` pins one ID for the whole run while the remaining files churn
  // through the other slots, so IDs are released and reacquired several times
  // with one of them permanently taken.
  const sources = [['slow', { block: true }]];
  for (let i = 1; i <= 4; i++) {
    sources.push([`file-${i}`]);
  }
  sources.push(['last', { release: true }]);

  const concurrency = 3;
  const logFile = tmpdir.resolve('worker-id-log.txt');
  const markerFile = tmpdir.resolve('worker-id-release.marker');
  writeFileSync(logFile, '');

  const files = sources.map(([name, options], index) => {
    const file = tmpdir.resolve(`worker-id-${index}-${name}.mjs`);
    writeFileSync(file, testFile(name, options));
    return file;
  });

  const result = spawnSync(
    process.execPath,
    ['--test', `--test-concurrency=${concurrency}`, ...files],
    { env: { ...process.env, WORKER_LOG: logFile, MARKER_FILE: markerFile } },
  );
  assert.strictEqual(result.status, 0, `Runner failed: ${result.stderr.toString()}`);

  const events = readFileSync(logFile, 'utf8').trim().split('\n');
  assert.strictEqual(events.length, sources.length * 2,
                     `Unexpected event log:\n${events.join('\n')}`);

  // The blocking file must still be running when the last file starts
  const eventsAt = (line) => events.findIndex((event) => event.startsWith(line));
  assert.ok(eventsAt('start last') < eventsAt('end slow'),
            `Files did not overlap:\n${events.join('\n')}`);

  const conflicts = findConflicts(events);
  assert.deepStrictEqual(
    conflicts, [], `Worker IDs were not exclusive:\n${events.join('\n')}\n\n${conflicts.join('\n')}`);

  for (const event of events) {
    const id = Number(event.split(' ')[2]);
    assert.ok(Number.isInteger(id) && id >= 1 && id <= concurrency,
              `Worker ID outside 1..${concurrency}:\n${events.join('\n')}`);
  }
});
