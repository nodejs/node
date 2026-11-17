'use strict';

// The WPT runner can run each spec on a worker thread or in a child process.
// Suites opt into either, and share the status files, so both backends have to
// report the same things in the same way.

const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');
const { spawnSyncAndExitWithoutError } = require('../common/child_process');
const { backends, WPTRunner } = require('../common/wpt');

const queueProbe = process.env.NODE_TEST_WPT_QUEUE_PROBE === '1';
const backendProbe = process.env.NODE_TEST_WPT_BACKEND_PROBE;

const harnessPath = fixtures.path('wpt', 'resources', 'testharness.js');
const specPath = fixtures.path('wpt-backends-spec.js');
const execArgv = [];

function payload(throws) {
  return {
    testRelativePath: 'backends.any.js',
    wptRunner: path.join(__dirname, '../common/wpt.js'),
    wptPath: 'compression',
    initScript: throws ? 'globalThis.WPT_BACKENDS_THROW = true;' : null,
    harness: {
      code: fs.readFileSync(harnessPath, 'utf8'),
      filename: harnessPath,
    },
    scriptsToRun: [{
      code: fs.readFileSync(specPath, 'utf8'),
      filename: specPath,
    }],
    needsGc: false,
    // A regular expression only survives the trip to a child process because
    // the runner forks with advanced serialization.
    skippedTests: ['skipped by name', /^skipped by pattern$/],
  };
}

// Collect what a backend reports for one spec, driving it the way the runner
// does so that the real spawn options are exercised.
async function collect(backend, throws) {
  const events = [];
  let done = false;

  const handle = backends[backend](execArgv, payload(throws), {
    message: (message) => {
      switch (message.type) {
        case 'result':
          events.push(`result ${message.result.status} ${message.result.name}`);
          break;
        case 'skip':
          events.push(`skip ${message.name}`);
          break;
        case 'completion':
          events.push(`completion ${message.status.status}`);
          done = true;
          handle.kill();
          break;
        default:
          assert.fail(`Unexpected message type: ${message.type}`);
      }
    },
    failure: (failure) => {
      if (done) {
        return false;
      }
      done = true;
      events.push(`uncaught ${failure.name} | ${failure.message}`);
      return true;
    },
  });

  const watchdog = setTimeout(
    common.mustNotCall(`The ${backend} WPT backend did not finish`),
    common.platformTimeout(10_000),
  );
  try {
    await handle.finished;
  } finally {
    clearTimeout(watchdog);
  }
  return events;
}

async function compare(throws) {
  const comparable = async (backend) => {
    const events = await collect(backend, throws);
    // A worker thread keeps delivering messages after it throws, so `events`
    // is copied here and only the error is compared; what the spec reports
    // before throwing is covered by the run that completes normally.
    const wanted = throws ? (event) => event.startsWith('uncaught ') : () => true;
    return events.filter(wanted).sort();
  };

  const thread = await comparable('thread');
  const child = await comparable('process');
  assert.notStrictEqual(thread.length, 0);
  assert.deepStrictEqual(thread, child);
  return thread;
}

// End to end: the runner's own reporting has to match too. The webidl spec
// reports an uncaught error whose name the status file matches on.
function runDriver(driver, spec, backend) {
  const { child: { stdout } } = spawnSyncAndExitWithoutError(
    process.execPath,
    [path.join(__dirname, '../wpt', driver), spec],
    { env: { ...process.env, WPT_BACKEND: backend }, encoding: 'utf8' },
  );
  // Specs run concurrently, so the lines arrive in an arbitrary order.
  return stdout.split('\n').filter((line) => line.startsWith('[')).sort();
}

function runQueueProbe() {
  backends.process = common.mustCall((execArgv, workerData, handlers) => {
    queueMicrotask(() => handlers.message({
      type: 'completion',
      status: { status: 0 },
    }));
    return {
      kill() {},
      // Model the gap between a spec completing and its process closing. The
      // close notification alone must not be responsible for keeping the
      // parent alive long enough to start the next queued spec.
      finished: new Promise((resolve) => {
        setTimeout(common.mustCall(resolve), 1).unref();
      }),
    };
  }, 2);

  process.argv[2] = 'compression-bad-chunks.any.js';
  const runner = new WPTRunner('compression', {
    backend: 'process',
    concurrency: 1,
  });
  // WPT drivers intentionally do not await this method.
  runner.runJsTests();
}

