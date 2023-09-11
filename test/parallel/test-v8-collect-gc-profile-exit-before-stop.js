'use strict';
require('../common');
const { GCProfiler } = require('v8');

// Test if it makes the process crash.
{
  const profiler = new GCProfiler();
  profiler.start();
  profiler.stop();
  profiler.start();
  profiler.stop();
}
{
  const profiler = new GCProfiler();
  profiler.start();
  profiler.stop();
}
