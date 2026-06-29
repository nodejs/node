// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test normalization.
 */

'use strict';

const { ClosureRemover } = require('../mutators/closure_remover.js');
const helpers = require('./helpers.js');
const normalizer = require('../mutators/normalizer.js');
const scriptMutator = require('../script_mutator.js');
const sourceHelpers = require('../source_helpers.js');

describe('Normalize', () => {
  it('test basic', () => {
    const source = helpers.loadTestData('normalize.js');

    const mutator = new normalizer.IdentifierNormalizer();
    mutator.mutate(source);

    const normalized_0 = sourceHelpers.generateCode(source);
    helpers.assertExpectedResult(
        'normalize_expected_0.js', normalized_0);

    mutator.mutate(source);
    const normalized_1 = sourceHelpers.generateCode(source);
    helpers.assertExpectedResult(
        'normalize_expected_1.js', normalized_1);
  });

  it('test simple_test.js', () => {
    const source = helpers.loadTestData('simple_test.js');

    const mutator = new normalizer.IdentifierNormalizer();
    mutator.mutate(source);

    const normalized = sourceHelpers.generateCode(source);
    helpers.assertExpectedResult(
        'simple_test_expected.js', normalized);
  });

  it('test already normalized variables', () => {
    const source = helpers.loadTestData('normalize_fuzz_test.js');

    const mutator = new normalizer.IdentifierNormalizer();
    mutator.mutate(source);

    const normalized = sourceHelpers.generateCode(source);
    helpers.assertExpectedResult(
        'normalize_fuzz_test_expected.js', normalized);
  });

  it('test already normalized functions', () => {
    const source = helpers.loadTestData('normalize_fuzz_test_functions.js');

    const mutator = new normalizer.IdentifierNormalizer();
    mutator.mutate(source);

    const normalized = sourceHelpers.generateCode(source);
    helpers.assertExpectedResult(
        'normalize_fuzz_test_functions_expected.js', normalized);
  });

  it('test closure transformations', () => {
    const source = helpers.loadTestData('closures.js');

    const mutator = new normalizer.IdentifierNormalizer();
    mutator.mutate(source);

    // Ensure we activate closure transformations.
    const settings = scriptMutator.defaultSettings();
    settings['TRANSFORM_CLOSURES'] = 1.0;

    const closures = new ClosureRemover(settings);
    closures.mutate(source);

    helpers.assertExpectedResult(
        'closures_expected.js', sourceHelpers.generateCode(source));
  });
});
