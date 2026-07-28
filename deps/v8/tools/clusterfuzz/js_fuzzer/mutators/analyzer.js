// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Analyzer.
 * Mutator that acquires additional information used in subsequent mutators.
 * This mutator doesn't change the structure, but only marks nodes with
 * meta information.
 */

'use strict';

const assert = require('assert');
const babelTypes = require('@babel/types');

const common = require('./common.js');
const mutator = require('./mutator.js');

// Additional resource needed if the AsyncIterator ID is found in the code.
const ASYNC_ITERATOR_ID = 'AsyncIterator'
const ASYNC_ITERATOR_RESOURCE = 'async_iterator.js'

// State for keeping track of loops and loop bodies during traversal.
const LoopState = Object.freeze({
    LOOP:   Symbol("loop"),
    BLOCK:  Symbol("block"),
});

// We consider loops with more iterations than this as large. This can be used
// to reduce some mutations in such loops.
const LARGE_LOOP_THRESHOLD = 1000;

/**
 * Combine multiple enter/exit visitor pairs. Also single enter or exit
 * methods are possible.
 */
function compound(...visitors) {
  const enters = [];
  const exits = [];
  for (const visitor of visitors) {
    if ("enter" in visitor) enters.push(visitor.enter);
    if ("exit" in visitor) exits.push(visitor.exit);
  }
  return {
    enter: enters,
    // Exit in reverse order so that the corresponding enter/exit pairs
    // operate on a stack.
    exit: exits.reverse(),
  };
}

/**
 * Returns true for numeric values with or without an unary minus compound.
 */
function isNumber(node) {
  return (
      (
          // Direct valid number literal.
          babelTypes.isLiteral(node) &&
          !isNaN(node.value)
      ) ||
      (
          // A number literal wrapped with an unary minus.
          babelTypes.isUnaryExpression(node) &&
          node.operator === '-' &&
          babelTypes.isLiteral(node.argument) &&
          !isNaN(node.argument.value)
      )
  );
}

/**
 * Extracts the numeric value from a node. We assume it is a number as
 * determined by the function above.
 */
function getNumber(node) {
  // Direct valid number literal.
  if (babelTypes.isLiteral(node)) {
    assert(!isNaN(node.value));
    return node.value;
  }
  // A number literal wrapped with an unary minus.
  assert(babelTypes.isUnaryExpression(node));
  assert(node.operator === '-');
  assert(babelTypes.isLiteral(node.argument));
  assert(!isNaN(node.argument.value));
  return -node.argument.value;
}

/**
 * Resolve two nodes in any order as [identifier, number] if possible,
 * [undefined, undefined] otherwise.
 */
function idAndNumber(node1, node2) {
  if(!node1 || !node2) {
    return [undefined, undefined];
  }
  if (babelTypes.isIdentifier(node1) && isNumber(node2)) {
    return [node1.name, getNumber(node2)];
  }
  if (isNumber(node1) && babelTypes.isIdentifier(node2)) {
    return [node2.name, getNumber(node1)];
  }
  return [undefined, undefined];
}

/**
 * Return true if we approximate this node as a large for-loop. We
 * heuristically look at simple loop initializers and conditions only.
 *
 * As this is only an approximation, it will determine some large loops
 * as small and vice versa. Hence the result of this should only be used
 * for soft logical changes during fuzzing, e.g. behind a probability.
 */
function approximateLargeForLoop(node) {
  assert(babelTypes.isForStatement(node));

  // The initializer must be a single declaration of the form:
  // `{var|let|const} {decl}`.
  if (!node.init ||
      !babelTypes.isVariableDeclaration(node.init) ||
      node.init.declarations.length != 1) {
    return false;
  }

  // The declaration must assign a number to an identifier: `id = num`.
  const decl = node.init.declarations[0];
  if (!decl ||
      !babelTypes.isVariableDeclarator(decl) ||
      !babelTypes.isIdentifier(decl.id) ||
      !decl.init ||
      !isNumber(decl.init)) {
    return false;
  }

  // The test part must be a single binary expression. We don't care which
  // operator, e.g. `A < B`.
  const initID = decl.id.name;
  const initNumber = getNumber(decl.init);
  if (!node.test || !babelTypes.isBinaryExpression(node.test)) {
    return false;
  }

  // The two operants must be an identifier and a number in any order. E.g.:
  // `id < num`.
  const [testID, testNumber] = idAndNumber(node.test.left, node.test.right);
  if (testID === undefined || testNumber === undefined) {
    return false;
  }

  // If we test the same identifier we initialized and if the numeric
  // difference is large enough, we approximate this as large. We ignore
  // increments.
  return (
      initID === testID &&
      Math.abs(initNumber - testNumber) >= LARGE_LOOP_THRESHOLD);
}


class ContextAnalyzer extends mutator.Mutator {
  constructor() {
    super();
    this.loopStack = [];
  }

  /**
   * Return true if the traversal is currently within a loop test part.
   */
  isLoopTest() {
    return this.loopStack.at(-1) === LoopState.LOOP;
  }

  get visitor() {
    const thisMutator = this;
    const loopStack = this.loopStack;

    // Maintain the state if we're in a loop.
    const loopStatement = {
      enter(path) {
        loopStack.push(LoopState.LOOP);
      },
      exit(path) {
        assert(loopStack.pop() === LoopState.LOOP);
      }
    };

    // Mark functions that contain empty infinite loops.
    const emptyInfiniteLoop = {
      enter(path) {
        if (common.isInfiniteLoop(path.node) &&
            path.node.body &&
            babelTypes.isBlockStatement(path.node.body) &&
            !path.node.body.body.length) {
          const fun = path.findParent((p) => p.isFunctionDeclaration());
          if (fun && fun.node && fun.node.id &&
              babelTypes.isIdentifier(fun.node.id) &&
              common.isFunctionIdentifier(fun.node.id.name)) {
            thisMutator.context.infiniteFunctions.add(fun.node.id.name);
          }
        }
      },
    };

    const largeForLoop = {
      enter(path) {
        if (approximateLargeForLoop(path.node)) {
          common.setLargeLoop(path.node);
        }
      },
    };

    return {
      Identifier(path) {
        if (path.node.name === ASYNC_ITERATOR_ID) {
          thisMutator.context.extraResources.add(ASYNC_ITERATOR_RESOURCE);
        }
        if (thisMutator.isLoopTest() &&
            common.isVariableIdentifier(path.node.name)) {
          thisMutator.context.loopVariables.add(path.node.name);
        }
      },
      FunctionDeclaration(path) {
        if (path.node.id && babelTypes.isIdentifier(path.node.id)) {
          if (common.isFunctionIdentifier(path.node.id.name) &&
              path.node.async) {
            thisMutator.context.asyncFunctions.add(path.node.id.name);
          }
        }
      },
      WhileStatement: compound(loopStatement, emptyInfiniteLoop),
      DoWhileStatement: compound(loopStatement, emptyInfiniteLoop),
      ForStatement: compound(loopStatement, largeForLoop),
      BlockStatement: {
        enter(path) {
          loopStack.push(LoopState.BLOCK);
        },
        exit(path) {
          assert(loopStack.pop() === LoopState.BLOCK);
        }
      },
      // Mark nodes containing a yield expression up to the encolsing
      // generator function declaration.
      YieldExpression(path) {
        for (let parent = path.parentPath;
             parent && !parent.isFunctionDeclaration();
             parent = parent.parentPath) {
          common.setContainsYield(parent.node);
        }
      },
    };
  }
}

module.exports = {
  ContextAnalyzer: ContextAnalyzer,
};
