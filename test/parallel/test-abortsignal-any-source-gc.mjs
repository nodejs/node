// Flags: --expose-gc

import '../common/index.mjs';
import { gcUntil } from '../common/gc.js';
import assert from 'node:assert/strict';
import { it } from 'node:test';

for (const nested of [false, true]) {
  for (const accessor of ['aborted', 'reason', 'throwIfAborted']) {
    it(`preserves ${accessor} after source GC (nested: ${nested})`, async () => {
      let controller = new AbortController();
      const sourceRef = new WeakRef(controller.signal);
      let signal = AbortSignal.any([controller.signal]);
      if (nested) signal = AbortSignal.any([signal]);
      const reason = { message: 'stop' };

      controller.abort(reason);
      controller = null;

      // Do not observe the composite or attach a listener before source GC.
      await gcUntil('source signal is collected', () => sourceRef.deref() === undefined);

      // Exercise each entry point before any other accessor can refresh state.
      if (accessor === 'aborted') assert.strictEqual(signal.aborted, true);
      if (accessor === 'reason') assert.strictEqual(signal.reason, reason);
      assert.throws(() => signal.throwIfAborted(), (err) => err === reason);
      assert.strictEqual(signal.aborted, true);
      assert.strictEqual(signal.reason, reason);
    });
  }
}
