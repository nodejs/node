// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for mutating variables
 */

'use strict';

const babelTraverse = require('@babel/traverse').default;
const sinon = require('sinon');

const common = require('../mutators/common.js');
const helpers = require('./helpers.js');

const sandbox = sinon.createSandbox();

describe('Available variables and functions', () => {
  afterEach(() => {
    sandbox.restore();
  });

  function testAvailableIdentifiers(input, expected) {
    const source = helpers.loadTestData(input);
    const result = new Array();

    babelTraverse(source.ast, {
      CallExpression(path) {
        result.push({
          variables: common.availableVariables(path),
          functions: common.availableFunctions(path),
        });
      }
    });

    helpers.assertExpectedResult(
        expected, JSON.stringify(result, null, 2));
  }

  it('test with functions and strict execution order', () => {
    sandbox.stub(common, 'STRICT_EXECUTION_ORDER_PROB').value(1.0);
    testAvailableIdentifiers(
        'available_variables.js', 'available_variables_expected.js');
  });

  it('test with functions and sloppy execution order', () => {
    sandbox.stub(common, 'STRICT_EXECUTION_ORDER_PROB').value(0.0);
    testAvailableIdentifiers(
        'available_variables.js',
        'available_variables_sloppy_expected.js');
  });

  it('test with function expressions', () => {
    sandbox.stub(common, 'STRICT_EXECUTION_ORDER_PROB').value(0.0);
    testAvailableIdentifiers(
        'available_variables_fun_exp.js',
        'available_variables_fun_exp_expected.js');
  });

  it('test with arrow function expressions', () => {
    sandbox.stub(common, 'STRICT_EXECUTION_ORDER_PROB').value(0.0);
    testAvailableIdentifiers(
        'available_variables_arrow_fun_exp.js',
        'available_variables_arrow_fun_exp_expected.js');
  });
});
