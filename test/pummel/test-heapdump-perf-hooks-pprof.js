'use strict';
// This tests heap snapshot integration of the WallProfiler of
// node:perf_hooks/pprof.

const common = require('../common');
const { validateByRetainingPath } = require('../common/heap');
const assert = require('node:assert');
const { time } = require('node:perf_hooks/pprof');

// Before the time profiler is started, no WallProfiler should be created.
{
  const nodes = validateByRetainingPath('Node / WallProfiler', []);
  assert.strictEqual(nodes.length, 0);
}

time.start();

// While the time profiler is started, a WallProfiler should exist, without
// records since CPU time is not collected.
{
  validateByRetainingPath('Node / WallProfiler', [
    { node_name: 'WallProfiler', edge_name: 'native_to_javascript' },
  ]);
  const withRecords = validateByRetainingPath('Node / WallProfiler', [
    { node_name: 'Node / records', edge_name: 'records' },
  ], true);
  assert.strictEqual(withRecords.length, 0);
}

time.stop();

// CPU time is attributed to samples with records, which are allocated upfront.
if (!common.isWindows) {
  time.start({ collectCpuTime: true });
  validateByRetainingPath('Node / WallProfiler', [
    { node_name: 'Node / records', edge_name: 'records' },
  ]);
  time.stop();
}
