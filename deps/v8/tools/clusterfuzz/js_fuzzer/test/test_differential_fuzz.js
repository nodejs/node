// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for differential fuzzing.
 */

'use strict';

const assert = require('assert');
const path = require('path');
const program = require('commander');
const sinon = require('sinon');

const corpus = require('../corpus.js');
const helpers = require('./helpers.js');
const scriptMutator = require('../script_mutator.js');
const sourceHelpers = require('../source_helpers.js');
const random = require('../random.js');
const runner = require('../runner.js');

const { DifferentialFuzzMutator, DifferentialFuzzSuppressions } = require(
    '../mutators/differential_fuzz_mutator.js');
const { DifferentialScriptMutator, FuzzilliDifferentialScriptMutator } = require(
    '../differential_script_mutator.js');

const sandbox = sinon.createSandbox();

function testMutators(settings, mutatorClass, inputFile, expectedFile) {
  const source = helpers.loadTestData('differential_fuzz/' + inputFile);

  const mutator = new mutatorClass(settings);
  mutator.mutate(source);

  const mutated = sourceHelpers.generateCode(source);
  helpers.assertExpectedResult(
      'differential_fuzz/' + expectedFile, mutated);
}

describe('Differential fuzzing', () => {
  beforeEach(() => {
    // By default, deterministically use all mutations of differential
    // fuzzing.
    this.settings = helpers.zeroSettings();
    this.settings['DIFF_FUZZ_EXTRA_PRINT'] = 1.0;
    this.settings['DIFF_FUZZ_TRACK_CAUGHT'] = 1.0;
    this.settings['DIFF_FUZZ_BLOCK_PRINT'] = 0.0;
    this.settings['DIFF_FUZZ_SKIP_FUNCTIONS'] = 0.0;

    // Fake fuzzer being called with --input_dir flag.
    this.oldInputDir = program.input_dir;
    program.input_dir = helpers.BASE_DIR;

    // Make remaining places deterministic that don't use global settings, e.g.
    // try-catch skip behavior.
    helpers.deterministicRandom(sandbox);

    sandbox.stub(corpus, 'TRANSPILE_PROB').value(0.0);
  });

  afterEach(() => {
    sandbox.restore();
    program.input_dir = this.oldInputDir;
  });

  it('applies suppressions', () => {
    // This selects the first random variable when replacing .arguments.
    sandbox.stub(random, 'single').callsFake(a => a[0]);
    testMutators(
        this.settings,
        DifferentialFuzzSuppressions,
        'suppressions.js',
        'suppressions_expected.js');
  });

  it('adds extra printing', () => {
    testMutators(
        this.settings,
        DifferentialFuzzMutator,
        'mutations.js',
        'mutations_expected.js');
  });

  it('adds block printing', () => {
    this.settings['DIFF_FUZZ_EXTRA_PRINT'] = 0.0;
    this.settings['DIFF_FUZZ_BLOCK_PRINT'] = 1.0;
    testMutators(
        this.settings,
        DifferentialFuzzMutator,
        'mutations.js',
        'mutations_block_expected.js');
  });

  it('skips functions', () => {
    this.settings['DIFF_FUZZ_SKIP_FUNCTIONS'] = 1.0;
    testMutators(
        this.settings,
        DifferentialFuzzMutator,
        'mutations.js',
        'mutations_skip_fun_expected.js');
  });

  it('does no extra printing', () => {
    this.settings['DIFF_FUZZ_EXTRA_PRINT'] = 0.0;
    testMutators(
        this.settings,
        DifferentialFuzzMutator,
        'exceptions.js',
        'exceptions_expected.js');
  });

  it('runs end to end', () => {
    // Don't choose any zeroed settings or IGNORE_DEFAULT_PROB in try-catch
    // mutator. Choose using original flags with >= 2%.
    const chooseOrigFlagsProb = 0.2;
    sandbox.stub(random, 'choose').callsFake((p) => p >= chooseOrigFlagsProb);

    // Fake build directory from which two json configurations for flags are
    // loaded.
    const env = {
      APP_DIR: 'test_data/differential_fuzz',
      GENERATE: process.env.GENERATE,
    };
    sandbox.stub(process, 'env').value(env);

    // Fake loading resources and instead load one fixed fake file for each.
    sandbox.stub(sourceHelpers, 'loadResource').callsFake(() => {
      return helpers.loadTestData('differential_fuzz/fake_resource.js');
    });

    // Load input files.
    const files = [
      'v8/differential_fuzz/input1.js',
      'v8/differential_fuzz/input2.js',
    ];
    const sources = files.map(helpers.loadV8TestData);

    // Apply top-level fuzzing, with all probabilistic configs switched off.
    this.settings['DIFF_FUZZ_EXTRA_PRINT'] = 0.0;
    this.settings['DIFF_FUZZ_TRACK_CAUGHT'] = 0.0;
    const mutator = new DifferentialScriptMutator(
        this.settings, helpers.DB_DIR);
    const mutated = mutator.mutateMultiple(sources);
    helpers.assertExpectedResult(
        'differential_fuzz/combined_expected.js', mutated.code);

    // Flags for v8_foozzie.py are calculated from v8_fuzz_experiments.json and
    // v8_fuzz_flags.json in test_data/differential_fuzz.
    const expectedFlags = [
      '--first-config=ignition',
      '--second-config=ignition_turbo',
      '--second-d8=d8',
      '--second-config-extra-flags=--foo1',
      '--second-config-extra-flags=--foo2',
      '--first-config-extra-flags=--flag1',
      '--second-config-extra-flags=--flag1',
      '--first-config-extra-flags=--flag2',
      '--second-config-extra-flags=--flag2',
      '--first-config-extra-flags=--flag3',
      '--second-config-extra-flags=--flag3',
      '--first-config-extra-flags=--flag4',
      '--second-config-extra-flags=--flag4'
    ];
    assert.deepEqual(expectedFlags, mutated.flags);
  });

  it('runs end to end with fuzzilli inputs', () => {
    // Similar to the test above but using a Fuzzilli input as well.
    const chooseOrigFlagsProb = 0.2;
    sandbox.stub(random, 'choose').callsFake((p) => p >= chooseOrigFlagsProb);

    const env = {
      APP_DIR: 'test_data/differential_fuzz',
      GENERATE: process.env.GENERATE,
    };
    sandbox.stub(process, 'env').value(env);

    sandbox.stub(sourceHelpers, 'loadResource').callsFake(() => {
      return helpers.loadTestData('differential_fuzz/fake_resource.js');
    });

    const sources = [
      helpers.loadV8TestData('v8/differential_fuzz/input2.js'),
      helpers.loadFuzzilliTestData('fuzzilli/fuzzdir-1/corpus/program_1.js'),
    ];

    this.settings['DIFF_FUZZ_EXTRA_PRINT'] = 0.0;
    this.settings['DIFF_FUZZ_TRACK_CAUGHT'] = 0.0;
    const mutator = new DifferentialScriptMutator(
        this.settings, helpers.DB_DIR);
    const mutated = mutator.mutateMultiple(sources);
    helpers.assertExpectedResult(
        'differential_fuzz/fuzzilli_combined_expected.js', mutated.code);

    const expectedFlags = [
      // Flags from v8_fuzz_experiments.json.
      '--first-config=ignition',
      '--second-config=ignition_turbo',
      '--second-d8=d8',
      // Flags from v8_fuzz_flags.json.
      '--second-config-extra-flags=--foo1',
      '--second-config-extra-flags=--foo2',
      // Flags from the mjsunit input.
      '--first-config-extra-flags=--flag4',
      '--second-config-extra-flags=--flag4',
      // Flags from the fuzzilli input.
      '--first-config-extra-flags=--fuzzilli-flag1',
      '--second-config-extra-flags=--fuzzilli-flag1',
      '--first-config-extra-flags=--fuzzilli-flag2',
      '--second-config-extra-flags=--fuzzilli-flag2',
    ];
    assert.deepEqual(expectedFlags, mutated.flags);
  });

  it('chooses fuzzilli runner', () => {
    sandbox.stub(random, 'randInt').callsFake(() => 2);

    const env = {
      APP_DIR: 'test_data/differential_fuzz',
      GENERATE: process.env.GENERATE,
    };
    sandbox.stub(process, 'env').value(env);

    const mutator = new DifferentialScriptMutator(
        this.settings, helpers.DB_DIR);

    assert.deepEqual(
        runner.RandomCorpusRunnerWithFuzzilli, mutator.runnerClass);
  });
});

