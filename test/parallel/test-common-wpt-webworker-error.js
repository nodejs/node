'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { fork } = require('child_process');
const { Worker } = require('worker_threads');

const workerPath = path.join(__dirname, '../common/wpt/worker.js');
const harnessPath = fixtures.path('wpt', 'resources', 'testharness.js');
const execArgv = [];
const workerData = {
  testRelativePath: 'workers/error-after-result.any.js',
  wptRunner: path.join(__dirname, '../common/wpt.js'),
  wptPath: 'workers',
  initScript: null,
  harness: {
    code: fs.readFileSync(harnessPath, 'utf8'),
    filename: harnessPath,
  },
  scriptsToRun: [],
  webWorker: {
    path: fixtures.path('web-worker', 'wpt-error-after-result.js'),
    isAnyTest: true,
    initScript: null,
    variant: '',
    scripts: [],
    skippedTests: [],
  },
  needsGc: false,
  skippedTests: [],
};

// Both backends have to report an error thrown by a Web Worker after a result
// was already reported, and report it the same way.
check('thread', () => {
  const worker = new Worker(workerPath, { execArgv, workerData });
  return { runner: worker, stop: () => worker.terminate() };
});
check('process', () => {
  const child = fork(workerPath, { execArgv, serialization: 'advanced' });
  child.send(workerData);
  return { runner: child, stop: () => child.kill() };
});

function check(backend, start) {
  const { runner, stop } = start();

  const timeout = setTimeout(() => {
    stop();
    assert.fail(`WPT worker error was not reported on the ${backend} backend`);
  }, common.platformTimeout(10_000));

  const onCompletion = common.mustCall((status) => {
    clearTimeout(timeout);
    assert.strictEqual(status.status, 1);
    assert.strictEqual(status.message, 'probe error after first result');
    stop();
  });

  runner.on('error', common.mustNotCall());
  // The error reaches the runner straight from the Web Worker while results
  // travel through the harness, so it can arrive before them.
  runner.on('message', common.mustCallAtLeast((message) => {
    switch (message.type) {
      case 'result':
        assert.strictEqual(message.result.name, 'reported before error');
        assert.strictEqual(message.result.status, 0);
        break;
      case 'completion':
        onCompletion(message.status);
        break;
      default:
        assert.fail(`Unexpected message type: ${message.type}`);
    }
  }, 1));
}
