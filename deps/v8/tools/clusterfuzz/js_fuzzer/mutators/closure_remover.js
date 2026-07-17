// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Closure remover.
 *
 * This transforms top-level parameterless closures and inlines their code
 * into the enclosing program scope. Such code is more accessible to subsequent
 * mutations.
 */

const assert = require('assert');
const babelTemplate = require('@babel/template').default;
const babelTypes = require('@babel/types');

const common = require('./common.js');
const mutator = require('./mutator.js');
const random = require('../random.js');

/**
 * Returns true if this function expression can be transformed.
 */
function canTransformFunctionExpression(node, referencedFunctionIds) {
  return (
      // Needs to be a kind of function expression.
      (babelTypes.isFunctionExpression(node) ||
       babelTypes.isArrowFunctionExpression(node)) &&
      // Can't contain return statements. We mark these during the first
      // pass of the analysis.
      !node.__returns &&
      // We just don't support these.
      !node.async &&
      !node.expression &&
      !node.generator &&
      // Can't have parameters.
      node.params.length == 0 &&
      // Is either anonymous or doesn't reference its identifier. We track
      // references during the first pass of the analysis.
      (!node.id ||
       (babelTypes.isIdentifier(node.id) &&
        !referencedFunctionIds.has(node.id.name))) &&
      // Has a block body.
      node.body &&
      babelTypes.isBlockStatement(node.body) &&
      // The block is not empty.
      node.body.body &&
      node.body.body.length
  );
}

/**
 * Returns true if this closure can be transformed.
 */
function canTransformClosure(path, referencedFunctionIds) {
  return (
      // We only transform top-level nodes.
      path.parentPath.isProgram() &&
      // Expect a call-expression statement.
      path.node.expression &&
      babelTypes.isCallExpression(path.node.expression) &&
      // Can't pass arguments.
      path.node.expression.arguments.length == 0 &&
      // Must call an inlined function expression.
      path.node.expression.callee &&
      canTransformFunctionExpression(
          path.node.expression.callee, referencedFunctionIds)
  );
}

// We access the body assuming it has been checked before with
// `isRemovableFunctionExpression`.
function getFunctionBody(path) {
  return path.node.expression.callee.body.body;
}

class ClosureRemover extends mutator.Mutator {
  get visitor() {
    if (!random.choose(this.settings.TRANSFORM_CLOSURES)) {
      return [];
    }

    const thisMutator = this;
    const referencedFunctionIds = new Set();
    const functionStack = [];
    let transformationCount = 0;

    // Maintain function expression stack. During the traversal, return
    // statements belong to the function on the top of the stack.
    const functionExpression = {
      enter(path) {
        functionStack.push(path.node);
      },
      exit(path) {
        const node = functionStack.pop()
        assert(babelTypes.isFunctionExpression(node) ||
               babelTypes.isArrowFunctionExpression(node));
      }
    };

    return [{
      // First pass to mark functions with return statements and to mark
      // referenced function identifiers.
      ArrowFunctionExpression: functionExpression,
      FunctionExpression: functionExpression,
      Identifier(path) {
        if (path.isReferencedIdentifier() &&
            common.isFunctionIdentifier(path.node.name)) {
          referencedFunctionIds.add(path.node.name);
        }
      },
      ReturnStatement(path) {
        const currentFunction = functionStack.at(-1);
        if (currentFunction) {
          currentFunction.__returns = true;
        }
        path.skip();
      }
    }, {
      // Second pass to transform closures.
      ExpressionStatement(path) {
        if (!canTransformClosure(path, referencedFunctionIds)) {
          path.skip();
          return;
        }
        for (const node of getFunctionBody(path)) {
          path.insertBefore(node);
        }
        path.remove();
        transformationCount++;
      },
      Program: {
        exit(path) {
          thisMutator.annotate(
              path.node,
              `Transformed ${transformationCount} closures.`);
        }
      }
    }];
  }
}

module.exports = {
  ClosureRemover: ClosureRemover,
}
