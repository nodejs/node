// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Expression mutator.
 */

'use strict';

const babelTemplate = require('@babel/template').default;
const babelTraverse = require('@babel/traverse').default;
const babelTypes = require('@babel/types');
const babylon = require('@babel/parser');

const common = require('./common.js');
const random = require('../random.js');
const mutator = require('./mutator.js');
const sourceHelpers = require('../source_helpers.js');
const tryCatch = require('./try_catch.js');

const BABYLON_RELAXED_SUPER_OPTIONS = Object.assign(
    {}, sourceHelpers.BABYLON_OPTIONS);
BABYLON_RELAXED_SUPER_OPTIONS['allowSuperOutsideMethod'] = true;

/**
 * Check if a snippet containing `super` can be inserted at `path`
 * without creating a syntax error.
 *
 * Syntax errors would be raised if super is used within a class that
 * doesn't have a superclass or if a super() call is used outside of
 * a constructor. Member expressions like super.x are allowed.
 */
function validateSuper(path, source) {
  const ast = babylon.parse(source, BABYLON_RELAXED_SUPER_OPTIONS);

  // TODO(389069288): The DB currently only stores if a snippet contains
  // super. We could move this further differentiation into the DB.
  // But since it's rarely used, we just re-parse the snippet here on the fly
  // for now.
  // Furthermore: Some snippets might actually provide a self-contained
  // `super`, properly enclosed by an extending class. For these we could be
  // more permissive, but they are rare.
  let call = false;
  babelTraverse(ast, {
    Super(path) {
      if (babelTypes.isCallExpression(path.parent)) {
        call = true;
      }
    },
  });

  const surroundingMethod = path.findParent(x => x.isClassMethod());
  const surroundingClass = surroundingMethod.findParent(x => x.isClass());
  return surroundingClass && surroundingClass.node.superClass &&
         (!call || surroundingMethod.node.kind == 'constructor');
}

class CrossOverMutator extends mutator.Mutator {
  constructor(settings, db) {
    super(settings);
    this._db = db;
  }

  // For testing.
  db() {
    return this._db;
  }

  createInsertion(path, expression) {
    if (expression.needsSuper &&
        !validateSuper(path, expression.source)) {
      return undefined;
    }

    // Insert the statement.
    let toInsert = babelTemplate(
        expression.source,
        sourceHelpers.BABYLON_REPLACE_VAR_OPTIONS);
    const dependencies = {};
    const expressionDependencies = expression.dependencies;

    if (expressionDependencies) {
      const variables = common.availableVariables(path);
      if (variables.length < expressionDependencies.length) {
        return undefined;
      }
      const chosenVariables = random.sample(
          variables, expressionDependencies.length);
      for (const [index, dependency] of expressionDependencies.entries()) {
        dependencies[dependency] = chosenVariables[index];
      }
    }

    try {
      toInsert = toInsert(dependencies);
    } catch (e) {
      if (this.settings.testing) {
        // Fail early in tests.
        throw e;
      }
      console.log('ERROR: Failed to parse:', expression.source);
      console.log(e);
      return undefined;
    }

    if (expression.needsTryCatch) {
      toInsert = tryCatch.wrapTryCatch(toInsert);
    }

    this.annotate(
        toInsert,
        'Crossover from ' + expression.originalPath);

    return toInsert;
  }

  get visitor() {
    const thisMutator = this;

    return [{
      ForStatement(path) {
        // Cross-over insertions in large loops often lead to timeouts.
        if (common.isLargeLoop(path.node) &&
            random.choose(mutator.SKIP_LARGE_LOOP_MUTATION_PROB)) {
          path.skip();
        }
      },
      ExpressionStatement(path) {
        if (!random.choose(thisMutator.settings.MUTATE_CROSSOVER_INSERT)) {
          return;
        }

        const canHaveSuper = Boolean(path.findParent(x => x.isClassMethod()));
        const randomExpression = thisMutator.db().getRandomStatement(
            {canHaveSuper: canHaveSuper});

        const toInsert = thisMutator.createInsertion(path, randomExpression);
        if (!toInsert) return;

        if (random.choose(0.5)) {
          thisMutator.insertBeforeSkip(path, toInsert);
        } else {
          thisMutator.insertAfterSkip(path, toInsert);
        }

        path.skip();
      },
    }, {
    }];
  }
}

module.exports = {
  CrossOverMutator: CrossOverMutator,
};
