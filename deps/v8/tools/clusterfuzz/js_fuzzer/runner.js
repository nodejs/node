// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Main execution flow.
 */

'use strict';

const path = require('path');

const db = require('./db.js');
const corpus = require('./corpus.js');
const random = require('./random.js');
const sourceHelpers = require('./source_helpers.js');

// Maximum number of test inputs to use for one fuzz test.
const MAX_TEST_INPUTS_PER_TEST = 10;
const MAX_WASM_TEST_INPUTS_PER_TEST = 5;

/**
 * Returns an array of maxium `count` parsed input sources, randomly
 * selected from a primary corpus and a list of secondary corpora.
 */
function getRandomInputs(primaryCorpus, secondaryCorpora, count) {
  count = random.randInt(2, count);

  // Choose 40%-80% of inputs from primary corpus.
  const primaryCount = Math.floor(random.uniform(0.4, 0.8) * count);
  count -= primaryCount;

  let inputs = primaryCorpus.getRandomTestcases(primaryCount);

  // Split remainder equally between the secondary corpora.
  const secondaryCount = Math.floor(count / secondaryCorpora.length);

  for (let i = 0; i < secondaryCorpora.length; i++) {
    let currentCount = secondaryCount;
    if (i == secondaryCorpora.length - 1) {
      // Last one takes the remainder.
      currentCount = count;
    }

    count -= currentCount;
    if (currentCount) {
      inputs = inputs.concat(
          secondaryCorpora[i].getRandomTestcases(currentCount));
    }
  }

  return random.shuffle(inputs);
}

class Runner {
  *inputGen() {
    throw new Error("Not implemented.");
  }

  *enumerateInputs() {
    let i = 0;
    for (const value of this.inputGen()) {
      yield [i, value];
      i++;
    }
  }
}

/**
 * Runner that randomly selects a number of tests from all corpora.
 */
class RandomCorpusRunner extends Runner {
  constructor(settings,
              maxTestInputs=MAX_TEST_INPUTS_PER_TEST) {
    super();
    const inputDir = path.resolve(settings.input_dir);
    this.primary = settings.engine;
    this.numFiles = settings.no_of_files;
    this.maxTestInputs = maxTestInputs;
    this.corpora = {
      'v8': corpus.create(inputDir, 'v8'),
      'chakra': corpus.create(inputDir, 'chakra'),
      'spidermonkey': corpus.create(inputDir, 'spidermonkey'),
      'jsc': corpus.create(inputDir, 'WebKit/JSTests'),
      'crash': corpus.create(inputDir, 'CrashTests'),
    };
  }

  *inputGen() {
    const primary = this.corpora[this.primary];
    const secondary = Object.values(this.corpora);

    for (let i = 0; i < this.numFiles; i++) {
      const inputs = getRandomInputs(
          primary,
          random.shuffle(secondary),
          this.maxTestInputs);

      if (inputs.length > 0) {
        yield inputs;
      }
    }
  }
}

/**
 * Like above, including the Fuzzilli corpus.
 */
class RandomCorpusRunnerWithFuzzilli extends RandomCorpusRunner {
  constructor(settings,
              maxTestInputs=MAX_TEST_INPUTS_PER_TEST) {
    super(settings, maxTestInputs);
    const forDiffFuzz = random.toggle(settings.diff_fuzz, 0.2);
    this.corpora.fuzzilli = corpus.create(
        settings.input_dir, 'fuzzilli', false, this.corpora.v8,
        forDiffFuzz);
  }
}

/**
 * Runner that randomly selects Wasm cases from V8 and Fuzzilli.
 */
class RandomWasmCorpusRunner extends Runner {
  constructor(settings,
              maxTestInputs=MAX_WASM_TEST_INPUTS_PER_TEST) {
    super();
    this.numFiles = settings.no_of_files;
    this.maxTestInputs = maxTestInputs;

    // Bias a bit towards the V8 corpus.
    const v8Corpus = corpus.create(settings.input_dir, 'v8_wasm');
    const fuzzilliCorpus = corpus.create(
        settings.input_dir, 'fuzzilli_wasm', false, v8Corpus);
    this.corpora = [v8Corpus, v8Corpus, fuzzilliCorpus];
  }

  *inputGen() {
    for (let i = 0; i < this.numFiles; i++) {
      const count = random.randInt(1, this.maxTestInputs);
      const inputs = [];
      for (let j= 0; j < count; j++) {
        inputs.push(...random.single(this.corpora).getRandomTestcases(1));
      }

      if (inputs.length > 0) {
        yield inputs;
      }
    }
  }
}

/**
 * Runner that enumerates all tests from a particular corpus.
 */
class SingleCorpusRunner extends Runner {
  constructor(inputDir, corpusName, extraStrict) {
    super();
    this.corpus = corpus.create(
      path.resolve(inputDir), corpusName, extraStrict);
  }

  *inputGen() {
    for (const input of this.corpus.getAllTestcases()) {
      yield [input];
    }
  }
}

/**
 * Runner that enumerates random cases from the Fuzzilli corpus without
 * repeats and without cases from the crashes directory.
 */
class RandomFuzzilliNoCrashCorpusRunner extends Runner {
  constructor(settings) {
    super();
    this.numFiles = settings.no_of_files;

    // We need a V8 corpus placeholder only to cross-load dependencies
    // from there, e.g. the wasm module builder.
    const v8Corpus = corpus.create(settings.input_dir, 'v8');
    const forDiffFuzz = random.toggle(settings.diff_fuzz, 0.2);
    this.corpus = corpus.create(
      settings.input_dir, 'fuzzilli_no_crash', false, v8Corpus,
      forDiffFuzz);
  }

  *inputGen() {
    // The 'permittedFiles' are already shuffled. Just using the first x
    // files is random enough.
    for (const relPath of this.corpus.permittedFiles.slice(0, this.numFiles)) {
      const source = this.corpus.loadTestcase(relPath, false, 'sloppy');
      if (source) {
        yield [source];
      }
    }
  }
}

module.exports = {
  RandomCorpusRunner: RandomCorpusRunner,
  RandomCorpusRunnerWithFuzzilli: RandomCorpusRunnerWithFuzzilli,
  RandomFuzzilliNoCrashCorpusRunner: RandomFuzzilliNoCrashCorpusRunner,
  RandomWasmCorpusRunner: RandomWasmCorpusRunner,
  SingleCorpusRunner: SingleCorpusRunner,
};
