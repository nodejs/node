// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test normalization.
 */

'use strict';

const sinon = require('sinon');

const common = require('../mutators/common.js');
const helpers = require('./helpers.js');
const random = require('../random.js');
const scriptMutator = require('../script_mutator.js');
const sourceHelpers = require('../source_helpers.js');
const tryCatch = require('../mutators/try_catch.js');

const sandbox = sinon.createSandbox();

function loadSource() {
  return helpers.loadTestData('try_catch.js');
}

function testTryCatch(source, expected) {
  const mutator = new tryCatch.AddTryCatchMutator();
  mutator.mutate(source);

  const mutated = sourceHelpers.generateCode(source);
  helpers.assertExpectedResult(expected, mutated);
}

describe('Try catch', () => {
  afterEach(() => {
    sandbox.restore();
  });

  // Wrap on exit, hence wrap everything nested.
  it('wraps all', () => {
    sandbox.stub(random, 'choose').callsFake(() => { return false; });
    sandbox.stub(random, 'random').callsFake(() => { return 0.7; });
    testTryCatch(loadSource(), 'try_catch_expected.js');
  });

  // Wrap on enter and skip.
  it('wraps toplevel', () => {
    sandbox.stub(random, 'choose').callsFake(() => { return false; });
    sandbox.stub(random, 'random').callsFake(() => { return 0.04; });
    const source = loadSource();

    // Fake source fraction 0.1 (i.e. the second of 10 files).
    // Probability for toplevel try-catch is 0.05.
    common.setSourceLoc(source, 1, 10);

    testTryCatch(source, 'try_catch_toplevel_expected.js');
  });

  // Choose the rare case of skipping try-catch.
  it('wraps nothing', () => {
    sandbox.stub(random, 'choose').callsFake(() => { return false; });
    sandbox.stub(random, 'random').callsFake(() => { return 0.01; });
    const source = loadSource();

    // Fake source fraction 0.2 (i.e. the third of 10 files).
    // Probability for skipping is 0.02.
    common.setSourceLoc(source, 2, 10);

    testTryCatch(source, 'try_catch_nothing_expected.js');
  });

  // Choose to alter the target probability to 0.9 resulting in skipping
  // all try-catch.
  it('wraps nothing with high target probability', () => {
    sandbox.stub(random, 'choose').callsFake(() => { return true; });
    sandbox.stub(random, 'uniform').callsFake(() => { return 0.9; });
    sandbox.stub(random, 'random').callsFake(() => { return 0.8; });
    const source = loadSource();

    // Fake source fraction 0.9 (i.e. the last of 10 files).
    // Probability for skipping is 0.81 (0.9 * 0.9).
    common.setSourceLoc(source, 9, 10);

    testTryCatch(source, 'try_catch_alternate_expected.js');
  });

  // General distribution with pseudo-random execution.
  it('distributes', () => {
    helpers.deterministicRandom(sandbox);

    sandbox.stub(sourceHelpers, 'loadResource').callsFake(() => {
      return helpers.loadTestData('differential_fuzz/fake_resource.js');
    });

    // Zero settings for all mutators (try-catch always runs).
    const settings = scriptMutator.defaultSettings();
    for (const key of Object.keys(settings)) {
      settings[key] = 0.0;
    }

    const inputs = [
        'try_catch_distribution.js',
        'try_catch_distribution.js',
        'try_catch_distribution.js',
        'try_catch_distribution.js',
        'try_catch_distribution.js',
        'try_catch_distribution.js',
    ];

    const sources = inputs.map(input => helpers.loadTestData(input));
    const mutator = new scriptMutator.ScriptMutator(settings, helpers.DB_DIR);
    const mutated = mutator.mutateMultiple(sources);

    helpers.assertExpectedResult(
        'try_catch_distribution_expected.js', mutated.code);
  });
});
