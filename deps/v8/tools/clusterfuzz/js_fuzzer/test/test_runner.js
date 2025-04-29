// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test the runner implementation.
 */

'use strict';

const assert = require('assert');
const path = require('path');
const sinon = require('sinon');

const helpers = require('./helpers.js');
const runner = require('../runner.js');

const sandbox = sinon.createSandbox();

describe('Load tests', () => {
  afterEach(() => {
    sandbox.restore();
  });

  it('from test archive', () => {
    helpers.deterministicRandom(sandbox);
    const archivePath = path.join(helpers.BASE_DIR, 'input_archive');

    // Create 2 test cases with a maximum of 2 inputs per test.
    const testRunner = new runner.RandomCorpusRunner(archivePath, 'v8', 2, 2);
    var inputs = Array.from(testRunner.enumerateInputs());

    // Check the enumeration counter separately.
    assert.equal(2, inputs.length);
    assert.deepEqual([0, 1], inputs.map((x) => x[0]));

    assert.deepEqual(
        ["v8/test/mjsunit/wasm/regress-123.js",
         "v8/test/mjsunit/wasm/regress-456.js"],
        inputs[0][1].map((x) => x.relPath));
    assert.deepEqual(
        ["v8/test/mjsunit/v8_test.js", "chakra/chakra_test2.js"],
        inputs[1][1].map((x) => x.relPath));
  });

  it('from test corpus', () => {
    // Also need to fix random here as each corpus shuffles their test cases
    // once.
    sandbox.stub(Math, 'random').callsFake(() => 0.5);
    const archivePath = path.join(helpers.BASE_DIR, 'input_archive');

    // Select all tests from the chakra archive (has 2 in total). Also exercise
    // the extra strict parsing logic, which parses all files twice.
    const testRunner = new runner.SingleCorpusRunner(
        archivePath, 'chakra', true);
    var inputs = Array.from(testRunner.enumerateInputs());

    assert.equal(2, inputs.length);
    assert.deepEqual([0, 1], inputs.map((x) => x[0]));

    assert.deepEqual(
        ["chakra/chakra_test2.js"],
        inputs[0][1].map((x) => x.relPath));
    assert.deepEqual(
        ["chakra/chakra_test1.js"],
        inputs[1][1].map((x) => x.relPath));
  });

  it('includes fuzzilli', () => {
    sandbox.stub(Math, 'random').callsFake(() => 0.2);
    const archivePath = path.join(helpers.BASE_DIR, 'input_archive');

    // Create 2 test cases with a maximum of 2 inputs per test.
    const testRunner = new runner.RandomCorpusRunnerWithFuzzilli(
        archivePath, 'v8', 2, 2);
    var inputs = Array.from(testRunner.enumerateInputs());

    // Check the enumeration counter separately.
    assert.equal(2, inputs.length);
    assert.deepEqual([0, 1], inputs.map((x) => x[0]));

    assert.deepEqual(
        ["fuzzilli/fuzzdir-1/corpus/program_1.js",
         "fuzzilli/fuzzdir-2/crashes/program_3.js"],
        inputs[0][1].map((x) => x.relPath));
    assert.deepEqual(
        ["fuzzilli/fuzzdir-2/crashes/program_2.js",
         "fuzzilli/fuzzdir-1/corpus/program_1.js"],
        inputs[1][1].map((x) => x.relPath));
  });

  it('does not pick crashes with fuzzilli runner', () => {
    sandbox.stub(Math, 'random').callsFake(() => 0.2);
    const archivePath = path.join(helpers.BASE_DIR, 'input_archive');

    // Try to get 2 test cases, but there's only 1 viable in the test data.
    const testRunner = new runner.RandomFuzzilliNoCrashCorpusRunner(
        archivePath, 'v8', 2);
    var inputs = Array.from(testRunner.enumerateInputs());

    assert.equal(1, inputs.length);
    assert.deepEqual(
        ["fuzzilli/fuzzdir-1/corpus/program_1.js"],
        inputs[0][1].map((x) => x.relPath));
  });

  it('only picks Wasm cases', () => {
    helpers.deterministicRandom(sandbox);
    const archivePath = path.join(helpers.BASE_DIR, 'input_archive');

    // Create 4 test cases with a maximum of 2 inputs per test.
    const testRunner = new runner.RandomWasmCorpusRunner(
        archivePath, 'v8', 4, 2);
    var inputs = Array.from(testRunner.enumerateInputs());

    // Check the enumeration counter separately.
    assert.equal(4, inputs.length);
    assert.deepEqual([0, 1, 2, 3], inputs.map((x) => x[0]));

    assert.deepEqual(
        ["v8/test/mjsunit/wasm/regress-123.js",
         "fuzzilli/fuzzdir-2/crashes/program_3.js"],
        inputs[0][1].map((x) => x.relPath));
    assert.deepEqual(
        ["v8/test/mjsunit/wasm/regress-456.js",
         "fuzzilli/fuzzdir-2/crashes/program_3.js"],
        inputs[1][1].map((x) => x.relPath));
    assert.deepEqual(
        ["fuzzilli/fuzzdir-2/crashes/program_3.js"],
        inputs[2][1].map((x) => x.relPath));
    assert.deepEqual(
        ["fuzzilli/fuzzdir-2/crashes/program_3.js",
         "v8/test/mjsunit/wasm/regress-456.js"],
        inputs[3][1].map((x) => x.relPath));
  });
});
