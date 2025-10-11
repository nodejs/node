// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Regression tests.
 */

'use strict';

const assert = require('assert');
const { execSync } = require("child_process");
const fs = require('fs');
const path = require('path');
const sinon = require('sinon');
const tempfile = require('tempfile');
const tempy = require('tempy');

const baseMutator = require('../mutators/mutator.js');
const crossOver = require('../mutators/crossover_mutator.js');
const db = require('../db.js');
const exceptions = require('../exceptions.js');
const functionCallMutator = require('../mutators/function_call_mutator.js');
const helpers = require('./helpers.js');
const random = require('../random.js');
const scriptMutator = require('../script_mutator.js');
const sourceHelpers = require('../source_helpers.js');
const tryCatch = require('../mutators/try_catch.js');
const variableMutator = require('../mutators/variable_mutator.js');


const sandbox = sinon.createSandbox();

const SYNTAX_ERROR_RE = /.*SyntaxError.*/

function createFuzzTest(fake_db, settings, inputFiles) {
  const sources = inputFiles.map(input => helpers.loadV8TestData(input));

  const mutator = new scriptMutator.ScriptMutator(settings, fake_db);
  const result = mutator.mutateMultiple(sources);

  const output_file = tempfile('.js');
  fs.writeFileSync(output_file, result.code);
  return { file:output_file, flags:result.flags };
}

function execFile(jsFile) {
  execSync("node --allow-natives-syntax " + jsFile, {stdio: ['pipe']});
}

