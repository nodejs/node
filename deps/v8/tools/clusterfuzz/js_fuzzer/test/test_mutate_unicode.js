// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test string-unicode mutator.
 */

'use strict';

const sinon = require('sinon');

const helpers = require('./helpers.js');
const scriptMutator = require('../script_mutator.js');
const stringUnicode = require('../mutators/string_unicode_mutator.js');
const random = require('../random.js');
const sourceHelpers = require('../source_helpers.js');

const sandbox = sinon.createSandbox();



describe('String Unicode mutator', () => {

  afterEach(() => {
    sandbox.restore();
  });

  it('mutates all characters in identifiers with unicode escapes', () => {
    sandbox.restore();
    helpers.deterministicRandom(sandbox);

    const source = helpers.loadTestData('mutate_string.js');

    const settings = scriptMutator.defaultSettings();
    settings.MUTATE_UNICODE_ESCAPE_PROB = 1.0;
    settings.ENABLE_IDENTIFIER_UNICODE_ESCAPE = 1.0;
    settings.ENABLE_STRINGLITERAL_UNICODE_ESCAPE = 0.0;
    settings.ENABLE_REGEXPLITERAL_UNICODE_ESCAPE = 0.0;

    const mutator = new stringUnicode.StringUnicodeMutator(settings);
    mutator.mutate(source);

    const mutated = sourceHelpers.generateCode(source);

    helpers.assertExpectedResult('mutate_string_expected.js', mutated, true);
  });

  it('mutates some characters in identifiers, string literals and regexpes with pseudo-random behavior', () => {
    sandbox.restore();
    helpers.deterministicRandom(sandbox);

    const source = helpers.loadTestData('mutate_string.js');

    const settings = scriptMutator.defaultSettings();
    settings.MUTATE_UNICODE_ESCAPE_PROB = 0.2;
    settings.ENABLE_IDENTIFIER_UNICODE_ESCAPE = 1.0;
    settings.ENABLE_STRINGLITERAL_UNICODE_ESCAPE = 1.0;
    settings.ENABLE_REGEXPLITERAL_UNICODE_ESCAPE = 1.0;

    const mutator = new stringUnicode.StringUnicodeMutator(settings);
    mutator.mutate(source);

    const mutated = sourceHelpers.generateCode(source);

    helpers.assertExpectedResult('mutate_string_mixed_expected.js', mutated, true);
  });

  it('mutates all characters in string literals with unicode escapes', () => {
    sandbox.restore();
    helpers.deterministicRandom(sandbox);

    const source = helpers.loadTestData('mutate_unicode_string_literal.js');

    const settings = scriptMutator.defaultSettings();
    settings.MUTATE_UNICODE_ESCAPE_PROB = 1.0;
    settings.ENABLE_IDENTIFIER_UNICODE_ESCAPE = 0.0;
    settings.ENABLE_STRINGLITERAL_UNICODE_ESCAPE = 1.0;
    settings.ENABLE_REGEXPLITERAL_UNICODE_ESCAPE = 0.0;

    const mutator = new stringUnicode.StringUnicodeMutator(settings);
    mutator.mutate(source);

    const mutated = sourceHelpers.generateCode(source);

    helpers.assertExpectedResult('mutate_unicode_string_literal_expected.js', mutated, true);
  });


  it('mutates all characters in regexpes with unicode escapes', () => {
    sandbox.restore();
    helpers.deterministicRandom(sandbox);

    const source = helpers.loadTestData('mutate_unicode_regexp.js');

    const settings = scriptMutator.defaultSettings();
    settings.MUTATE_UNICODE_ESCAPE_PROB = 1.0;
    settings.ENABLE_IDENTIFIER_UNICODE_ESCAPE = 0.0;
    settings.ENABLE_STRINGLITERAL_UNICODE_ESCAPE = 0.0;
    settings.ENABLE_REGEXPLITERAL_UNICODE_ESCAPE = 1.0;

    const mutator = new stringUnicode.StringUnicodeMutator(settings);
    mutator.mutate(source);

    const mutated = sourceHelpers.generateCode(source);

    helpers.assertExpectedResult('mutate_regexp_expected.js', mutated, true);
  });

  it('does not mutate string when all mutation types are disabled', () => {
    sandbox.restore();

    const source = helpers.loadTestData('mutate_string.js');

    const settings = scriptMutator.defaultSettings();
    settings.MUTATE_UNICODE_ESCAPE_PROB = 0.0;
    settings.ENABLE_IDENTIFIER_UNICODE_ESCAPE = 0.0;
    settings.ENABLE_STRINGLITERAL_UNICODE_ESCAPE = 0.0;
    settings.ENABLE_REGEXPLITERAL_UNICODE_ESCAPE = 0.0;

    const mutator = new stringUnicode.StringUnicodeMutator(settings);
    mutator.mutate(source);

    const mutated = sourceHelpers.generateCode(source);

    helpers.assertExpectedResult('mutate_string_no_change_expected.js', mutated, false);
  });
});
