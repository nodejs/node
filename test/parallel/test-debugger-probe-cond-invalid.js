// This tests that probe mode rejects malformed --cond usage.
'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const fixtures = require('../common/fixtures');
const { assertProbeCliError } = require('../common/debugger-probe');

const cwd = fixtures.path('debugger');

assertProbeCliError(
  ['--cond', 'x', '--probe', 'probe.js:12', '--expr', 'finalValue', 'probe.js'],
  /Unexpected --cond before --probe/, { cwd });

assertProbeCliError(
  ['--probe', 'probe.js:12', '--cond', 'x', '--expr', 'finalValue', 'probe.js'],
  /Each --probe must be followed immediately by --expr/, { cwd });

assertProbeCliError(
  ['--probe', 'probe.js:12', '--expr', 'finalValue', '--cond', 'x', '--cond', 'y', 'probe.js'],
  /A --probe can have at most one --cond/, { cwd });

assertProbeCliError(
  ['--probe', 'probe.js:12', '--expr', 'finalValue', '--cond', '   ', 'probe.js'],
  /Missing value for --cond/, { cwd });

assertProbeCliError(
  ['--probe', 'probe.js:12', '--expr', 'finalValue', '--cond'],
  /Missing value for --cond/, { cwd });

assertProbeCliError(
  ['--probe', 'probe.js:12', '--expr', 'a', '--cond', 'x',
   '--probe', 'probe.js:12', '--expr', 'b', '--cond', 'y', 'probe.js'],
  /Probes at probe\.js:12 must use the same --cond \(or none\)/, { cwd });

assertProbeCliError(
  ['--probe', 'probe.js:12', '--expr', 'a', '--cond', 'x',
   '--probe', 'probe.js:12', '--expr', 'b', 'probe.js'],
  /Probes at probe\.js:12 must use the same --cond \(or none\)/, { cwd });
