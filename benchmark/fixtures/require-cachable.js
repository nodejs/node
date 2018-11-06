'use strict';

const list = require('internal/bootstrap/cache');
const {
  isMainThread
} = require('worker_threads');

for (const key of list.cachableBuiltins) {
  if (!isMainThread && key === 'trace_events') {
    continue;
  }
  require(key);
}
