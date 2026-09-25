// Flags: --expose-gc --minor-ms
'use strict';

require('../common');
const assert = require('assert');
const { GCProfiler } = require('v8');

const profiler = new GCProfiler();
profiler.start();
global.gc({ type: 'minor', execution: 'sync' });

const profile = profiler.stop();
assert.deepStrictEqual(
  profile.statistics.map(({ gcType }) => gcType),
  ['MinorMarkSweep'],
);
