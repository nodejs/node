'use strict';

const {
  Symbol,
  Date,
  DatePrototypeGetMilliseconds,
  DatePrototypeToUTCString,
} = primordials;

const { setUnrefTimeout } = require('internal/timers');

const { InternalPerformanceEntry } = require('internal/perf/performance_entry');

const {
  enqueue,
  hasObserver,
} = require('internal/perf/observe');

let utcCache;

function utcDate() {
  if (!utcCache) cache();
  return utcCache;
}

function cache() {
  const d = new Date();
  utcCache = DatePrototypeToUTCString(d);
  setUnrefTimeout(resetCache, 1000 - DatePrototypeGetMilliseconds(d));
}

function resetCache() {
  utcCache = undefined;
}

function emitStatistics(statistics) {
  if (!hasObserver('http') || statistics == null) return;
  const startTime = statistics.startTime;
  const diff = process.hrtime(startTime);
  const entry = new InternalPerformanceEntry(
    'HttpRequest',
    'http',
    startTime[0] * 1000 + startTime[1] / 1e6,
    diff[0] * 1000 + diff[1] / 1e6,
    undefined,
  );
  enqueue(entry);
}

module.exports = {
  kOutHeaders: Symbol('kOutHeaders'),
  kNeedDrain: Symbol('kNeedDrain'),
  utcDate,
  emitStatistics,
};
