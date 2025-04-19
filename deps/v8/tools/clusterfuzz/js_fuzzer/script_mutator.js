// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Script mutator.
 */

'use strict';

const assert = require('assert');
const fs = require('fs');
const path = require('path');

const babelTraverse = require('@babel/traverse').default;
const babelTypes = require('@babel/types');

const common = require('./mutators/common.js');
const db = require('./db.js');
const exceptions = require('./exceptions.js');
const random = require('./random.js');
const runner = require('./runner.js');
const sourceHelpers = require('./source_helpers.js');

const { AddTryCatchMutator } = require('./mutators/try_catch.js');
const { ArrayMutator } = require('./mutators/array_mutator.js');
const { ClosureRemover } = require('./mutators/closure_remover.js');
const { ContextAnalyzer } = require('./mutators/analyzer.js');
const { CrossOverMutator } = require('./mutators/crossover_mutator.js');
const { ExpressionMutator } = require('./mutators/expression_mutator.js');
const { FunctionCallMutator } = require('./mutators/function_call_mutator.js');
const { IdentifierNormalizer } = require('./mutators/normalizer.js');
const { MutationContext } = require('./mutators/mutator.js');
const { NumberMutator } = require('./mutators/number_mutator.js');
const { ObjectMutator } = require('./mutators/object_mutator.js');
const { VariableMutator } = require('./mutators/variable_mutator.js');
const { VariableOrObjectMutator } = require('./mutators/variable_or_object_mutation.js');

const CHAKRA_WASM_MODULE_BUILDER_REL = 'chakra/WasmSpec/testsuite/harness/wasm-module-builder.js'
const CHAKRA_WASM_CONSTANTS_REL = 'chakra/WasmSpec/testsuite/harness/wasm-constants.js'
const V8_WASM_MODULE_BUILDER_REL = 'v8/test/mjsunit/wasm/wasm-module-builder.js';

const MAX_EXTRA_MUTATIONS = 5;

function defaultSettings() {
  return {
    ADD_VAR_OR_OBJ_MUTATIONS: 0.1,
    DIFF_FUZZ_EXTRA_PRINT: 0.1,
    DIFF_FUZZ_TRACK_CAUGHT: 0.4,
    MUTATE_ARRAYS: 0.1,
    MUTATE_CROSSOVER_INSERT: 0.05,
    MUTATE_EXPRESSIONS: 0.1,
    MUTATE_FUNCTION_CALLS: 0.1,
    MUTATE_NUMBERS: 0.05,
    MUTATE_OBJECTS: 0.1,
    MUTATE_VARIABLES: 0.075,
    SCRIPT_MUTATOR_EXTRA_MUTATIONS: 0.2,
    SCRIPT_MUTATOR_SHUFFLE: 0.2,
    // Probability to remove certain types of closures: Anonymous parameterless
    // functions calling themselves, but not referencing themselves. These
    // appear often appear in test input and subsequent mutations are more
    // likely without these closures.
    TRANSFORM_CLOSURES: 0.2,
  };
}

/**
 * Create a context with information, useful in subsequent analyses.
 */
function analyzeContext(source) {
  const analyzer = new ContextAnalyzer();
  const context = new MutationContext();
  analyzer.mutate(source, context);
  return context;
}

class Result {
  constructor(code, flags) {
    this.code = code;
    this.flags = flags;
  }
}

class ScriptMutator {
  constructor(settings, db_path=undefined) {
    // Use process.cwd() to bypass pkg's snapshot filesystem.
    this.mutateDb = new db.MutateDb(db_path || path.join(process.cwd(), 'db'));
    this.crossover = new CrossOverMutator(settings, this.mutateDb);
    this.mutators = [
      new ArrayMutator(settings),
      new ObjectMutator(settings),
      new VariableMutator(settings),
      new NumberMutator(settings),
      this.crossover,
      new ExpressionMutator(settings),
      new FunctionCallMutator(settings),
      new VariableOrObjectMutator(settings),
    ];
    this.closures = new ClosureRemover(settings);
    this.trycatch = new AddTryCatchMutator(settings);
    this.settings = settings;
  }

  /**
   * Returns a runner class that decides the composition of tests from
   * different corpora.
   */
  get runnerClass() {
    // Choose a setup with the Fuzzilli corpus with a 50% chance.
    return random.single(
        [runner.RandomCorpusRunner, runner.RandomCorpusRunnerWithFuzzilli]);
  }

  _addMjsunitIfNeeded(dependencies, input) {
    if (dependencies.has('mjsunit')) {
      return;
    }

    if (!input.absPath.includes('mjsunit')) {
      return;
    }

    // Find mjsunit.js
    let mjsunitPath = input.absPath;
    while (path.dirname(mjsunitPath) != mjsunitPath &&
           path.basename(mjsunitPath) != 'mjsunit') {
      mjsunitPath = path.dirname(mjsunitPath);
    }

    if (path.basename(mjsunitPath) == 'mjsunit') {
      mjsunitPath = path.join(mjsunitPath, 'mjsunit.js');
      dependencies.set('mjsunit', sourceHelpers.loadDependencyAbs(
          input.corpus, mjsunitPath));
      return;
    }

    console.log('ERROR: Failed to find mjsunit.js');
  }

