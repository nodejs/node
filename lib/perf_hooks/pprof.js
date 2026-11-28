'use strict';

// Public entry point for the pprof profiler.
// Usage: require('perf_hooks/pprof') or require('node:perf_hooks/pprof')

const {
  ObjectFreeze,
} = primordials;

const { emitExperimentalWarning } = require('internal/util');
emitExperimentalWarning('perf_hooks/pprof');

const {
  constants,
  isStarted,
  profile,
  start,
  stop,
  v8ProfilerStuckEventLoopDetected,
} = require('internal/perf/pprof/time_profiler');
const {
  encode,
  encodeSync,
} = require('internal/perf/pprof/encoder');

const time = ObjectFreeze({
  constants,
  isStarted,
  profile,
  start,
  stop,
  v8ProfilerStuckEventLoopDetected,
});

module.exports = {
  encode,
  encodeSync,
  time,
};
