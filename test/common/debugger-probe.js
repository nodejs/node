'use strict';

const assert = require('assert');
const fixtures = require('./fixtures');
const path = require('path');

function debuggerFixturePath(name) {
  return path.relative(process.cwd(), fixtures.path('debugger', name));
}

// Work around a pre-existing inspector issue: if the debuggee exits too quickly
// the inspector can segfault while tearing down. For now normalize the segfault
// back to the expected terminal event (e.g. "completed" or "miss")
// until the upstream bug is fixed.
// See https://github.com/nodejs/node/issues/62765
// https://github.com/nodejs/node/issues/58245
const probeTargetExitSignal = 'SIGSEGV';

function assertProbeJson(output, expected) {
  const normalized = JSON.parse(output);
  const lastResult = normalized.results?.[normalized.results.length - 1];

  if (lastResult?.event === 'error' &&
    lastResult.error?.code === 'probe_target_exit' &&
    lastResult.error?.signal === probeTargetExitSignal) {
    // Log to facilitate debugging if this normalization is occurring.
    console.log('Normalizing trailing SIGSEGV in JSON probe output');
    normalized.results[normalized.results.length - 1] = expected.results.at(-1);
  }

  assert.deepStrictEqual(normalized, expected);
}

function assertProbeText(output, expected) {
  const signalPrefix = `Target exited with signal ${probeTargetExitSignal}`;
  const idx = output.indexOf(signalPrefix);
  let normalized;
  if (idx !== -1) {
    // Log to facilitate debugging if this normalization is occurring.
    console.log('Normalizing trailing SIGSEGV in text probe output');
    const lineStart = output.lastIndexOf('\n', idx);
    normalized = (lineStart === -1 ? '' : output.slice(0, lineStart)) + '\nCompleted';
  } else {
    normalized = output;
  }
  assert.strictEqual(normalized, expected);
}

module.exports = {
  assertProbeJson,
  assertProbeText,
  missScript: debuggerFixturePath('probe-miss.js'),
  probeScript: debuggerFixturePath('probe.js'),
  throwScript: debuggerFixturePath('probe-throw.js'),
  probeTypesScript: debuggerFixturePath('probe-types.js'),
  timeoutScript: debuggerFixturePath('probe-timeout.js'),
};