  _addSpiderMonkeyShellIfNeeded(dependencies, input) {
    // Find shell.js files
    const shellJsPaths = new Array();
    let currentDir = path.dirname(input.absPath);

    while (path.dirname(currentDir) != currentDir) {
      const shellJsPath = path.join(currentDir, 'shell.js');
      if (fs.existsSync(shellJsPath)) {
         shellJsPaths.push(shellJsPath);
      }

      if (currentDir == 'spidermonkey') {
        break;
      }
      currentDir = path.dirname(currentDir);
    }

    // Add shell.js dependencies in reverse to add ones that are higher up in
    // the directory tree first.
    for (let i = shellJsPaths.length - 1; i >= 0; i--) {
      if (!dependencies.has(shellJsPaths[i])) {
        const dependency = sourceHelpers.loadDependencyAbs(
            input.corpus, shellJsPaths[i]);
        dependencies.set(shellJsPaths[i], dependency);
      }
    }
  }

  _addStubsIfNeeded(dependencies, input, baseName, corpusDir) {
    if (dependencies.has(baseName) || !input.absPath.includes(corpusDir)) {
      return;
    }
    dependencies.set(baseName, sourceHelpers.loadResource(baseName + '.js'));
  }

  _addJSTestStubsIfNeeded(dependencies, input) {
    this._addStubsIfNeeded(dependencies, input, 'jstest_stubs', 'JSTests');
  }

  _addChakraStubsIfNeeded(dependencies, input) {
    this._addStubsIfNeeded(dependencies, input, 'chakra_stubs', 'chakra');
  }

  _addSpidermonkeyStubsIfNeeded(dependencies, input) {
    this._addStubsIfNeeded(
        dependencies, input, 'spidermonkey_stubs', 'spidermonkey');
  }

  mutate(source, context) {
    let mutators = this.mutators.slice();
    let annotations = [];
    if (random.choose(this.settings.SCRIPT_MUTATOR_SHUFFLE)){
      annotations.push(' Script mutator: using shuffled mutators');
      random.shuffle(mutators);
    }

    if (random.choose(this.settings.SCRIPT_MUTATOR_EXTRA_MUTATIONS)){
      for (let i = random.randInt(1, MAX_EXTRA_MUTATIONS); i > 0; i--) {
        let mutator = random.single(this.mutators);
        mutators.push(mutator);
        annotations.push(` Script mutator: extra ${mutator.constructor.name}`);
      }
    }

    // We always remove certain closures first.
    mutators.unshift(this.closures);

    // Try-catch wrapping should always be the last mutation.
    mutators.push(this.trycatch);

    for (const mutator of mutators) {
      mutator.mutate(source, context);
    }

    for (const annotation of annotations.reverse()) {
      sourceHelpers.annotateWithComment(source.ast, annotation);
    }
  }

  /**
   * Particular dependencies have precedence over others due to duplicate
   * variable declarations in their sources.
   *
   * This is currently only implemented for the wasm-module-builder, which
   * lives in V8 and in an older version in the Chakra test suite. It could
   * be generalized for other cases.
   */
  resolveCollisions(inputs) {
    let hasWasmModuleBuilder = false;
    inputs.forEach(input => {
      hasWasmModuleBuilder |= input.dependentPaths.filter(
          (x) => x.endsWith(V8_WASM_MODULE_BUILDER_REL)).length;
    });
    if (!hasWasmModuleBuilder) {
      return;
    }
    inputs.forEach(input => {
      input.dependentPaths = input.dependentPaths.filter(
          (x) => !x.endsWith(CHAKRA_WASM_MODULE_BUILDER_REL) &&
                 !x.endsWith(CHAKRA_WASM_CONSTANTS_REL));
    });
  }

  // Returns parsed dependencies for inputs.
  resolveInputDependencies(inputs) {
    const dependencies = new Map();

    // Resolve test harness files.
    inputs.forEach(input => {
      try {
        // TODO(machenbach): Some harness files contain load expressions
        // that are not recursively resolved. We already remove them, but we
        // also need to load the dependencies they point to.
        this._addJSTestStubsIfNeeded(dependencies, input);
        this._addChakraStubsIfNeeded(dependencies, input);
        this._addMjsunitIfNeeded(dependencies, input);
        this._addSpidermonkeyStubsIfNeeded(dependencies, input);
        this._addSpiderMonkeyShellIfNeeded(dependencies, input);
      } catch (e) {
        console.log(
            'ERROR: Failed to resolve test harness for', input.relPath);
        throw e;
      }
    });

    // Resolve dependencies loaded within the input files.
    inputs.forEach(input => {
      try {
        input.loadDependencies(dependencies);
      } catch (e) {
        console.log(
            'ERROR: Failed to resolve dependencies for', input.relPath);
        throw e;
      }
    });

    // Map.values() returns values in insertion order.
    return Array.from(dependencies.values());
  }

