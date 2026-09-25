'use strict';

// The embedtest binary runs on its own MultiIsolatePlatform without Node's
// tracing agent, so node:trace_events must report itself as unavailable.

const common = require('../common');
const { spawnSyncAndAssert } = require('../common/child_process');

spawnSyncAndAssert(
  common.resolveBuiltBinary('embedtest'),
  [
    'try { require("node:trace_events").createTracing({ categories: ["v8"] }); }' +
    'catch (e) { console.log(e.code); }',
  ],
  {
    trim: true,
    stdout: 'ERR_TRACE_EVENTS_UNAVAILABLE',
  });
