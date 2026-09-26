'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const fs = require('fs');
const path = require('path');
const { backends } = require('../common/wpt');

const harnessPath = fixtures.path('wpt', 'resources', 'testharness.js');
const harness = {
  code: fs.readFileSync(harnessPath, 'utf8'),
  filename: harnessPath,
};

async function check(backend, webWorker, {
  name,
  code,
  allowed,
  passes = 1,
  singleTest = false,
  message = /uncaught rejection probe/,
}) {
  const label = `${backend}, ${webWorker}, ${name}`;
  const script = {
    filename: fixtures.path('wpt-rejection-probe.js'),
    // Use a direct worker script for single_test: .any.js wrappers call
    // done() automatically, completing the implicit test before its timers.
    code: `${webWorker && singleTest ? 'importScripts("/resources/testharness.js");' : ''}\n${code}`,
  };
  const workerData = {
    testRelativePath: 'rejection-probe.any.js',
    wptRunner: path.join(__dirname, '../common/wpt.js'),
    wptPath: 'compression',
    harness,
    scriptsToRun: webWorker ? [] : [script],
    webWorker: webWorker ? {
      path: script.filename,
      modifiedScript: script,
      isAnyTest: !singleTest,
      variant: '',
      scripts: [],
    } : undefined,
  };

  let result;
  const statuses = [];
  const handle = backends[backend](['--experimental-web-worker'], workerData, {
    message(message) {
      if (message.type === 'result') {
        statuses.push(message.result.status);
      } else if (message.type === 'completion') {
        // The process backend can complete before its uncaught-error handler
        // exits. Preserve any failure it already reported.
        result ||= message.status;
        handle.kill();
      } else {
        assert.fail(`Unexpected message type: ${message.type}`);
      }
    },
    failure(error) {
      if (result) return false;
      result = { status: 1, message: error.message };
      return true;
    },
  });
  const timeout = setTimeout(() => {
    handle.kill();
    assert.fail(`WPT rejection probe did not finish: ${label}`);
  }, common.platformTimeout(10_000));

  try {
    await handle.finished;
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
  for (const backend of ['thread', 'process']) {
    for (const webWorker of [false, true]) {
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
        await check(backend, webWorker, {
          name: setup || 'default settings',
          code: `${setup}
            const t = async_test('rejection probe');
            Promise.reject(new Error('uncaught rejection probe'));
            setTimeout(() => t.done(), 0);`,
          allowed,
        });
      }

      for (const allowed of [false, true]) {
        await check(backend, webWorker, {
          name: `nested setup allows rejection: ${allowed}`,
          code: `setup(() => setup({ allow_uncaught_exception: ${allowed} }),
                      { allow_uncaught_exception: ${!allowed} });
            const t = async_test('rejection probe');
            Promise.reject(new Error('uncaught rejection probe'));
            setTimeout(() => t.done(), 0);`,
          allowed,
        });

        await check(backend, webWorker, {
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

        await check(backend, webWorker, {
          name: `promise_setup applies deferred allowance: ${allowed}`,
          code: `promise_setup(async () => {}, { allow_uncaught_exception: ${allowed} });
            setup({ allow_uncaught_exception: ${!allowed} });
            promise_test(async () => {
              Promise.reject(new Error('uncaught rejection probe'));
              await new Promise(resolve => setTimeout(resolve, 0));
            }, 'rejection probe');`,
          allowed,
        });

        await check(backend, webWorker, {
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

        await check(backend, webWorker, {
          name: `single_test still fails with allowance: ${allowed}`,
          code: `setup({ single_test: true, allow_uncaught_exception: ${allowed} });
            setup({ single_test: false });
            Promise.reject(new Error('uncaught rejection probe'));
            setTimeout(done, 0);`,
          allowed: false,
          singleTest: true,
        });
      }

      for (const setup of [
        'setup(() => { throw new Error("setup failure"); }, { allow_uncaught_exception: true });',
        'promise_setup(async () => { throw new Error("setup failure"); }, { allow_uncaught_exception: true });',
      ]) {
        await check(backend, webWorker, {
          name: setup,
          code: `${setup}\npromise_test(async () => {}, 'setup must succeed first');`,
          allowed: false,
          message: /setup failure/,
        });
      }

      await check(backend, webWorker, {
        name: 'unexpected rejection fails even with a permanently pending test',
        code: `async_test('pending forever');
          Promise.reject(new Error('uncaught rejection probe'));`,
        allowed: false,
      });
    }
  }
})().then(common.mustCall());
