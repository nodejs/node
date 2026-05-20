// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Function calls mutator.
 */

'use strict';

const babelTemplate = require('@babel/template').default;
const babelTypes = require('@babel/types');

const common = require('./common.js');
const random = require('../random.js');
const mutator = require('./mutator.js');

const REPLACE_CROSS_ASYNC_PROB = 0.2;

function _liftExpressionsToStatements(path, nodes) {
  // If the node we're replacing is an expression in an expression statement,
  // lift the replacement nodes into statements too.
  if (!babelTypes.isExpressionStatement(path.parent)) {
    return nodes;
  }

  return nodes.map(n => babelTypes.expressionStatement(n));
}

/**
 * All functions in the path's scope that keep the same async property.
 * A different property (e.g. sync to async) is used only with a small
 * probability. Additionally this doesn't include the current function.
 */
function availableReplacementFunctionNames(path, context) {
  const available = common.availableFunctionNames(path);
  const oldFunc = path.node.callee.name;

  // The available set is a fresh set which we can mutate here.
  available.delete(oldFunc);

  // Disregard infinitely running functions.
  for (const name of context.infiniteFunctions.values()) {
    available.delete(name);
  }

  // If we don't care to maintain the async property, just use all functions.
  if (random.choose(module.exports.REPLACE_CROSS_ASYNC_PROB)) {
    return Array.from(available.values());
  }

  // Return an array with functions of the same async property as the current.
  const oldIsAsync = context.asyncFunctions.has(oldFunc);
  let result = new Array();
  for (const name of available.values()) {
    if (oldIsAsync == context.asyncFunctions.has(name)) {
      result.push(name);
    }
  }
  return result;
}

class FunctionCallMutator extends mutator.Mutator {
  get visitor() {
    const thisMutator = this;

    return {
      CallExpression(path) {
        if (!babelTypes.isIdentifier(path.node.callee)) {
          return;
        }

        if (!common.isFunctionIdentifier(path.node.callee.name)) {
          return;
        }

        if (!random.choose(thisMutator.settings.MUTATE_FUNCTION_CALLS)) {
          return;
        }

        const probability = random.random();
        if (probability < 0.3) {
          const replacement = random.single(availableReplacementFunctionNames(
              path, thisMutator.context));
          if (replacement) {
            thisMutator.annotate(
                path.node,
                `Replaced ${path.node.callee.name} with ${replacement}`);

            path.node.callee = babelTypes.identifier(replacement);
          }
        } else if (probability < 0.7 && thisMutator.settings.engine == 'v8') {
          const prepareTemplate = babelTemplate(
              '__V8BuiltinPrepareFunctionForOptimization(ID)');
          const optimizationMode = random.choose(0.7) ? 'Function' : 'Maglev';
          const optimizeTemplate = babelTemplate(
              `__V8BuiltinOptimize${optimizationMode}OnNextCall(ID)`);

          const nodes = [
              prepareTemplate({
                ID: babelTypes.cloneDeep(path.node.callee),
              }).expression,
              babelTypes.cloneDeep(path.node),
              babelTypes.cloneDeep(path.node),
              optimizeTemplate({
                ID: babelTypes.cloneDeep(path.node.callee),
              }).expression,
          ];

          thisMutator.annotate(
              path.node,
              `Optimizing ${path.node.callee.name}`);
          if (!babelTypes.isExpressionStatement(path.parent)) {
            nodes.push(path.node);
            thisMutator.replaceWithSkip(
                path, babelTypes.sequenceExpression(nodes));
          } else {
            thisMutator.insertBeforeSkip(
                path, _liftExpressionsToStatements(path, nodes));
          }
        } else if (probability < 0.8 && thisMutator.settings.engine == 'v8') {
          const template = babelTemplate(
              '__V8BuiltinCompileBaseline(ID)');

          const nodes = [
              template({
                ID: babelTypes.cloneDeep(path.node.callee),
              }).expression,
          ];

          thisMutator.annotate(
              nodes[0],
              `Compiling baseline ${path.node.callee.name}`);

          if (!babelTypes.isExpressionStatement(path.parent)) {
            nodes.push(path.node);
            thisMutator.replaceWithSkip(
                path, babelTypes.sequenceExpression(nodes));
          } else {
            thisMutator.insertBeforeSkip(
                path, _liftExpressionsToStatements(path, nodes));
          }
        } else if (probability < 0.9 &&
                   thisMutator.settings.engine == 'v8') {
          const template = babelTemplate(
              '__V8BuiltinDeoptimizeFunction(ID)');
          const insert = _liftExpressionsToStatements(path, [
              template({
                ID: babelTypes.cloneDeep(path.node.callee),
              }).expression,
          ]);

          thisMutator.annotate(
              path.node,
              `Deoptimizing ${path.node.callee.name}`);

          thisMutator.insertAfterSkip(path, insert);
        } else {
          const template = babelTemplate(
              'runNearStackLimit(() => { return CALL });');
          thisMutator.annotate(
              path.node,
              `Run to stack limit ${path.node.callee.name}`);

          thisMutator.replaceWithSkip(
              path,
              template({
                CALL: path.node,
              }).expression);
        }

        path.skip();
      },
    }
  }
}

module.exports = {
  REPLACE_CROSS_ASYNC_PROB: REPLACE_CROSS_ASYNC_PROB,
  FunctionCallMutator: FunctionCallMutator,
};
