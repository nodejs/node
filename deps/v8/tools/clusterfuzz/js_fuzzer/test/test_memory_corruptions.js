// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test memory-corruption mutations.
 */

'use strict';

const assert = require('assert');
const sinon = require('sinon');

const helpers = require('./helpers.js');
const sourceHelpers = require('../source_helpers.js');
const scriptMutator = require('../script_mutator.js');
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

  it('adds --memory-corruption-via-watchpoints flag and uses markForCorruptionOnAccess', () => {
    // Make random operations deterministic.
    helpers.deterministicRandom(sandbox);

    const settings = helpers.zeroSettings();
    settings.is_sandbox_fuzzing = true;
    settings.is_x64_linux = true;
    settings.CORRUPT_MEMORY = 1.0;

    settings.CORRUPT_MEMORY_VIA_WATCHPOINTS = 0.5;
    const source = helpers.loadTestData('memory_corruption/input.js');
    const mutator = new scriptMutator.ScriptMutator(settings, helpers.DB_DIR);
    const result = mutator.mutateMultiple([source]);
    assert.ok(result.flags.includes('--memory-corruption-via-watchpoints'));

    const code = sourceHelpers.generateCode(source);
    helpers.assertExpectedResult(
        'memory_corruption/watchpoints_expected.js', code);
  });

  it('does not use markForCorruptionOnAccess when is_x64_linux is false', () => {
    const settings = helpers.zeroSettings();
    settings.CORRUPT_MEMORY = 1.0;
    settings.is_x64_linux = false;
    settings.CORRUPT_MEMORY_VIA_WATCHPOINTS = 1.0;

    const source = helpers.loadTestData('memory_corruption/input.js');
    const mutator = new memory.MemoryCorruptionMutator(settings);
    mutator.mutate(source);
    const code = sourceHelpers.generateCode(source);

    // The output should NOT contain markForCorruptionOnAccess
    assert.ok(!code.includes('markForCorruptionOnAccess'));
  });
});
