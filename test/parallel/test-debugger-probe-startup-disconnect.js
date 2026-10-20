// Flags: --expose-internals
// This tests that a disconnect while probe mode is waiting for target startup
// is reported as a structured probe failure instead of an internal error.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const assert = require('assert');
const { EventEmitter } = require('events');
const { assertProbeJson } = require('../common/debugger-probe');
const { ProbeInspectorSession } = require('internal/debugger/inspect_probe');

const probe = {
  expr: 'value',
  target: { suffix: 'probe-target.js', line: 1 },
};
const client = new EventEmitter();
client.connect = common.mustCall();
client.callMethod = common.mustCall((method) => {
  assert.strictEqual(method, 'NodeRuntime.enable');
  setImmediate(() => client.emit('close'));
  return new Promise(() => {});
});
client.reset = common.mustCall();

const session = new ProbeInspectorSession({
  childArgv: ['-e', ''],
  host: '127.0.0.1',
  port: 0,
  probes: [probe],
  skipPortPreflight: true,
});
session.client = client;

session.run().then(common.mustCall(({ code, report }) => {
  assert.strictEqual(code, 1);
  assertProbeJson(report, {
    v: 2,
    probes: [probe],
    results: [{
      event: 'error',
      pending: [0],
      error: {
        code: 'probe_failure',
        message:
          'Inspector connection lost before probes started before probes: ' +
          'probe-target.js:1. The target startup may have torn down the ' +
          'inspector. If startup does not touch the inspector, this is likely ' +
          'a Node.js bug. Please file an issue.',
        stderr: '',
        details: { lastCdpMethod: 'NodeRuntime.enable' },
      },
    }],
  });
}));
