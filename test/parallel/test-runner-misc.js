'use strict';
const common = require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const { setTimeout } = require('timers/promises');

if (process.argv[2] === 'child') {
  const test = require('node:test');

  if (process.argv[3] === 'abortSignal') {
    assert.throws(() => test({ signal: {} }), {
      code: 'ERR_INVALID_ARG_TYPE',
      name: 'TypeError'
    });

    let testSignal;
    test({ timeout: 10 }, common.mustCall(async ({ signal }) => {
      assert.strictEqual(signal.aborted, false);
      testSignal = signal;
      await setTimeout(50);
    })).finally(common.mustCall(() => {
      test(() => assert.strictEqual(testSignal.aborted, true));
    }));

    test.describe('Abort Signal in describe', common.mustCall(({ signal }) => {
      test.it('Supports AbortSignal', common.mustCall(() => {
        assert.strictEqual(signal.aborted, false);
      }));
    }));

    let describeSignal;
    test.describe('describe signal timeout', { timeout: 10 }, common.mustCall(async ({ signal }) => {
      await test.it('nested it timeout', common.mustCall(async (t) => {
        assert.strictEqual(t.signal.aborted, false);
        describeSignal = t.signal;
        await setTimeout(50);
      }));
    }));

    test(() => assert.strictEqual(describeSignal.aborted, true));

    let manualAbortSignal;
    test.describe('describe signal manual abort', common.mustCall((t) => {
      const controller = new AbortController();
      t.signal.addEventListener('abort', () => controller.abort());

      return test.it('nested manual abort', { signal: controller.signal }, common.mustCall(async (t) => {
        assert.strictEqual(t.signal.aborted, false);
        manualAbortSignal = t.signal;
        controller.abort();
        await setTimeout(50);
      }));
    }));
    test(() => assert.strictEqual(manualAbortSignal.aborted, true));
  } else assert.fail('unreachable');
} else {
  const child = spawnSync(process.execPath, [__filename, 'child', 'abortSignal']);
  const stdout = child.stdout.toString();
  assert.match(stdout, /pass 4$/m);
  assert.match(stdout, /fail 0$/m);
  assert.match(stdout, /cancelled 3$/m);
  assert.strictEqual(child.status, 1);
  assert.strictEqual(child.signal, null);
}