describe('Regression tests', () => {
  beforeEach(() => {
    helpers.deterministicRandom(sandbox);
    this.settings = helpers.zeroSettings();
  });

  afterEach(() => {
    sandbox.restore();
  });

  it('combine strict and with', () => {
    // Test that when a file with "use strict" is used in the inputs,
    // the result is only strict if no other file contains anything
    // prohibited in strict mode (here a with statement).
    // It is assumed that such input files are marked as sloppy in the
    // auto generated exceptions.
    sandbox.stub(exceptions, 'getGeneratedSloppy').callsFake(
        () => { return new Set(['regress/strict/input_with.js']); });
    const {file, flags} = createFuzzTest(
        'test_data/regress/empty_db',
        this.settings,
        ['regress/strict/input_strict.js', 'regress/strict/input_with.js']);
    execFile(file);
  });

  it('combine strict and with, life analysis', () => {
    // As above, but without the sloppy file being marked. "Strict"
    // incompatibility will also be detected at parse time.
    const {file, flags} = createFuzzTest(
        'test_data/regress/empty_db',
        this.settings,
        ['regress/strict/input_strict.js', 'regress/strict/input_with.js']);
    execFile(file);
  });

  it('combine strict and delete', () => {
    // As above with unqualified delete.
    sandbox.stub(exceptions, 'getGeneratedSloppy').callsFake(
        () => { return new Set(['regress/strict/input_delete.js']); });
    const {file, flags} = createFuzzTest(
        'test_data/regress/empty_db',
        this.settings,
        ['regress/strict/input_strict.js', 'regress/strict/input_delete.js']);
    execFile(file);
  });

  it('combine strict and delete, life analysis', () => {
    // As above, but without the sloppy file being marked. "Strict"
    // incompatibility will also be detected at parse time.
    const {file, flags} = createFuzzTest(
        'test_data/regress/empty_db',
        this.settings,
        ['regress/strict/input_strict.js', 'regress/strict/input_delete.js']);
    execFile(file);
  });

  it('mutates negative value', () => {
    // This tests that the combination of number, function call and expression
    // mutator does't produce an update expression.
    // Previously the 1 in -1 was replaced with another negative number leading
    // to e.g. -/*comment/*-2. Then cloning the expression removed the
    // comment and produced --2 in the end.
    this.settings['MUTATE_NUMBERS'] = 1.0;
    this.settings['MUTATE_FUNCTION_CALLS'] = 1.0;
    this.settings['MUTATE_EXPRESSIONS'] = 1.0;
    const {file, flags} = createFuzzTest(
        'test_data/regress/empty_db',
        this.settings,
        ['regress/numbers/input_negative.js']);
    execFile(file);
  });

  it('mutates indices', () => {
    // Test that indices are not replaced with a negative number causing a
    // syntax error (e.g. {-1: ""}).
    this.settings['MUTATE_NUMBERS'] = 1.0;
    const {file, flags} = createFuzzTest(
        'test_data/regress/empty_db',
        this.settings,
        ['regress/numbers/input_indices.js']);
    execFile(file);
  });

  it('does not collide the module builder', () => {
    const {file, flags} = createFuzzTest(
        'test_data/regress/empty_db',
        this.settings,
        ['regress/dependency_collision/v8/test/mjsunit/wasm/input.js',
         'regress/dependency_collision/chakra/input.js']);
    execFile(file);
  });

  it('resolves flag contradictions', () => {
    sandbox.stub(exceptions, 'CONTRADICTORY_FLAGS').value(
        [['--flag1', '--flag2']])
    const {file, flags} = createFuzzTest(
        'test_data/regress/empty_db',
        this.settings,
        ['v8/regress/contradictions/input1.js',
         'v8/regress/contradictions/input2.js']);
    assert.deepEqual(['--flag1'], flags);
  });

  it('skips mjs flags', () => {
    const {file, flags} = createFuzzTest(
        'test_data/regress/empty_db',
        this.settings,
        ['v8/regress/mjs_flags/input.js']);
    assert.deepEqual(['--ok-flag1', '--ok-flag2'], flags);
  });

  function testSuper(settings, db_path, expected) {
    // Enforce mutations at every possible location.
    settings['MUTATE_CROSSOVER_INSERT'] = 1.0;
    // Choose the only-super-statments path. This also fixed the insertion
    // order to only insert before a statement.
    sandbox.stub(random, 'choose').callsFake(() => { return true; });

    const fakeDb = new db.MutateDb(db_path);
    const mutator = new crossOver.CrossOverMutator(settings, fakeDb);

    // An input with a couple of insertion spots in constructors and
    // methods of two classes. One root and one a subclass.
    const source = helpers.loadTestData('regress/super/input.js');
    mutator.mutate(source);

    const mutated = sourceHelpers.generateCode(source);
    helpers.assertExpectedResult(expected, mutated);
  }

  it('mutates super call', () => {
    // Ensure that a super() call expression isn't added to a
    // non-constructor class member or to a root class.
    testSuper(
        this.settings,
        'test_data/regress/super/super_call_db',
        'regress/super/call_expected.js');
  });

  it('mutates super member expression', () => {
    // Ensure that a super.x member expression isn't added to a
    // root class.
    testSuper(
        this.settings,
        'test_data/regress/super/super_member_db',
        'regress/super/member_expected.js');
  });

  it('does not cross-insert duplicate variables', () => {
    // Ensure we don't declare a duplicate variable when the
    // declaration is part of a cross-over inserted snippet.
    this.settings['MUTATE_CROSSOVER_INSERT'] = 1.0;
    const fakeDb = new db.MutateDb(
        'test_data/regress/duplicates/duplicates_db');
    const mutator = new crossOver.CrossOverMutator(this.settings, fakeDb);

    const source = helpers.loadTestData('regress/duplicates/input.js');
    mutator.mutate(source);

    const mutated = sourceHelpers.generateCode(source);
    helpers.assertExpectedResult(
        'regress/duplicates/duplicates_expected.js', mutated);
  });

  function testAsyncReplacements(settings, expected) {
    settings['MUTATE_FUNCTION_CALLS'] = 1.0;
    sandbox.stub(sourceHelpers, 'loadResource').callsFake(() => {
      return helpers.loadTestData('differential_fuzz/fake_resource.js');
    });

    // Go only into function-call replacements.
    sandbox.stub(random, 'random').callsFake(() => { return 0.2; });

    // Input with several async and non-async replacement sites.
    const source = helpers.loadTestData('regress/async/input.js');
    const mutator = new scriptMutator.ScriptMutator(
        settings, 'test_data/regress/empty_db');
    const mutated = mutator.mutateMultiple([source]).code;
    helpers.assertExpectedResult(expected, mutated);
  }

  it('makes no cross-async replacements', () => {
    // Test to show that when REPLACE_CROSS_ASYNC_PROB isn't chosen, there
    // are no replacements that change the async property.
    sandbox.stub(functionCallMutator, 'REPLACE_CROSS_ASYNC_PROB').value(0);
    testAsyncReplacements(
        this.settings, 'regress/async/no_async_expected.js');
  });

  it('makes full cross-async replacements', () => {
    // Test to show that when REPLACE_CROSS_ASYNC_PROB is chosen, the
    // replacement functions are from the pool of all functions (both
    // maintaining and not maintaining the async property).
    sandbox.stub(functionCallMutator, 'REPLACE_CROSS_ASYNC_PROB').value(1);
    testAsyncReplacements(
        this.settings, 'regress/async/full_async_expected.js');
  });

  it('does not drop required parentheses', () => {
    sandbox.stub(sourceHelpers, 'loadResource').callsFake(() => {
      return helpers.loadTestData('differential_fuzz/fake_resource.js');
    });

    const source = helpers.loadTestData('regress/parentheses/input.js');
    const mutator = new scriptMutator.ScriptMutator(
        this.settings, 'test_data/regress/empty_db');
    const mutated = mutator.mutateMultiple([source]).code;
    helpers.assertExpectedResult('regress/parentheses/expected.js', mutated);
  });

  it('loads async iterator resource', () => {
    // Test that inputs with the `AsyncIterator` identifier load an additional
    // stub.
    sandbox.stub(sourceHelpers, 'loadResource').callsFake((resource) => {
      if (resource === 'async_iterator.js') {
        // Stub out resource loading, except for the resource in question.
        return sourceHelpers.loadResource.wrappedMethod(resource);
      }
      return helpers.loadTestData('differential_fuzz/fake_resource.js');
    });

    const source = helpers.loadTestData('regress/iterator/input.js');
    const mutator = new scriptMutator.ScriptMutator(
        this.settings, 'test_data/regress/empty_db');
    const mutated = mutator.mutateMultiple([source]).code;
    helpers.assertExpectedResult('regress/iterator/expected.js', mutated);
  });

  it('does not crash on spurious identifiers', () => {
    // Test that we don't erroneously load identifiers like `toString` from
    // the map of additional resources.
    const source = helpers.loadTestData('regress/resources/input.js');
    const mutator = new scriptMutator.ScriptMutator(
        this.settings, 'test_data/regress/empty_db');
    const mutated = mutator.mutateMultiple([source]).code;
  });

  it('does not mutate loop variables', () => {
    sandbox.stub(sourceHelpers, 'loadResource').callsFake(() => {
      return helpers.loadTestData('differential_fuzz/fake_resource.js');
    });

    // Choose replacement in var-or-object mutation.
    sandbox.stub(random, 'random').callsFake(() => 0.75);

    // We usually skip loop variables with 0.95 - set it to 1.0
    // for a more predictable test.
    sandbox.stub(variableMutator, 'SKIP_LOOP_VAR_PROB').value(1.0);

    // Try-catch is not helpful for reading the test output.
    sandbox.stub(
        tryCatch.AddTryCatchMutator.prototype, "mutate").returns(undefined);

    // Maximum variable mutations.
    this.settings['MUTATE_VARIABLES'] = 1.0;
    this.settings['ADD_VAR_OR_OBJ_MUTATIONS'] = 1.0;

    // Also stress closure removal (though it has no effect in this test).
    this.settings['TRANSFORM_CLOSURES'] = 1.0;

    const source = helpers.loadTestData('loop_mutations.js');
    const mutator = new scriptMutator.ScriptMutator(
        this.settings, 'test_data/regress/empty_db');
    const mutated = mutator.mutateMultiple([source]).code;
    helpers.assertExpectedResult('loop_mutations_expected.js', mutated);
  });

  it('does not block-wrap nested functions', () => {
    sandbox.stub(sourceHelpers, 'loadResource').callsFake(() => {
      return helpers.loadTestData('differential_fuzz/fake_resource.js');
    });

    // Maximum top-level wrapping and no skipping try-catch to
    // deterministically try to wrap an entire function block.
    sandbox.stub(tryCatch, 'DEFAULT_SKIP_PROB').value(0.0);
    sandbox.stub(tryCatch, 'DEFAULT_TOPLEVEL_PROB').value(1.0);
    sandbox.stub(tryCatch, 'IGNORE_DEFAULT_PROB').value(0.0);

    const source = helpers.loadTestData('regress/legacy_scope/input.js');
    const mutator = new scriptMutator.ScriptMutator(
        this.settings, 'test_data/regress/empty_db');
    const mutated = mutator.mutateMultiple([source]).code;
    helpers.assertExpectedResult('regress/legacy_scope/expected.js', mutated);
  });

  it('does not try-catch wrap in infinite while loops', () => {
    sandbox.stub(sourceHelpers, 'loadResource').callsFake(() => {
      return helpers.loadTestData('differential_fuzz/fake_resource.js');
    });

    // No top-level wrapping and no skipping try-catch to
    // deterministically try to wrap within the loop.
    sandbox.stub(tryCatch, 'DEFAULT_SKIP_PROB').value(0.0);
    sandbox.stub(tryCatch, 'DEFAULT_TOPLEVEL_PROB').value(0.0);
    sandbox.stub(tryCatch, 'IGNORE_DEFAULT_PROB').value(0.0);

    const source = helpers.loadTestData('regress/try_catch_while_true/input.js');
    const mutator = new scriptMutator.ScriptMutator(
        this.settings, 'test_data/regress/empty_db');
    const mutated = mutator.mutateMultiple([source]).code;
    helpers.assertExpectedResult('regress/try_catch_while_true/expected.js', mutated);
  });

  it('does not replace with diverging functions', () => {
    sandbox.stub(sourceHelpers, 'loadResource').callsFake(() => {
      return helpers.loadTestData('differential_fuzz/fake_resource.js');
    });

    // Go only into function-call replacements.
    sandbox.stub(random, 'random').callsFake(() => { return 0.2; });
    this.settings['MUTATE_FUNCTION_CALLS'] = 1.0;

    const source = helpers.loadTestData('regress/infinite_loop_fun/input.js');
    const mutator = new scriptMutator.ScriptMutator(
        this.settings, 'test_data/regress/empty_db');
    const mutated = mutator.mutateMultiple([source]).code;
    helpers.assertExpectedResult('regress/infinite_loop_fun/expected.js', mutated);
  });

  it('mutates less in large loops', () => {
    sandbox.stub(sourceHelpers, 'loadResource').callsFake(() => {
      return helpers.loadTestData('differential_fuzz/fake_resource.js');
    });

    // Try-catch is not helpful for reading the test output.
    sandbox.stub(
        tryCatch.AddTryCatchMutator.prototype, "mutate").returns(undefined);

    // Maximum var/obj and cross-over mutations.
    this.settings['ADD_VAR_OR_OBJ_MUTATIONS'] = 1.0;
    this.settings['MUTATE_CROSSOVER_INSERT'] = 1.0;
    const fakeDb = new db.MutateDb(
        'test_data/regress/duplicates/duplicates_db');
    sandbox.stub(crossOver.CrossOverMutator.prototype, "db").returns(fakeDb);

    // Test case with large loops.
    const source = helpers.loadTestData('regress/large_loops/input.js');
    const mutator = new scriptMutator.ScriptMutator(
        this.settings, 'test_data/regress/empty_db');
    const mutated = mutator.mutateMultiple([source]).code;
    helpers.assertExpectedResult('regress/large_loops/expected.js', mutated);
  });

  // TODO(https://crbug.com/389069288): This still needs fixing, though it's
  // probably rare that usage of 'arguments' and assigning to 'arguments'
  // happen in the same test case. Due to the assignment, all occurences
  // are normalized. Such an assignment is not allowed in strict mode and
  // causes a syntax error in this case. We'd need to fix this in a way that
  // unnormalized argument assignment is not mixed with strict-mode cases.
  //
  // It's analogue with eval and undefined, though the strict-mode rules are
  // asymmetric:
  //
  // Not allowed in strict mode:
  // let eval = 0;
  // let arguments = 0;
  // arguments = 0;
  // undefined = 0;
  // eval = 0
  //
  // Allowed in strict mode:
  // let undefined = 0;
  it('does not normalize arguments', () => {
    sandbox.stub(sourceHelpers, 'loadResource').callsFake(() => {
      return helpers.loadTestData('differential_fuzz/fake_resource.js');
    });

    // Try-catch is not helpful for reading the test output.
    sandbox.stub(
        tryCatch.AddTryCatchMutator.prototype, "mutate").returns(undefined);

    const source = helpers.loadTestData('regress/arguments/input.js');
    const mutator = new scriptMutator.ScriptMutator(
        this.settings, 'test_data/regress/empty_db');
    const mutated = mutator.mutateMultiple([source]).code;
    helpers.assertExpectedResult('regress/arguments/expected.js', mutated);
  });

  it('does not try-catch wrap yield expressions', () => {
    sandbox.stub(sourceHelpers, 'loadResource').callsFake(() => {
      return helpers.loadTestData('differential_fuzz/fake_resource.js');
    });

    const source = helpers.loadTestData('regress/yield/input.js');
    const mutator = new scriptMutator.ScriptMutator(
        this.settings, 'test_data/regress/empty_db');
    const mutated = mutator.mutateMultiple([source]).code;
    helpers.assertExpectedResult('regress/yield/expected.js', mutated);
  });

  it('does not assign to const variables', () => {
    sandbox.stub(sourceHelpers, 'loadResource').callsFake(() => {
      return helpers.loadTestData('differential_fuzz/fake_resource.js');
    });
    this.settings['MUTATE_VARIABLES'] = 1.0;

    const source = helpers.loadTestData('regress/const_var/input.js');
    const mutator = new scriptMutator.ScriptMutator(
        this.settings, 'test_data/regress/empty_db');
    const mutated = mutator.mutateMultiple([source]).code;
    helpers.assertExpectedResult('regress/const_var/expected.js', mutated);
  });
});

describe('DB tests', () => {
  afterEach(() => {
    sandbox.restore();
  });

  it('creates DB with Object.assign', () => {
    // Test that an Object.assign expression is inserted into the snippet DB.
    const tmpOut = tempy.directory();
    const mutateDb = new db.MutateDbWriter(tmpOut);
    const source = helpers.loadTestData('regress/db/input/input.js');
    mutateDb.process(source);
    mutateDb.writeIndex();
    helpers.assertExpectedPath('regress/db/assign_expected', tmpOut);
  });
});
