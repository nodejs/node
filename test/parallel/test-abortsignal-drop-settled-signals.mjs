// Flags: --expose_gc

import * as common from '../common/index.mjs';

if (common.isASan) {
  common.skip('ASan messes with memory measurements');
}

import { describe, it } from 'node:test';

const makeSubsequentCalls = (limit, done, holdReferences = false) => {
  let dependantSymbol;
  let signalRef;
  const ac = new AbortController();
  const retainedSignals = [];
  const handler = () => { };

  function run(iteration) {
    if (iteration > limit) {
      // This setImmediate is necessary to ensure that in the last iteration the remaining signal is GCed (if not
      // retained)
      setImmediate(() => {
        global.gc();
        done(ac.signal, dependantSymbol);
      });
      return;
    }

    if (holdReferences) {
      retainedSignals.push(AbortSignal.any([ac.signal]));
    } else {
      // Using a WeakRef to avoid retaining information that will interfere with the test
      signalRef = new WeakRef(AbortSignal.any([ac.signal]));
      signalRef.deref().addEventListener('abort', handler);
    }

    if (!dependantSymbol) {
      const kDependantSignals = Object.getOwnPropertySymbols(ac.signal).find(
        (s) => s.toString() === 'Symbol(kDependantSignals)'
      );
      dependantSymbol = kDependantSignals;
    }

    setImmediate(() => {
      // Removing the event listener at some moment in the future
      // Which will then allow the signal to be GCed
      signalRef?.deref()?.removeEventListener('abort', handler);
      run(iteration + 1);
    });
  }

  run(1);
};

const runShortLivedSourceSignal = (limit, done) => {
  const signalRefs = new Set();

  function run(iteration) {
    if (iteration > limit) {
      global.gc();
      done(signalRefs);
      return;
    }

    const ac = new AbortController();
    signalRefs.add(new WeakRef(ac.signal));
    AbortSignal.any([ac.signal]);

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

it('does not prevent source signal from being GCed if it is short-lived', (t, done) => {
  runShortLivedSourceSignal(limit, (signalRefs) => {
    setImmediate(() => {
      const unGCedSignals = [...signalRefs].filter((ref) => ref.deref());

      t.assert.equal(unGCedSignals, 0);
      done();
    });
  });
});

it('drops settled dependant signals when signal is composite', (t, done) => {
  const controllers = Array.from({ length: 2 }, () => new AbortController());
  const composedSignal1 = AbortSignal.any([controllers[0].signal]);
  const composedSignalRef = new WeakRef(AbortSignal.any([composedSignal1, controllers[1].signal]));

  const kDependantSignals = Object.getOwnPropertySymbols(controllers[0].signal).find(
    (s) => s.toString() === 'Symbol(kDependantSignals)'
  );

  setImmediate(() => {
    global.gc();

    t.assert.equal(composedSignalRef.deref(), undefined);
    t.assert.equal(controllers[0].signal[kDependantSignals].size, 2);
    t.assert.equal(controllers[1].signal[kDependantSignals].size, 1);

    setImmediate(() => {
      t.assert.equal(controllers[0].signal[kDependantSignals].size, 0);
      t.assert.equal(controllers[1].signal[kDependantSignals].size, 0);

      done();
    });
  });
});
