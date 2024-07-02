'use strict';
const common = require('../common.js');
const EventEmitter = require('events').EventEmitter;

function _onceWrap1(target, type, listener) {
  const state = { fired: false, target, type, listener };

  function wrapped(...args) {
    if (!state.fired) {
      state.fired = true;
      target.removeListener(type, wrapped);
      listener.apply(target, args);
    }
  }

  wrapped.listener = listener;

  if (target.shouldStoreReference) {
    state.wrapFn = wrapped;
  }

  return wrapped;
}

function _onceWrap2(target, type, listener) {
  function wrapped(...args) {
    if (!wrapped.fired) {
      wrapped.fired = true;
      target.removeListener(type, wrapped);
      listener.apply(target, args);
    }
  }

  wrapped.listener = listener;
  wrapped.fired = false;
  return wrapped;
}

const bench = common.createBenchmark(main, {
  n: [5e6],
  listeners: [5, 50],
  method: ['_onceWrap1', '_onceWrap2'],
});

function main({ n, listeners, method }) {
  const ee = new EventEmitter();
  ee.setMaxListeners(listeners * 2 + 1);

  ee.shouldStoreReference = method === '_onceWrap1';

  const wrapper = (method === '_onceWrap1') ? _onceWrap1 : _onceWrap2;

  for (let k = 0; k < listeners; k += 1) {
    ee.on('dummy', () => {});
  }

  bench.start();
  for (let i = 0; i < n; i += 1) {
    wrapper(ee, 'dummy', () => {});
  }
  bench.end(n);
}
