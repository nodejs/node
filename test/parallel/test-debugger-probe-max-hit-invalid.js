// This tests that probe mode rejects malformed --max-hit usage.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const { assertProbeCliError } = require('../common/debugger-probe');

const cwd = fixtures.path('debugger');

// --max-hit before any --probe.
assertProbeCliError(
  ['--max-hit', '1', '--probe', 'probe.js:12', '--expr', 'finalValue', 'probe.js'],
  /Unexpected --max-hit before --probe/, { cwd });

// --max-hit between a --probe and its --expr.
assertProbeCliError(
  ['--probe', 'probe.js:12', '--max-hit', '1', '--expr', 'finalValue', 'probe.js'],
  /Each --probe must be followed immediately by --expr/, { cwd });

// Duplicate --max-hit for a single probe.
assertProbeCliError(
  ['--probe', 'probe.js:12', '--expr', 'finalValue', '--max-hit', '1', '--max-hit', '2', 'probe.js'],
  /Duplicate --max-hit for a single --probe/, { cwd });

// Non-numeric value.
assertProbeCliError(
  ['--probe', 'probe.js:12', '--expr', 'finalValue', '--max-hit', 'abc', 'probe.js'],
  /Invalid max-hit: abc/, { cwd });

// Zero is not allowed (limit must be at least 1).
assertProbeCliError(
  ['--probe', 'probe.js:12', '--expr', 'finalValue', '--max-hit', '0', 'probe.js'],
  /Invalid max-hit: 0/, { cwd });

// Missing value: --max-hit as the final token has nothing to consume.
assertProbeCliError(
  ['--probe', 'probe.js:12', '--expr', 'finalValue', '--max-hit'],
  /Missing value for --max-hit/, { cwd });
