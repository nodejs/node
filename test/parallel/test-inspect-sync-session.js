'use strict';

// This tests that inspector.SyncSession() works.

const common = require('../common');
common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const inspector = require('inspector');
const assert = require('assert');
const session = new inspector.SyncSession();
const { pathToFileURL } = require('url');

session.connect();

// Test Profiler
session.post('Profiler.enable');
session.post('Profiler.start');

// Test HeapProfiler
session.post('HeapProfiler.enable');
session.post('HeapProfiler.startSampling');

// Test Runtime
session.post('Runtime.enable');
let res = session.post('Runtime.evaluate', { expression: '1 + 2' });
assert.deepStrictEqual(res, {
  result: { type: 'number', value: 3, description: '3' }
});

// Test Debug
session.post('Debugger.enable');
{
  const scripts = new Map();
  session.on('Debugger.scriptParsed', common.mustCallAtLeast((info) => {
    scripts.set(info.params.url, info.params.scriptId);
  }));
  // Trigger Debugger.scriptParsed
  const filepath = fixtures.path('empty.js');
  const url = pathToFileURL(filepath);
  require(filepath);
  assert(scripts.has(url.href));
}

{
  session.post('Debugger.setSkipAllPauses', { skip: false });
  let callFrames;
  session.once('Debugger.paused', common.mustCall((obj) => {
    callFrames = obj.params.callFrames;
  }));
  session.post('Debugger.pause');
  assert.strictEqual(callFrames[0].url, 'inspector.js');
  assert.strictEqual(callFrames[0].this.className, 'SyncSession');
}

// Test Profiler
res = session.post('Profiler.stop');
assert(Array.isArray(res.profile.nodes));

// Test HeapProfiler
res = session.post('HeapProfiler.stopSampling');
assert(Array.isArray(res.profile.samples));

session.disconnect();
