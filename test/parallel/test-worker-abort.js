'use strict';
const common = require('../common');
const { Worker, isMainThread } = require('worker_threads');

if (isMainThread) {
  const ac = new AbortController();
  const w = new Worker(__filename, { signal: ac.signal });
  w.on('online', common.mustCall(() => {
    ac.abort();
  }));
  w.on('exit', common.mustCall());
} else {
  setInterval(() => {}, 1000);
}
