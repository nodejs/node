// Flags: --expose-gc
'use strict';
require('../common');
const { GCProfiler } = require('v8');

// Test start and stop more than once
{
  const profiler = new GCProfiler();
  profiler.start();
  profiler.stop();
  profiler.start();
  profiler.stop();
}

// Test free the GCProfiler memory when GC
{
  const profiler = new GCProfiler();
  profiler.start();
  profiler.stop();
}
global?.gc();

// Test free the GCProfiler memory when the Environment exits
{
  const profiler = new GCProfiler();
  profiler.start();
  profiler.stop();
}
