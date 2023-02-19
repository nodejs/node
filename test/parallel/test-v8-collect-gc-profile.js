// Flags: --expose-gc
'use strict';
require('../common');
const assert = require('assert');
const { GCProfiler } = require('v8');
const { testGCProfiler, emitGC } = require('../common/v8');

assert.throws(() => {
  new GCProfiler({
    mode: 'invalid type',
  });
}, /ERR_INVALID_ARG_TYPE/);

assert.throws(() => {
  new GCProfiler({
    mode: 12345,
  });
}, /ERR_INVALID_ARG_VALUE/);

const options = [
  undefined,
  {
    mode: GCProfiler.GC_PROFILER_MODE.TIME,
    format: GCProfiler.GC_PROFILER_FORMAT.STRING,
  },
  {
    mode: GCProfiler.GC_PROFILER_MODE.TIME,
    format: GCProfiler.GC_PROFILER_FORMAT.OBJECT,
  },
  {
    mode: GCProfiler.GC_PROFILER_MODE.TIME,
    format: GCProfiler.GC_PROFILER_FORMAT.NONE,
  },
  {
    mode: GCProfiler.GC_PROFILER_MODE.ALL,
    format: GCProfiler.GC_PROFILER_FORMAT.STRING,
  },
  {
    mode: GCProfiler.GC_PROFILER_MODE.ALL,
    format: GCProfiler.GC_PROFILER_FORMAT.OBJECT,
  },
  {
    mode: GCProfiler.GC_PROFILER_MODE.ALL,
    format: GCProfiler.GC_PROFILER_FORMAT.NONE,
  },
];

options.forEach((option) => testGCProfiler(option));

// Test getTotalGCTime
{
  const profiler = new GCProfiler();
  assert.ok(profiler.getTotalGCTime() === undefined);
  profiler.start();
  emitGC();
  assert.ok(profiler.getTotalGCTime() >= 0);
  profiler.stop(GCProfiler.GC_PROFILER_FORMAT.NONE);
  assert.ok(profiler.getTotalGCTime() === undefined);
}

// Test stop
{
  const profiler = new GCProfiler();
  // Call stop before start
  profiler.stop();
  profiler.start();
  assert.throws(() => {
    profiler.stop('invalid type');
  }, /ERR_INVALID_ARG_TYPE/);
  assert.throws(() => {
    profiler.stop(12345);
  }, /ERR_INVALID_ARG_VALUE/);
  assert.ok(profiler.stop(GCProfiler.GC_PROFILER_FORMAT.NONE) === undefined);
  // Call stop after stop
  profiler.stop();
}
