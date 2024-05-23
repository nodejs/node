'use strict';

const {
  MathFloor,
  NumberPrototypeToFixed,
  Number,
  StringPrototypeSplit,
  StringPrototypePadStart,
} = primordials;

const { trace } = internalBinding('trace_events');

const {
  CHAR_LOWERCASE_B: kTraceBegin,
  CHAR_LOWERCASE_E: kTraceEnd,
  CHAR_LOWERCASE_N: kTraceInstant,
} = require('internal/constants');

function pad(value) {
  return StringPrototypePadStart(`${value}`, 2, '0');
}

const kSecond = 1000;
const kMinute = 60 * kSecond;
const kHour = 60 * kMinute;

function formatTime(ms) {
  let hours = 0;
  let minutes = 0;
  let seconds = 0;

  if (ms >= kSecond) {
    if (ms >= kMinute) {
      if (ms >= kHour) {
        hours = MathFloor(ms / kHour);
        ms = ms % kHour;
      }
      minutes = MathFloor(ms / kMinute);
      ms = ms % kMinute;
    }
    seconds = ms / kSecond;
  }

  if (hours !== 0 || minutes !== 0) {
    ({ 0: seconds, 1: ms } = StringPrototypeSplit(
      NumberPrototypeToFixed(seconds, 3),
      '.',
    ));
    const res = hours !== 0 ? `${hours}:${pad(minutes)}` : minutes;
    return `${res}:${pad(seconds)}.${ms} (${hours !== 0 ? 'h:m' : ''}m:ss.mmm)`;
  }

  if (seconds !== 0) {
    return `${NumberPrototypeToFixed(seconds, 3)}s`;
  }

  return `${Number(NumberPrototypeToFixed(ms, 3))}ms`;
}

/**
 * @typedef {(label: string, timeFormatted: string, args?: any[]) => void} LogImpl
 */

/**
 * Returns true if label was found
 * @param {string} timesStore
 * @param {string} implementation
 * @param {LogImpl} logImp
 * @param {string} label
 * @param {any} args
 * @returns {boolean}
 */
function timeLogImpl(timesStore, implementation, logImp, label, args) {
  const time = timesStore.get(label);
  if (time === undefined) {
    process.emitWarning(`No such label '${label}' for ${implementation}`);
    return false;
  }

  const duration = process.hrtime(time);
  const ms = duration[0] * 1000 + duration[1] / 1e6;

  const formatted = formatTime(ms);

  if (args === undefined) {
    logImp(label, formatted);
  } else {
    logImp(label, formatted, args);
  }

  return true;
}

/**
 * @param {SafeMap} timesStore
 * @param {string} traceCategory
 * @param {string} implementation
 * @param {string} label
 * @returns {void}
 */
function time(timesStore, traceCategory, implementation, label = 'default') {
  // Coerces everything other than Symbol to a string
  label = `${label}`;
  if (timesStore.has(label)) {
    process.emitWarning(`Label '${label}' already exists for ${implementation}`);
    return;
  }

  trace(kTraceBegin, traceCategory, `time::${label}`, 0);
  timesStore.set(label, process.hrtime());
}

/**
 * @param {SafeMap} timesStore
 * @param {string} traceCategory
 * @param {string} implementation
 * @param {LogImpl} logImpl
 * @param {string} label
 * @returns {void}
 */
function timeEnd(timesStore, traceCategory, implementation, logImpl, label = 'default') {
  // Coerces everything other than Symbol to a string
  label = `${label}`;

  const found = timeLogImpl(timesStore, implementation, logImpl, label);
  trace(kTraceEnd, traceCategory, `time::${label}`, 0);

  if (found) {
    timesStore.delete(label);
  }
}

/**
 * @param {SafeMap} timesStore
 * @param {string} traceCategory
 * @param {string} implementation
 * @param {LogImpl} logImpl
 * @param {string} label
 * @param {any[]} args
 * @returns {void}
 */
function timeLog(timesStore, traceCategory, implementation, logImpl, label = 'default', args) {
  // Coerces everything other than Symbol to a string
  label = `${label}`;
  timeLogImpl(timesStore, implementation, logImpl, label, args);

  trace(kTraceInstant, traceCategory, `time::${label}`, 0);
}

module.exports = {
  time,
  timeLog,
  timeEnd,
  formatTime,
};
