'use strict';

const common = require('../common');
const dc = require('diagnostics_channel');

const channel = dc.tracingChannel('test');
const nextChannel = dc.tracingChannel('test:next');

const handlers = {
  start: common.mustNotCall(),
  end: common.mustNotCall(),
  asyncStart: common.mustNotCall(),
  asyncEnd: common.mustNotCall(),
  error: common.mustNotCall(),
};

const nextHandlers = {
  start: common.mustNotCall(),
  end: common.mustNotCall(),
  asyncStart: common.mustNotCall(),
  asyncEnd: common.mustNotCall(),
  error: common.mustNotCall(),
};

// Subscribe after traceIterator call - no events should fire for the iterator
// or for subsequent next() calls since the iterator was not wrapped
const iter = channel.traceIterator(function*() {
  yield 1;
}, {});

channel.subscribe(handlers);
nextChannel.subscribe(nextHandlers);

iter.next();
