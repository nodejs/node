import * as common from '../common/index.mjs';
import * as events from 'node:events';
import * as assert from 'node:assert';
import { describe, it } from 'node:test';

describe('events.addAbortListener', () => {
  it('should throw if signal not provided', () => {
    assert.throws(() => events.addAbortListener(), { code: 'ERR_INVALID_ARG_TYPE' });
  });

  it('should throw if provided signal is invalid', () => {
    assert.throws(() => events.addAbortListener(undefined), { code: 'ERR_INVALID_ARG_TYPE' });
    assert.throws(() => events.addAbortListener(null), { code: 'ERR_INVALID_ARG_TYPE' });
    assert.throws(() => events.addAbortListener({}), { code: 'ERR_INVALID_ARG_TYPE' });
  });

  it('should throw if listener is not a function', () => {
    const { signal } = new AbortController();
    assert.throws(() => events.addAbortListener(signal), { code: 'ERR_INVALID_ARG_TYPE' });
    assert.throws(() => events.addAbortListener(signal, {}), { code: 'ERR_INVALID_ARG_TYPE' });
    assert.throws(() => events.addAbortListener(signal, undefined), { code: 'ERR_INVALID_ARG_TYPE' });
  });

  it('should return a Disposable', () => {
    const { signal } = new AbortController();
    const disposable = events.addAbortListener(signal, common.mustNotCall());

    assert.strictEqual(typeof disposable[Symbol.dispose], 'function');
  });

  it('should execute the listener immediately for aborted runners', () => {
    const disposable = events.addAbortListener(AbortSignal.abort(), common.mustCall());
    assert.strictEqual(typeof disposable[Symbol.dispose], 'function');
  });

  it('should execute the listener even when event propagation stopped', () => {
    const controller = new AbortController();
    const { signal } = controller;

    signal.addEventListener('abort', (e) => e.stopImmediatePropagation());
    events.addAbortListener(
      signal,
      common.mustCall((e) => assert.strictEqual(e.target, signal)),
    );

    controller.abort();
  });

  it('should remove event listeners when disposed', () => {
    const controller = new AbortController();
    const disposable = events.addAbortListener(controller.signal, common.mustNotCall());
    disposable[Symbol.dispose]();
    controller.abort();
  });
});
