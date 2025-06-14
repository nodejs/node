// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Allocation-timeout mutator.
 */

'use strict';

const babelTemplate = require('@babel/template').default;
const babelTypes = require('@babel/types');

const common = require('./common.js');
const mutator = require('./mutator.js');
const random = require('../random.js');

const setAllocationTimeout = babelTemplate(
    '__callSetAllocationTimeout(TIMEOUT, INLINE);');

// Higher likelihood to insert before a function call.
const FUN_CALL_PROB_FACTOR = 2;

// Higher likelihood to insert if a function gets optimized.
const OPTIMIZED_FUNS_PROB_FACTOR = 3;

function isFunctionCall(path) {
  return (babelTypes.isCallExpression(path.node.expression) &&
          babelTypes.isIdentifier(path.node.expression.callee));
}

function isOptimizationCall(path) {
  return path.node.expression.callee.name.startsWith('__V8BuiltinOptimize');
}

class AllocationTimeoutMutator extends mutator.Mutator {

  setTimeout() {
    const id = babelTypes.identifier('__callSetAllocationTimeout');
    const timeout = babelTypes.numericLiteral(random.randInt(0, 3000));
    const inline = babelTypes.booleanLiteral(random.single([true, false]));
    const expression = babelTypes.expressionStatement(
      babelTypes.callExpression(id, [timeout, inline]));
    this.annotate(expression, 'Random timeout');
    return expression;
  }

  get visitor() {
    if (!random.choose(this.settings.ENABLE_ALLOCATION_TIMEOUT)) {
      return [];
    }
    const settings = this.settings;
    const thisMutator = this;

    return {
      ExpressionStatement(path) {
        let probability = settings.MUTATE_ALLOCATION_TIMEOUT;
        if (isFunctionCall(path)) {
          probability *= FUN_CALL_PROB_FACTOR;
          if (isOptimizationCall(path)) {
            probability *= OPTIMIZED_FUNS_PROB_FACTOR;
          }
        }

        if (!random.choose(probability)) {
          return;
        }

        // If an optimization runtime function is called, we want the
        // timeout afterwards (i.e. before calling the optimized function).
        if (isFunctionCall(path) && isOptimizationCall(path)) {
          thisMutator.insertAfterSkip(path, thisMutator.setTimeout());
        } else {
          thisMutator.insertBeforeSkip(path, thisMutator.setTimeout());
        }

        path.skip();
      },
      ForStatement(path) {
        // GCs in large loops often lead to timeouts.
        if (common.isLargeLoop(path.node) &&
            random.choose(mutator.SKIP_LARGE_LOOP_MUTATION_PROB)) {
          path.skip();
        }
      },
    };
  }
}

module.exports = {
  AllocationTimeoutMutator: AllocationTimeoutMutator,
};