function checkQueuedSpecsKeepRunnerAlive() {
  const result = spawnSync(process.execPath, [__filename], {
    env: {
      ...process.env,
      NODE_TEST_WPT_QUEUE_PROBE: '1',
      WPT_BACKEND: 'process',
    },
    encoding: 'utf8',
    timeout: common.platformTimeout(10_000),
  });
  const { error, status, stdout, stderr } = result;
  assert.ifError(error);
  assert.strictEqual(status, 0, `Queued WPT probe failed:\n${stdout}${stderr}`);
}

function runDefaultBackendProbe() {
  const runner = new WPTRunner('compression');
  assert.strictEqual(runner.runSpec, backends[backendProbe]);
}

function checkBackendSelection() {
  const env = { ...process.env };
  delete env.WPT_BACKEND;
  delete env.WPT_INSPECT;

  for (const [name, expected, args, overrides] of [
    ['default', 'thread', [__filename]],
    ['environment override', 'process', [__filename], { WPT_BACKEND: 'process' }],
    ['inspect override', 'process', [__filename], {
      WPT_BACKEND: 'thread',
      WPT_INSPECT: '1',
    }],
  ]) {
    const result = spawnSync(process.execPath, args, {
      env: {
        ...env,
        ...overrides,
        NODE_TEST_WPT_BACKEND_PROBE: expected,
      },
      encoding: 'utf8',
      timeout: common.platformTimeout(10_000),
    });
    const { error, status, stdout, stderr } = result;
    assert.ifError(error);
    assert.strictEqual(
      status,
      0,
      `${name} WPT backend probe failed:\n${stdout}${stderr}`,
    );
  }
}

function checkInspectSelection() {
  const driver = path.join(__dirname, '../wpt/test-compression.js');
  const env = { ...process.env, WPT_INSPECT: '1' };
  delete env.WPT_BACKEND;

  const runFailure = common.mustCall((args, expected) => {
    const result = spawnSync(process.execPath, [driver, ...args], {
      env,
      encoding: 'utf8',
      timeout: common.platformTimeout(10_000),
    });
    const { error, status, stdout, stderr } = result;
    assert.ifError(error);
    assert.strictEqual(status, 1, `WPT inspect probe passed:\n${stdout}${stderr}`);
    assert.match(stderr, expected);
  }, 3);

  runFailure([], /WPT_INSPECT requires a WPT test path/);
  runFailure(
    ['compression-bad-chunks.any.js'],
    /WPT_INSPECT requires exactly one generated WPT test path; .* matched 2:\r?\ncompression\/compression-bad-chunks\.any\.html\r?\ncompression\/compression-bad-chunks\.any\.worker\.html/,
  );
  runFailure(
    ['compression/compression-bad-chunks.any.worker.html'],
    /WPT_INSPECT does not support worker tests: compression\/compression-bad-chunks\.any\.worker\.html/,
  );
}

async function main() {
  checkBackendSelection();
  checkInspectSelection();
  checkQueuedSpecsKeepRunnerAlive();

  const completed = await compare(false);
  assert.deepStrictEqual(completed, [
    'completion 0',
    'result 0 passes',
    'result 0 passes asynchronously',
    'result 1 fails',
    'skip skipped by name',
    'skip skipped by pattern',
  ]);

  const uncaught = await compare(true);
  assert.deepStrictEqual(uncaught, [
    'uncaught Error: deliberate uncaught error | deliberate uncaught error',
  ]);

  for (const [driver, spec] of [
    ['test-compression.js', 'compression-bad-chunks.any.js'],
    ['test-webidl.js', 'ecmascript-binding/global-object-implicit-this-value.any.js'],
  ]) {
    assert.deepStrictEqual(
      runDriver(driver, spec, 'thread'),
      runDriver(driver, spec, 'process'),
    );
  }

  const windowResults = runDriver(
    'test-events.js',
    'dom/events/Event-constructors.any.html',
    'thread',
  );
  const workerResults = runDriver(
    'test-events.js',
    'dom/events/Event-constructors.any.worker.html',
    'thread',
  ).map((line) => line.replace('.any.worker.html', '.any.html'));
  assert.deepStrictEqual(workerResults, windowResults);
}

if (backendProbe) {
  runDefaultBackendProbe();
} else if (queueProbe) {
  runQueueProbe();
} else {
  main().then(common.mustCall());
}
