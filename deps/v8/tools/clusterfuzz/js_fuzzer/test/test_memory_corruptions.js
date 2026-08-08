// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test memory-corruption mutations.
 */

'use strict';

const sinon = require('sinon');

const helpers = require('./helpers.js');
const sourceHelpers = require('../source_helpers.js');
const memory = require('../mutators/memory_corruption_mutator.js');

const sandbox = sinon.createSandbox();

describe('Mutate memory', () => {
  afterEach(() => {
    sandbox.restore();
  });

  it('corrupts objects', () => {
    // Make random operations deterministic.
    helpers.deterministicRandom(sandbox);

    const settings = helpers.zeroSettings();
    settings.CORRUPT_MEMORY = 1.0;

    const source = helpers.loadTestData('memory_corruption/input.js');
    const mutator = new memory.MemoryCorruptionMutator(settings);
    mutator.mutate(source);
    const code = sourceHelpers.generateCode(source);

    helpers.assertExpectedResult(
        'memory_corruption/output_expected.js', code);
  });
});
