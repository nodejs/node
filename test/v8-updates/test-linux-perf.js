'use strict';

// This test verifies that JavaScript functions are being correctly sampled by
// Linux perf. The test runs a JavaScript script, sampling the execution with
// Linux perf. It then uses `perf script` to generate a human-readable output,
// and uses regular expressions to find samples of the functions defined in
// `fixtures/linux-perf.js`.

// NOTE (mmarchini): this test is meant to run only on Linux machines with Linux
// perf installed. It will skip if those criteria are not met.

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { spawnSync } = require('child_process');
const fixtures = require('../common/fixtures');
const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

if (process.config.variables.node_shared)
  common.skip("can't test Linux perf with shared libraries yet");

const perfArgs = [
  'record',
  '-F500',
  '-g',
  '--',
  process.execPath,
  '--perf-basic-prof',
  '--interpreted-frames-native-stack',
  '--no-turbo-inlining',  // Otherwise simple functions might get inlined.
  fixtures.path('linux-perf.js'),
];

const perfScriptArgs = [
  'script',
];

const options = {
  cwd: tmpdir.path,
  encoding: 'utf-8',
};

if (!common.isLinux)
  common.skip('only testing Linux for now');

const perf = spawnSync('perf', perfArgs, options);

if (perf.error && perf.error.errno === 'ENOENT')
  common.skip('perf not found on system');

if (perf.status !== 0) {
  common.skip(`Failed to execute perf: ${perf.stderr}`);
}

const perfScript = spawnSync('perf', perfScriptArgs, options);

if (perf.error)
  common.skip(`perf script aborted: ${perf.error.errno}`);

if (perfScript.status !== 0) {
  common.skip(`Failed to execute perf script: ${perfScript.stderr}`);
}

const interpretedFunctionOneRe = /InterpretedFunction:functionOne/;
const compiledFunctionOneRe = /LazyCompile:\*functionOne/;
const interpretedFunctionTwoRe = /InterpretedFunction:functionTwo/;
const compiledFunctionTwoRe = /LazyCompile:\*functionTwo/;

const output = perfScript.stdout;

assert.ok(output.match(interpretedFunctionOneRe),
          "Couldn't find interpreted functionOne()");
assert.ok(output.match(compiledFunctionOneRe),
          "Couldn't find compiled functionOne()");
assert.ok(output.match(interpretedFunctionTwoRe),
          "Couldn't find interpreted functionTwo()");
assert.ok(output.match(compiledFunctionTwoRe),
          "Couldn't find compiled functionTwo");