  // Combines input dependencies with fuzzer resources.
  resolveDependencies(inputs) {
    this.resolveCollisions(inputs);
    const dependencies = this.resolveInputDependencies(inputs);

    // Add stubs for non-standard functions in the beginning.
    dependencies.unshift(sourceHelpers.loadResource('stubs.js'));

    // Add our fuzzing support helpers. This also overrides some common test
    // functions from earlier dependencies that cause early bailouts.
    dependencies.push(sourceHelpers.loadResource('fuzz_library.js'));

    return dependencies;
  }

  concatInputs(inputs) {
    return common.concatPrograms(inputs);
  }

  // Normalizes, combines and mutates multiple inputs.
  mutateInputs(inputs, dependencies) {
    const normalizerMutator = new IdentifierNormalizer();
    for (const [index, input] of inputs.entries()) {
      try {
        normalizerMutator.mutate(input);
      } catch (e) {
        console.log('ERROR: Failed to normalize ', input.relPath);
        throw e;
      }

      common.setSourceLoc(input, index, inputs.length);
    }

    // Combine ASTs into one. This is so that mutations have more context to
    // cross over content between ASTs (e.g. variables).
    const combinedSource = this.concatInputs(inputs);

    // First pass for context information, then run other mutators.
    const context = analyzeContext(combinedSource);
    this.mutate(combinedSource, context);

    // Add extra resources determined during mutation.
    for (const resource of context.extraResources.values()) {
      dependencies.push(sourceHelpers.loadResource(resource));
    }

    return combinedSource;
  }

  mutateMultiple(inputs) {
    // High level operation:
    // 1) Compute dependencies from inputs.
    // 2) Normalize, combine and mutate inputs.
    // 3) Generate code with dependency code prepended.
    // 4) Combine and filter flags from inputs.
    const dependencies = this.resolveDependencies(inputs);
    const combinedSource = this.mutateInputs(inputs, dependencies);
    const code = sourceHelpers.generateCode(combinedSource, dependencies);
    const allFlags = common.concatFlags(dependencies.concat([combinedSource]));
    const filteredFlags = exceptions.resolveContradictoryFlags(
        exceptions.filterFlags(allFlags));
    return new Result(code, filteredFlags);
  }
}

/**
 * Script mutator that only generates files depending on the
 * wasm-module-builder with appropriate mutations.
 */
class WasmScriptMutator extends ScriptMutator {
  constructor(settings, db_path) {
    super(settings, db_path);

    // Decrease cross-over and object mutations. Cross-over rarely
    // works well with Wasm. Object mutations might easily invalidate the
    // Wasm modules.
    this.settings.MUTATE_CROSSOVER_INSERT = 0.01;
    this.settings.MUTATE_OBJECTS = 0.05;

    // Increase number, variable and function-call mutations, which often
    // leave the underlying wasm-module-builder structures intact.
    this.settings.MUTATE_NUMBERS = 0.1;
    this.settings.MUTATE_VARIABLES = 0.1;
    this.settings.MUTATE_FUNCTION_CALLS = 0.15;

    // High likelihood to drop closures, as many wasm-module-builder cases
    // are wrapped with those. After the transformation, subsequent
    // mutations have more impact on the resulting code.
    this.settings.TRANSFORM_CLOSURES = 0.5;
  }

  get runnerClass() {
    return runner.RandomWasmCorpusRunner;
  }
}

/**
 * Script mutator that only inserts one cross-over expression from the DB to
 * validate.
 */
class CrossScriptMutator extends ScriptMutator {

  // We don't do any mutations except a deterministic insertion of one
  // snippet into a predefined place in a template.
  mutate(source, context) {
    // The __expression was pinned to the expression in the FixtureRunner.
    assert(source.__expression);
    const crossover = this.crossover;
    let done = false;
    babelTraverse(source.ast, {
      ExpressionStatement(path) {
        if (done || !path.node.expression ||
            !babelTypes.isCallExpression(path.node.expression)) {
          return;
        }
        // Avoid infinite loops if there's an expression statement in the
        // inserted expression.
        done = true;
        path.insertAfter(crossover.createInsertion(path, source.__expression));
      }
    });
  }

  // This mutator has only one input to which the __expression was pinned.
  concatInputs(inputs) {
    assert(inputs.length == 1);
    return inputs[0];
  }

  // No dependencies needed for simple snippet evaluation.
  resolveDependencies() {
    return [];
  }

  get runnerClass() {
    return runner.FixtureRunner;
  }
}

module.exports = {
  analyzeContext: analyzeContext,
  defaultSettings: defaultSettings,
  CrossScriptMutator: CrossScriptMutator,
  ScriptMutator: ScriptMutator,
  WasmScriptMutator: WasmScriptMutator,
};
