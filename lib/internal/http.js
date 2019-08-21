'use strict';

const { setUnrefTimeout } = require('internal/timers');
const { PerformanceEntry, notify } = internalBinding('performance');

var nowCache;
var utcCache;

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

function ondrain() {
  if (this._httpMessage) this._httpMessage.emit('drain');
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
  ondrain,
  nowDate,
  utcDate,
  emitStatistics
};
