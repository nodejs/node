'use strict';

const assert = require('assert');
const fixtures = require('./fixtures');
const path = require('path');

const probeTargetExitSignal = 'SIGSEGV';
const probeTargetExitMessage =
  `Target exited with signal ${probeTargetExitSignal} before target completion`;

function debuggerFixturePath(name) {
  return path.relative(process.cwd(), fixtures.path('debugger', name));
}

// Work around a pre-existing inspector issue: if the debuggee exits too quickly
// the inspector can segfault while tearing down. For now normalize the
// trailing segfault as completion until the upstream bug is fixed.
// See https://github.com/nodejs/node/issues/62765
// https://github.com/nodejs/node/issues/58245
function assertProbeJson(output, expected) {
  const normalized = JSON.parse(output);
  const lastResult = normalized.results?.[normalized.results.length - 1];

  if (lastResult?.event === 'error' &&
    lastResult.error?.code === 'probe_target_exit' &&
    lastResult.error?.signal === probeTargetExitSignal &&
    lastResult.error?.message === probeTargetExitMessage) {
    // Log to facilitate debugging if this normalization is occurring.
    console.log('Normalizing trailing SIGSEGV in JSON probe output');
    normalized.results[normalized.results.length - 1] = { event: 'completed' };
  }

  assert.deepStrictEqual(normalized, expected);
}

function assertProbeText(output, expected) {
  const idx = output.indexOf(probeTargetExitMessage);
  let normalized;
  if (idx !== -1) {
    // Log to facilitate debugging if this normalization is occurring.
    console.log('Normalizing trailing SIGSEGV in text probe output');
    normalized = output.slice(0, output.lastIndexOf('\n', idx)) + '\nCompleted';
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
