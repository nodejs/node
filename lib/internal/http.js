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

const { now } = require('internal/perf/utils');

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
  const entry = new InternalPerformanceEntry(
    statistics.type,
    'http',
    startTime,
    now() - startTime,
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
