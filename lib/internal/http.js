'use strict';

const {
  Symbol,
  Date,
  DatePrototypeGetMilliseconds,
  DatePrototypeToUTCString,
} = primordials;

const { setUnrefTimeout } = require('internal/timers');

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

module.exports = {
  kOutHeaders: Symbol('kOutHeaders'),
  kNeedDrain: Symbol('kNeedDrain'),
  utcDate,
};
