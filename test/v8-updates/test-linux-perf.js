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

if (!common.isLinux)
  common.skip('only testing Linux for now');

const frequency = 99;

const repeat = 5;

// Expected number of samples we'll capture per repeat
const sampleCount = 10;
const sleepTime = sampleCount * (1.0 / frequency);

const perfFlags = [
  'record',
  `-F${frequency}`,
  '-g',
];

const nodeCommonFlags = [
  '--perf-basic-prof',
  '--interpreted-frames-native-stack',
  '--no-turbo-inlining',  // Otherwise simple functions might get inlined.
];

const perfInterpretedFramesArgs = [
  ...perfFlags,
  '--',
  process.execPath,
  ...nodeCommonFlags,
  '--no-opt',
  fixtures.path('linux-perf.js'),
  `${sleepTime}`,
  `${repeat}`,
];

const perfCompiledFramesArgs = [
  ...perfFlags,
  '--',
  process.execPath,
  ...nodeCommonFlags,
  '--always-opt',
  fixtures.path('linux-perf.js'),
  `${sleepTime}`,
  `${repeat}`,
];

const perfArgsList = [
  perfInterpretedFramesArgs, perfCompiledFramesArgs
];

const perfScriptArgs = [
  'script',
];

const options = {
  cwd: tmpdir.path,
  encoding: 'utf-8',
};

let output = '';

for (const perfArgs of perfArgsList) {
  const perf = spawnSync('perf', perfArgs, options);
  assert.ifError(perf.error);
  if (perf.status !== 0)
    throw new Error(`Failed to execute 'perf': ${perf.stderr}`);

  const perfScript = spawnSync('perf', perfScriptArgs, options);
  assert.ifError(perfScript.error);
  if (perfScript.status !== 0)
    throw new Error(`Failed to execute perf script: ${perfScript.stderr}`);

  output += perfScript.stdout;
}

const interpretedFunctionOneRe = /InterpretedFunction:functionOne/;
const compiledFunctionOneRe = /LazyCompile:\*functionOne/;
const interpretedFunctionTwoRe = /InterpretedFunction:functionTwo/;
const compiledFunctionTwoRe = /LazyCompile:\*functionTwo/;


assert.ok(output.match(interpretedFunctionOneRe),
          "Couldn't find interpreted functionOne()");
assert.ok(output.match(compiledFunctionOneRe),
          "Couldn't find compiled functionOne()");
assert.ok(output.match(interpretedFunctionTwoRe),
          "Couldn't find interpreted functionTwo()");
assert.ok(output.match(compiledFunctionTwoRe),
          "Couldn't find compiled functionTwo");
