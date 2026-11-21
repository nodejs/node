'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { Worker } = require('worker_threads');

const harnessPath = fixtures.path('wpt', 'resources', 'testharness.js');
const harness = {
  code: fs.readFileSync(harnessPath, 'utf8'),
  filename: harnessPath,
};

async function check({
  name,
  code,
  allowed,
  passes = 1,
  message = /uncaught rejection probe/,
}) {
  const label = name;
  const script = {
    filename: fixtures.path('wpt-rejection-probe.js'),
    code,
  };
  const workerData = {
    testRelativePath: 'rejection-probe.any.js',
    wptRunner: path.join(__dirname, '../common/wpt.js'),
    wptPath: 'compression',
    harness,
    scriptsToRun: [script],
  };

  let result;
  const statuses = [];
  const worker = new Worker(path.join(__dirname, '../common/wpt/worker.js'), {
    workerData,
  });
  worker.on('message', (message) => {
    if (message.type === 'result') {
      statuses.push(message.result.status);
    } else if (message.type === 'completion') {
      result = message.status;
      worker.terminate();
    } else {
      assert.fail(`Unexpected message type: ${message.type}`);
    }
  });
  worker.on('error', (error) => {
    result = { status: 1, message: error.message };
  });
  const finished = new Promise((resolve) => worker.once('exit', resolve));
  const timeout = setTimeout(() => {
    worker.terminate();
    assert.fail(`WPT rejection probe did not finish: ${label}`);
  }, common.platformTimeout(10_000));

  try {
    await finished;
  } finally {
    clearTimeout(timeout);
  }

  assert(result, label);
  assert.strictEqual(result.status, allowed ? 0 : 1, label);
  if (allowed) {
    assert.deepStrictEqual(statuses, Array(passes).fill(0), label);
  } else {
    assert.match(result.message, message, label);
  }
}

(async () => {
  for (const [setup, allowed] of [
    ['', false],
    ['setup({ allow_uncaught_exception: false });', false],
    ['setup({ allow_uncaught_exception: true });', true],
    ['setup(() => {}, { allow_uncaught_exception: true });', true],
    ['setup(null, { allow_uncaught_exception: true });', true],
    ['setup({ allow_uncaught_exception: true }); setup({});', true],
    ['setup({ allow_uncaught_exception: true }); setup({ allow_uncaught_exception: false });', false],
    ['setup(Object.defineProperty({}, "allow_uncaught_exception", { value: true }));', false],
  ]) {
    await check({
      name: setup || 'default settings',
      code: `${setup}
        const t = async_test('rejection probe');
        Promise.reject(new Error('uncaught rejection probe'));
        setTimeout(() => t.done(), 0);`,
      allowed,
    });
  }

  for (const allowed of [false, true]) {
    await check({
      name: `nested setup allows rejection: ${allowed}`,
      code: `setup(() => setup({ allow_uncaught_exception: ${allowed} }),
                  { allow_uncaught_exception: ${!allowed} });
        const t = async_test('rejection probe');
        Promise.reject(new Error('uncaught rejection probe'));
        setTimeout(() => t.done(), 0);`,
      allowed,
    });

    await check({
      name: `late setup preserves allowance: ${allowed}`,
      code: `setup({ allow_uncaught_exception: ${allowed} });
        const t = async_test('rejection probe');
        test(() => {}, 'first result');
        setup(() => assert_unreached('late setup callback'),
              { allow_uncaught_exception: ${!allowed} });
        Promise.reject(new Error('uncaught rejection probe'));
        setTimeout(() => t.done(), 0);`,
      allowed,
      passes: 2,
    });

    await check({
      name: `promise_setup applies deferred allowance: ${allowed}`,
      code: `promise_setup(async () => {}, { allow_uncaught_exception: ${allowed} });
        setup({ allow_uncaught_exception: ${!allowed} });
        promise_test(async () => {
          Promise.reject(new Error('uncaught rejection probe'));
          await new Promise(resolve => setTimeout(resolve, 0));
        }, 'rejection probe');`,
      allowed,
    });

    await check({
      name: `late promise_setup preserves allowance: ${allowed}`,
      code: `setup({ allow_uncaught_exception: ${allowed} });
        const t = async_test('rejection probe');
        test(() => {}, 'first result');
        promise_setup(async () => {
          Promise.reject(new Error('uncaught rejection probe'));
          setTimeout(() => t.done(), 0);
        }, { allow_uncaught_exception: ${!allowed} });`,
      allowed,
      passes: 2,
    });

    await check({
      name: `single_test still fails with allowance: ${allowed}`,
      code: `setup({ single_test: true, allow_uncaught_exception: ${allowed} });
        setup({ single_test: false });
        Promise.reject(new Error('uncaught rejection probe'));
        setTimeout(done, 0);`,
      allowed: false,
    });
  }

  for (const setup of [
    'setup(() => { throw new Error("setup failure"); }, { allow_uncaught_exception: true });',
    'promise_setup(async () => { throw new Error("setup failure"); }, { allow_uncaught_exception: true });',
  ]) {
    await check({
      name: setup,
      code: `${setup}\npromise_test(async () => {}, 'setup must succeed first');`,
      allowed: false,
      message: /setup failure/,
    });
  }

  await check({
    name: 'unexpected rejection fails even with a permanently pending test',
    code: `async_test('pending forever');
      Promise.reject(new Error('uncaught rejection probe'));`,
    allowed: false,
  });
})().then(common.mustCall());
