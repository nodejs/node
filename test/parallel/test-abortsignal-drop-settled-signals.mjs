// Flags: --expose_gc

import * as common from '../common/index.mjs';

if (common.isASan) {
  common.skip('ASan messes with memory measurements');
}

import { describe, it } from 'node:test';

const makeSubsequentCalls = (limit, done, holdReferences = false) => {
  const ac = new AbortController();
  const retainedSignals = [];
  let dependantSymbol;

  let i = 0;
  function run() {
    if (holdReferences) {
      retainedSignals.push(AbortSignal.any([ac.signal]));
    } else {
      AbortSignal.any([ac.signal]);
    }

    if (!dependantSymbol) {
      const kDependantSignals = Object.getOwnPropertySymbols(ac.signal).filter(
        (s) => s.toString() === 'Symbol(kDependantSignals)'
      )[0];
      dependantSymbol ??= kDependantSignals;
    }

    if (++i >= limit) {
      global.gc();
      done(ac.signal[dependantSymbol].size);
      return;
    }
    setImmediate(run);
  }
  return run();
};

const limit = 50000;

describe('when there is a long-lived signal', () => {
  it('drops settled signals', (t, done) => {
    makeSubsequentCalls(limit, (totalDependantSignals) => {
      // We're unable to assert how many signals are dropped (since it depends on gc), but we can assert that some are.
      t.assert.equal(totalDependantSignals < limit, true);

      done();
    });
  });

  it('keeps all dependant signals', (t, done) => {
    makeSubsequentCalls(limit, (totalDependantSignals) => {
      t.assert.equal(totalDependantSignals, limit);

      done();
    }, true);
  });
});
