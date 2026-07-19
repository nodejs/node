// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test normalization.
 */

'use strict';

const path = require('path');
const sinon = require('sinon');

const corpus = require('../corpus.js');
const helpers = require('./helpers.js');
const sourceHelpers = require('../source_helpers.js');

const { ScriptMutator } = require('../script_mutator.js');

const sandbox = sinon.createSandbox();

function testLoad(testPath, expectedPath, loadFun=helpers.loadTestData) {
  const mutator = new ScriptMutator({}, helpers.DB_DIR);
  const source = loadFun(testPath);
  const dependencies = mutator.resolveInputDependencies([source]);
  const code = sourceHelpers.generateCode(source, dependencies);
  helpers.assertExpectedResult(expectedPath, code);
}

describe('V8 dependencies', () => {
  it('test', () => {
    testLoad(
        'mjsunit/test_load.js',
        'mjsunit/test_load_expected.js');
  });

  it('does not loop indefinitely', () => {
    testLoad(
        'mjsunit/test_load_self.js',
        'mjsunit/test_load_self_expected.js');
  });

  it('loads mjsunit dependencies also from another corpus', () => {
    const corpusDir = path.join(helpers.BASE_DIR, 'load' ,'fuzzilli_scenario');
    const v8Corpus = corpus.create(corpusDir, 'v8');
    const fuzzilliCorpus = corpus.create(
        corpusDir, 'fuzzilli', false, v8Corpus);
    const load = (relPath) => {
      return sourceHelpers.loadSource(fuzzilliCorpus, relPath);
    };
    testLoad(
        'fuzzilli/fuzzdir-1/corpus/program_x.js',
        'load/fuzzilli_scenario/test_load_expected.js',
        load);
  });
});

describe('Chakra dependencies', () => {
  it('test', () => {
    testLoad(
        'chakra/load.js',
        'chakra/load_expected.js');
  });
});

describe('JSTest dependencies', () => {
  afterEach(() => {
    sandbox.restore();
  });

  it('test', () => {
    const fakeStubs = sourceHelpers.loadSource(
        helpers.TEST_CORPUS, 'JSTests/fake_stub.js');
    sandbox.stub(sourceHelpers, 'loadResource').callsFake(() => fakeStubs);
    testLoad('JSTests/load.js', 'JSTests/load_expected.js');
  });
});

describe('SpiderMonkey dependencies', () => {
  it('test', () => {
    testLoad(
        'spidermonkey/test/load.js',
        'spidermonkey/test/load_expected.js');
  });
});

describe('Sandbox dependencies', () => {
  it('test', () => {
    testLoad(
        'sandbox/load.js',
        'sandbox/load_expected.js');
  });
});

describe('Language features', () => {
  it('test', () => {
    testLoad(
        'language/features.js',
        'language/features_expected.js');
  });
});

describe('Transpile', () => {
  // Test using Babel transpile when loading sources.
  it('test', () => {
    const corpusDir = path.join(helpers.BASE_DIR, 'transpile');
    const v8Corpus = corpus.create(corpusDir, 'v8');
    const load = (relPath) => {
      return sourceHelpers.loadSource(v8Corpus, relPath, false, true);
    };
    testLoad(
        // The inputs contain all class-related language features to test the
        // proper usage of the Babel transpile plugins and our post-processing.
        'v8/test/mjsunit/test1.js',
        // The test output contains the inlined non-transpiled dependencies
        // (here mjsunit.js) and the transpiled test1.js content.
        'transpile/v8/test/mjsunit/test1_expected.js',
        load);
  });
});

describe('Proto assignments', () => {
  // Unit test `inlineProtoAssignments`.
  it('test', () => {
    const corpusDir = path.join(helpers.BASE_DIR, 'transpile');
    const v8Corpus = corpus.create(corpusDir, 'v8');
    const source = sourceHelpers.loadSource(v8Corpus, 'proto1.js');
    sourceHelpers.inlineProtoAssignments(source.ast);
    const code = sourceHelpers.generateCode(source, []);
    helpers.assertExpectedResult('transpile/proto1_expected.js', code);
  });
});
