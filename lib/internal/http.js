'use strict';

const {
  Symbol,
} = primordials;

const { setUnrefTimeout } = require('internal/timers');
const { PerformanceEntry, notify } = internalBinding('performance');

const VALID_PATH_REGEX = /^[_0-9A-Za-z]+(?:\.[_0-9A-Za-z]+)*\.?:(?:[1-9]|[1-9][0-9]{1,2}|[1-5][0-9]{3}|6[0-4][0-9]{3}|65[0-4][0-9]{2}|655[0-2][0-9]|6553[0-5])$/;
let nowCache;
let utcCache;

function nowDate() {
  if (!nowCache) cache();
  return nowCache;
}

function utcDate() {
  if (!utcCache) cache();
  return utcCache;
}

function cache() {
  const d = new Date();
  nowCache = d.valueOf();
  utcCache = d.toUTCString();
  setUnrefTimeout(resetCache, 1000 - d.getMilliseconds());
}

function resetCache() {
  nowCache = undefined;
  utcCache = undefined;
}

function isValidCONNECTPath(path) {
  if (!VALID_PATH_REGEX.test(path)) {
    return false;
  }
  return true;
}

class HttpRequestTiming extends PerformanceEntry {
  constructor(statistics) {
    super();
    this.name = 'HttpRequest';
    this.entryType = 'http';
    const startTime = statistics.startTime;
    const diff = process.hrtime(startTime);
    this.duration = diff[0] * 1000 + diff[1] / 1e6;
    this.startTime = startTime[0] * 1000 + startTime[1] / 1e6;
  }
}

function emitStatistics(statistics) {
  notify('http', new HttpRequestTiming(statistics));
}

module.exports = {
  kOutHeaders: Symbol('kOutHeaders'),
  kNeedDrain: Symbol('kNeedDrain'),
  nowDate,
  utcDate,
  emitStatistics,
  isValidCONNECTPath
};
