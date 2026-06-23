'use strict';

const common = require('../common');
const assert = require('node:assert');
const { test } = require('node:test');
const nodeTimersPromises = require('node:timers/promises');

const bench = common.createBenchmark(main, {
  n: [1, 10, 100, 1000],
  mode: [
    'enable-empty-apis',
    'enable-setTimeout',
    'enable-setInterval',
    'enable-setImmediate',
    'enable-Date',
    'enable-scheduler.wait',
    'enable-AbortSignal.timeout',
    'enable-all',
    'enable-default',
    'setTimeout',
    'setInterval',
    'setImmediate',
    'scheduler.wait',
    'AbortSignal.timeout',
    'Date',
    'setTime',
    'runAll',
  ],
}, {
  // We don't want to test the reporter here.
  flags: ['--test-reporter=./benchmark/fixtures/empty-test-reporter.js'],
});

function benchmarkEnable(n, mode) {
  const enableMode = mode.replace('enable-', '');
  let enableOptions = { apis: [enableMode] };

  if (enableMode === 'all') {
    enableOptions.apis = ['setTimeout', 'setInterval', 'setImmediate', 'Date', 'scheduler.wait', 'AbortSignal.timeout'];
  }

  if (enableMode === 'empty-apis') {
    enableOptions.apis = [];
  }

  if (enableMode === 'default') {
    enableOptions = undefined;
  }

  test((t) => {
    bench.start();

    for (let i = 0; i < n; i++) {
      t.mock.timers.enable(enableOptions);
      t.mock.timers.reset();
    }

    bench.end(n);
  });
}

function benchmarkSetTimeout(n) {
  test((t) => {
    let noDead;

    t.mock.timers.enable({ apis: ['setTimeout'] });
    bench.start();

    for (let i = 0; i < n; i++) {
      setTimeout(() => {
        noDead = i;
      }, i + 1);
    }

    t.mock.timers.tick(n + 1);
    bench.end(n);

    assert.strictEqual(noDead, n - 1);
  });
}

function benchmarkSetInterval(n) {
  test((t) => {
    let noDead = 0;

    t.mock.timers.enable({ apis: ['setInterval'] });

    setInterval(() => {
      noDead++;
    }, 1);

    bench.start();

    t.mock.timers.tick(n);

    bench.end(n);

    assert.strictEqual(noDead, n);
  });
}

function benchmarkSetImmediate(n) {
  test((t) => {
    let noDead;

    t.mock.timers.enable({ apis: ['setImmediate'] });

    bench.start();

    for (let i = 0; i < n; i++) {
      setImmediate(() => {
        noDead = i;
      });
    }

    t.mock.timers.tick(0);
    bench.end(n);

    assert.strictEqual(noDead, n - 1);
  });
}

function benchmarkSchedulerWait(n) {
  test(async (t) => {
    const promises = [];
    let noDead;

    t.mock.timers.enable({ apis: ['scheduler.wait'] });

    bench.start();

    for (let i = 0; i < n; i++) {
      promises.push(nodeTimersPromises.scheduler.wait(i + 1).then(() => {
        noDead = i;
      }));
    }

    t.mock.timers.tick(n + 1);
    await Promise.all(promises);
    bench.end(n);

    assert.strictEqual(noDead, n - 1);
  });
}

function benchmarkAbortSignalTimeout(n) {
  test((t) => {
    let noDead;

    t.mock.timers.enable({ apis: ['AbortSignal.timeout'] });

    bench.start();

    for (let i = 0; i < n; i++) {
      noDead = AbortSignal.timeout(i + 1);
    }

    t.mock.timers.tick(n + 1);
    bench.end(n);

    assert.strictEqual(noDead.aborted, true);
  });
}

function benchmarkDate(n) {
  test((t) => {
    let noDead;

    t.mock.timers.enable({ apis: ['Date'] });

    bench.start();

    for (let i = 0; i < n; i++) {
      noDead = Date.now();
    }

    bench.end(n);

    assert.strictEqual(noDead, 0);
  });
}

function benchmarkSetTime(n) {
  test((t) => {
    let noDead;

    t.mock.timers.enable({ apis: ['Date'] });

    bench.start();

    for (let i = 0; i < n; i++) {
      t.mock.timers.setTime(i);
      noDead = Date.now();
    }

    bench.end(n);

    assert.strictEqual(noDead, n - 1);
  });
}

function benchmarkRunAll(n) {
  test((t) => {
    let noDead = 0;

    t.mock.timers.enable({ apis: ['setTimeout'] });

    for (let i = 0; i < n; i++) {
      setTimeout(() => {
        noDead++;
      }, i + 1);
    }

    bench.start();

    t.mock.timers.runAll();

    bench.end(n);

    assert.strictEqual(noDead, n);
  });
}

function main({ n, mode }) {
  switch (mode) {
    case 'enable-empty-apis':
    case 'enable-setTimeout':
    case 'enable-setInterval':
    case 'enable-setImmediate':
    case 'enable-Date':
    case 'enable-scheduler.wait':
    case 'enable-AbortSignal.timeout':
    case 'enable-all':
    case 'enable-default':
      benchmarkEnable(n, mode);
      break;
    case 'setTimeout':
      benchmarkSetTimeout(n);
      break;
    case 'setInterval':
      benchmarkSetInterval(n);
      break;
    case 'setImmediate':
      benchmarkSetImmediate(n);
      break;
    case 'scheduler.wait':
      benchmarkSchedulerWait(n);
      break;
    case 'AbortSignal.timeout':
      benchmarkAbortSignalTimeout(n);
      break;
    case 'Date':
      benchmarkDate(n);
      break;
    case 'setTime':
      benchmarkSetTime(n);
      break;
    case 'runAll':
      benchmarkRunAll(n);
      break;
  }
}
