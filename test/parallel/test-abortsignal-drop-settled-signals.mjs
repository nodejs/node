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

  // let i = 0;
  function run(iteration) {
    if (iteration > limit) {
      global.gc();
      done(ac.signal, dependantSymbol);
      return;
    }

    if (holdReferences) {
      retainedSignals.push(AbortSignal.any([ac.signal]));
    } else {
      AbortSignal.any([ac.signal]);
    }

    if (!dependantSymbol) {
      const kDependantSignals = Object.getOwnPropertySymbols(ac.signal).filter(
        (s) => s.toString() === 'Symbol(kDependantSignals)'
      )[0];
      dependantSymbol = kDependantSignals;
    }

    setImmediate(() => run(iteration + 1));
  }

  run(1);
};

const limit = 10_000;

describe('when there is a long-lived signal', () => {
  it('drops settled dependant signals', (t, done) => {
    makeSubsequentCalls(limit, (signal, depandantSignalsKey) => {
      setImmediate(() => {
        t.assert.equal(signal[depandantSignalsKey].size, 0);
        done();
      });
    });
  });

  it('keeps all active dependant signals', (t, done) => {
    makeSubsequentCalls(limit, (signal, depandantSignalsKey) => {
      t.assert.equal(signal[depandantSignalsKey].size, limit);

      done();
    }, true);
  });
});
