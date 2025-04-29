// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Variables mutator.
 */

'use strict';

const babelTypes = require('@babel/types');

const common = require('./common.js');
const random = require('../random.js');
const mutator = require('./mutator.js');

function _isInFunctionParam(path) {
  const child = path.find(p => p.parent && babelTypes.isFunction(p.parent));
  return child && child.parentKey === 'params';
}

// Skip possible loop-variable mutations with a high probability.
const SKIP_LOOP_VAR_PROB = 0.95;

/**
 * Returns true if path appears on the left-hand side of a variable declarator.
 *
 * This also includes nesting:
 * let lhs_var = rhs_var;
 * let {prop: lhs_var} = rhs_var;
 */
function _isBeingDeclared(path) {
  const child = path.find(
      p => p.parent && babelTypes.isVariableDeclarator(p.parent));
  return child && child.parent.id == child.node;
}

class VariableMutator extends mutator.Mutator {
  get visitor() {
    const thisMutator = this;

    return {
      AssignmentExpression(path) {
        // Don't change assignments to loop variables, also outside of loop
        // conditions.
        if (babelTypes.isIdentifier(path.node.left) &&
            common.isVariableIdentifier(path.node.left.name) &&
            thisMutator.context.loopVariables.has(path.node.left.name) &&
            random.choose(module.exports.SKIP_LOOP_VAR_PROB)) {
          path.skip();
        }
      },
      Identifier(path) {
        if (!random.choose(thisMutator.settings.MUTATE_VARIABLES)) {
          return;
        }

        if (!common.isVariableIdentifier(path.node.name)) {
          return;
        }

        // Don't mutate variables on the left-hand side of a declaration.
        if (_isBeingDeclared(path)) {
          return;
        }

        // Don't mutate function params.
        if (_isInFunctionParam(path)) {
          return;
        }

        if (common.isInForLoopCondition(path) ||
            common.isInWhileLoop(path)) {
          return;
        }

        const randVar = common.randomVariable(path);
        if (!randVar) {
          return;
        }

        const newName = randVar.name;

        // Don't assign to loop variables.
        if (babelTypes.isAssignmentExpression(path.parent) &&
            path.parent.left == path.node &&
            thisMutator.context.loopVariables.has(newName) &&
            random.choose(module.exports.SKIP_LOOP_VAR_PROB)) {
          return;
        }

        thisMutator.annotate(
            path.node,
            `Replaced ${path.node.name} with ${newName}`);
        path.node.name = newName;
      }
    };
  }
}

module.exports = {
  SKIP_LOOP_VAR_PROB: SKIP_LOOP_VAR_PROB,
  VariableMutator: VariableMutator,
};
