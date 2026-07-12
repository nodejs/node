'use strict';

const { getCategoryEnabledBuffer, trace, usePerfetto } = internalBinding('trace_events');
const {
  CHAR_UPPERCASE_B,
  CHAR_LOWERCASE_B,
  CHAR_UPPERCASE_C,
  CHAR_LOWERCASE_E,
  CHAR_UPPERCASE_E,
  CHAR_LOWERCASE_N,
} = require('internal/constants');

let nodeTraceEventCategory;
if (usePerfetto) {
  nodeTraceEventCategory = (category) => `${category}`;
} else {
  nodeTraceEventCategory = (category) => `node,${category}`;
}

// The async events describe the execution of a single asynchronous operation, and are
// used to measure the time spent in a single asynchronous operation.
// Async events may overlap with each other. Different events do not have
// to be nested, or FILO (first in last out).
// TODO(legendecas): V8 `trace` API does not support async marks in perfetto yet.
const kAsyncBegin = usePerfetto ? CHAR_UPPERCASE_B : CHAR_LOWERCASE_B;
const kAsyncEnd = usePerfetto ? CHAR_UPPERCASE_E : CHAR_LOWERCASE_E;

// The sync events describe the execution of a single thread, and are
// used to measure the time spent in a function.
// Sync events must be nested, and are FILO (first in last out), in a stack
// manner.
const kSyncBegin = CHAR_UPPERCASE_B;
const kSyncEnd = CHAR_UPPERCASE_E;

// Counter events track a named numeric value as it changes over time. Each
// event records the value at a point in time, and the trace viewer renders the
// series as a graph.
// TODO(legendecas): V8 `trace` API does not support count marks in perfetto yet.
const kTraceCount = usePerfetto ? CHAR_LOWERCASE_N : CHAR_UPPERCASE_C;

// Instant events mark a single moment in time. They have no duration and do
// not need to be paired or nested.
const kTraceInstant = CHAR_LOWERCASE_N;

module.exports = {
  usePerfetto,
  getCategoryEnabledBuffer,
  trace,
  nodeTraceEventCategory,
  kAsyncBegin,
  kAsyncEnd,
  kSyncBegin,
  kSyncEnd,
  kTraceCount,
  kTraceInstant,
};
