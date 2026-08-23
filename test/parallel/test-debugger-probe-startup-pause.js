// Flags: --expose-internals
// This tests that probe mode ignores pauses until startup has finished binding
// breakpoints.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const assert = require('assert');
const { ProbeInspectorSession } = require('internal/debugger/inspect_probe');

const session = new ProbeInspectorSession({
  probes: [],
});

const cdpCalls = [];
session.client = {
  callMethod: common.mustCall(async (method) => {
    cdpCalls.push(method);
  }),
};

async function testStartupPauseHandling() {
  await session.handlePaused({});
  assert.deepStrictEqual(cdpCalls, []);

  session.started = true;
  await session.handlePaused({});
  assert.deepStrictEqual(cdpCalls, ['Debugger.resume']);
}

testStartupPauseHandling().then(common.mustCall());
