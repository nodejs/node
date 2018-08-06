'use strict';

const common = require('../common.js');

const bench = common.createBenchmark(main, {
  n: [100000],
  method: ['trace', 'emit', 'isTraceCategoryEnabled', 'categoryGroupEnabled']
}, {
  flags: ['--expose-internals', '--trace-event-categories', 'foo']
});

const {
  TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN: kBeforeEvent
} = process.binding('constants').trace;

function doEmit(n, emit) {
  bench.start();
  for (var i = 0; i < n; i++) {
    emit(kBeforeEvent, 'foo', 'test', 0, 'arg1', 1);
  }
  bench.end(n);
}

function doTrace(n, trace) {
  bench.start();
  for (var i = 0; i < n; i++) {
    trace(kBeforeEvent, 'foo', 'test', 0, 'test');
  }
  bench.end(n);
}

function doIsTraceCategoryEnabled(n, isTraceCategoryEnabled) {
  bench.start();
  for (var i = 0; i < n; i++) {
    isTraceCategoryEnabled('foo');
    isTraceCategoryEnabled('bar');
  }
  bench.end(n);
}

function doCategoryGroupEnabled(n, categoryGroupEnabled) {
  bench.start();
  for (var i = 0; i < n; i++) {
    categoryGroupEnabled('foo');
    categoryGroupEnabled('bar');
  }
  bench.end(n);
}

function main({ n, method }) {
  const { internalBinding } = require('internal/test/binding');

  const {
    trace,
    isTraceCategoryEnabled,
    emit,
    categoryGroupEnabled
  } = internalBinding('trace_events');

  switch (method) {
    case '':
    case 'trace':
      doTrace(n, trace);
      break;
    case 'emit':
      doEmit(n, emit);
      break;
    case 'isTraceCategoryEnabled':
      doIsTraceCategoryEnabled(n, isTraceCategoryEnabled);
      break;
    case 'categoryGroupEnabled':
      doCategoryGroupEnabled(n, categoryGroupEnabled);
      break;
    default:
      throw new Error(`Unexpected method "${method}"`);
  }
}