// As above, but we don't zero the settings as we use zero settings in
// production here.
describe('Differential fuzzing with fuzzilli', () => {
  beforeEach(() => {
    // Fake fuzzer being called with --input_dir flag.
    this.oldInputDir = program.input_dir;
    program.input_dir = path.join(
        helpers.BASE_DIR, 'differential_fuzz_fuzzilli');

    helpers.deterministicRandom(sandbox);

    sandbox.stub(corpus, 'TRANSPILE_PROB').value(0.0);
  });

  afterEach(() => {
    sandbox.restore();
    program.input_dir = this.oldInputDir;
  });

  it('runs end to end', () => {
    const env = {
      APP_DIR: 'test_data/differential_fuzz',
      GENERATE: process.env.GENERATE,
    };
    sandbox.stub(process, 'env').value(env);

    sandbox.stub(sourceHelpers, 'loadResource').callsFake(() => {
      return helpers.loadTestData('differential_fuzz/fake_resource.js');
    });

    const mutator = new FuzzilliDifferentialScriptMutator(
        scriptMutator.defaultSettings(), helpers.DB_DIR);

    // Configure 3 output tests and V8 engine.
    const settings = {
      input_dir: program.input_dir,
      diff_fuzz: true,
      engine: 'v8',
      no_of_files: 3,
    };
    const testRunner = new mutator.runnerClass(settings);
    const inputSets = Array.from(testRunner.enumerateInputs());
    assert.equal(3, inputSets.length);
    for (const [i, inputs] of inputSets) {
      const mutated = mutator.mutateMultiple(inputs);
      helpers.assertExpectedResult(
          `differential_fuzz_fuzzilli/expected_code_${i}.js`, mutated.code);
      helpers.assertExpectedResult(
          `differential_fuzz_fuzzilli/expected_flags_${i}.js`,
          JSON.stringify(mutated.flags, null, 2));
    }
  });
});
