// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Numbers mutator.
 */

'use strict';

const babelTypes = require('@babel/types');

const arrayMutator = require('./array_mutator.js');
const common = require('./common.js');
const random = require('../random.js');
const mutator = require('./mutator.js');

const MIN_SAFE_INTEGER = -9007199254740991;
const MAX_SAFE_INTEGER = 9007199254740991;

// Large arrays cause too many mutations, which also wastes time in this
// visitor. Often, large arrays are output from Binaryen, which gets
// invalidated easily.
const SKIP_LARGE_ARRAYS_PROB = 0.9;


function isMethodOrObjectKey(path) {
  return (path.parent &&
          (babelTypes.isObjectMember(path.parent) ||
           babelTypes.isClassMethod(path.parent)) &&
          path.parent.key === path.node);
}

function isExponentiationBase(path) {
  return (path.parent &&
          babelTypes.isBinaryExpression(path.parent) &&
          path.parent.operator === '**' &&
          path.parent.left === path.node);
}

function createRandomNumber(value) {
  // TODO(ochang): Maybe replace with variable.
  const probability = random.random();
  if (probability < 0.01) {
    return babelTypes.numericLiteral(
        random.randInt(MIN_SAFE_INTEGER, MAX_SAFE_INTEGER));
  } else if (probability < 0.06) {
    return common.randomInterestingNumber();
  } else {
    return common.nearbyRandomNumber(value);
  }
}

class NumberMutator extends mutator.Mutator {
  constructor(settings) {
    super();
    this.settings = settings;
  }

  ignore(path) {
    return !random.choose(this.settings.MUTATE_NUMBERS) ||
           common.isInForLoopCondition(path) ||
           common.isInWhileLoop(path);
  }

  randomReplace(path, value, forcePositive=false) {
    const randomNumber = createRandomNumber(value);

    if (forcePositive) {
      randomNumber.value = Math.abs(randomNumber.value);
    }

    this.annotate(
        path.node,
        `Replaced ${value} with ${randomNumber.value}`);

    this.replaceWithSkip(path, randomNumber);
  }

  get visitor() {
    const thisMutator = this;

    return {
      ArrayExpression(path) {
        if (path.node.elements.length > arrayMutator.MAX_ARRAY_LENGTH &&
            random.choose(module.exports.SKIP_LARGE_ARRAYS_PROB)) {
          path.skip();
        }
      },
      NumericLiteral(path) {
        if (thisMutator.ignore(path)) {
          return;
        }

        // We handle negative unary expressions separately to replace the whole
        // expression below. E.g. -5 is UnaryExpression(-, NumericLiteral(5)).
        if (path.parent && babelTypes.isUnaryExpression(path.parent) &&
            path.parent.operator === '-') {
          return;
        }

        // Enfore positive numbers if the literal is the key of an object
        // property or method. Negative keys cause syntax errors.
        // TODO(389069288): We also enforce a positive base for
        // exponentiations as stand-alone negative numbers cause syntax errors.
        // We could support this case by constructing a negative number as an
        // UnaryExpression (with -) wrapping a number, which should get
        // Babel to parenthesize the expression.
        const forcePositive = (
            isMethodOrObjectKey(path) || isExponentiationBase(path));

        thisMutator.randomReplace(path, path.node.value, forcePositive);
      },
      UnaryExpression(path) {
        if (thisMutator.ignore(path)) {
          return;
        }

        // Handle the case we ignore above.
        if (path.node.operator === '-' &&
            babelTypes.isNumericLiteral(path.node.argument)) {
          thisMutator.randomReplace(path, -path.node.argument.value);
        }
      }
    };
  }
}

module.exports = {
  SKIP_LARGE_ARRAYS_PROB: SKIP_LARGE_ARRAYS_PROB,
  NumberMutator: NumberMutator,
};
