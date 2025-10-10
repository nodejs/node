// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Try catch wrapper.
 */

const assert = require('assert');
const babelTemplate = require('@babel/template').default;
const babelTypes = require('@babel/types');

const common = require('./common.js');
const mutator = require('./mutator.js');
const random = require('../random.js');

// Default target probability for skipping try-catch completely.
const DEFAULT_SKIP_PROB = 0.05;

// Default target probability to wrap only on toplevel, i.e. to not nest
// try-catch.
const DEFAULT_TOPLEVEL_PROB = 0.5;

// Probability to deviate from defaults and use extreme cases.
const IGNORE_DEFAULT_PROB = 0.03;

// We don't support 'using' and 'async using'. We wrap var with try-catch.
const WRAPPABLE_DECL_KINDS = new Set(['let', 'const']);

// This function is defined in resources/fuzz_library.js. The first version
// returns a permissive dummy on errors by default. The second version returns
// undefined.
const WRAP_FUN_PERMISSIVE = babelTemplate('__wrapTC(() => ID)');
const WRAP_FUN = babelTemplate('__wrapTC(() => ID, false)');

// Probability to use the permissive wrapper above.
const PERMISSIVE_WRAPPER_PROB = 0.9;

function isFunction(node) {
  return (babelTypes.isArrowFunctionExpression(node) ||
          babelTypes.isFunctionExpression(node) ||
          babelTypes.isFunctionDeclaration(node));
}

function isFunctionBody(path) {
  const parent = path.parent;
  return parent && isFunction(parent) && parent.body == path.node;
}

function hasFunctionDeclaration(body) {
  return body.some((node) => babelTypes.isFunctionDeclaration(node));
}

function _rawTryCatch(node, catchBlock=[]) {
  return babelTypes.tryStatement(
      node,
      babelTypes.catchClause(
          babelTypes.identifier('e'),
          babelTypes.blockStatement(catchBlock)));
}

function wrapTryCatch(node) {
  return _rawTryCatch(babelTypes.blockStatement([node]));
}

function wrapBlockWithTryCatch(block) {
  assert(block.body);
  assert(block.body.length);  // We don't wrap empty blocks.
  const lastStatement = block.body.at(-1);

  // If the block ended with the return of an object expression,
  // we also return an empty object expression in the catch block,
  // which reduces problems if the returned value is destructured.
  let catchBlock = [];
  if (babelTypes.isReturnStatement(lastStatement) &&
      lastStatement.argument &&
      babelTypes.isObjectExpression(lastStatement.argument)) {
    catchBlock.push(
        babelTypes.returnStatement(babelTypes.objectExpression([])));
  }
  return babelTypes.blockStatement([_rawTryCatch(block, catchBlock)]);
}

function skipReplaceVariableDeclarator(path) {
  return (
      // Uninitialized variable.
      !path.node.init ||
      // Simple initialization with a literal.
      babelTypes.isLiteral(path.node.init) ||
      // Wrapping a yield expression with an arrow function is
      // syntactically wrong.
      common.containsYield(path.node) ||
      // Initialization with undefined.
      (babelTypes.isIdentifier(path.node.init) &&
       path.node.init.name == 'undefined') ||
      // Consistency check.
      !babelTypes.isVariableDeclaration(path.parent) ||
      // Don't wrap variables in loop declarations.
      babelTypes.isLoop(path.parentPath.parent) ||
      // Only wrap supported kinds.
      !WRAPPABLE_DECL_KINDS.has(path.parent.kind))
}

function replaceVariableDeclarator(path) {
  let wrapped;
  let wrap_fun;
  if (random.choose(module.exports.PERMISSIVE_WRAPPER_PROB)) {
    wrap_fun = WRAP_FUN_PERMISSIVE;
  } else {
    wrap_fun = WRAP_FUN;
  }
  if (babelTypes.isAwaitExpression(path.node.init)) {
    // The await can't remain in the inner arrow function of wrap_fun,
    // we pull it outside of the wrapper instead. E.g.
    // "await x" becomes "await __wrapTC(() => x)".
    wrapped = babelTypes.AwaitExpression(
        wrap_fun({ID: path.node.init.argument}).expression);
  } else {
    wrapped = wrap_fun({ID: path.node.init}).expression;
  }
  path.replaceWith(babelTypes.variableDeclarator(path.node.id, wrapped));
  path.skip();
}

function replaceAndSkip(path) {
  if (!babelTypes.isLabeledStatement(path.parent) ||
      !babelTypes.isLoop(path.node)) {
    // Don't wrap loops with labels as it makes continue
    // statements syntactically invalid. We wrap the label
    // instead below.
    path.replaceWith(wrapTryCatch(path.node));
  }
  // Prevent infinite looping.
  path.skip();
}

function replaceBlockStatementAndSkip(path) {
  path.replaceWith(wrapBlockWithTryCatch(path.node));
  path.skip();
}

class AddTryCatchMutator extends mutator.Mutator {
  callWithProb(path, fun) {
    const probability = random.random();
    if (probability < this.skipProb * this.loc) {
      // Entirely skip try-catch wrapper.
      path.skip();
    } else if (probability < (this.skipProb + this.toplevelProb)) {
      // Only wrap on top-level.
      fun(path);
    }
  }

  get visitor() {
    const thisMutator = this;
    const accessStatement = {
      enter(path) {
        thisMutator.callWithProb(path, replaceAndSkip);
      },
      exit(path) {
        // Apply nested wrapping (is only executed if not skipped above).
        replaceAndSkip(path);
      }
    };
    const handleInfiniteLoops = {
      enter(path) {
        // Just replace the outer loop and don't progress on the inside if
        // it's an infinite loop, since sometimes exceptions are used to
        // break out of the loop.
        if (common.isInfiniteLoop(path.node)) {
          replaceAndSkip(path);
        }
      },
    };
    return {
      Program: {
        enter(path) {
          // Track original source location fraction in [0, 1).
          thisMutator.loc = 0;
          // Target probability for skipping try-catch.
          thisMutator.skipProb = module.exports.DEFAULT_SKIP_PROB;
          // Target probability for not nesting try-catch.
          thisMutator.toplevelProb = module.exports.DEFAULT_TOPLEVEL_PROB;
          // Maybe deviate from target probability for the entire test.
          if (random.choose(module.exports.IGNORE_DEFAULT_PROB)) {
            thisMutator.skipProb = random.uniform(0, 0.5);
            thisMutator.toplevelProb = random.uniform(0, 1);
            thisMutator.annotate(
                path.node,
                'Target skip probability ' + thisMutator.skipProb +
                ' and toplevel probability ' + thisMutator.toplevelProb);
          }
        }
      },
      Noop: {
        enter(path) {
          if (common.getSourceLoc(path.node)) {
            thisMutator.loc = common.getSourceLoc(path.node);
          }
        },
      },
      BlockStatement(path) {
        // It'd be superfluous to wrap existing try statements.
        if (path.parent && babelTypes.isTryStatement(path.parent)) {
          path.skip();
          return;
        }
        // Wrap the entire body of a function instead of descending into all
        // the function's nodes. This also includes return statements.
        // We only do this with the lower top-level wrapping probability and
        // don't provide an exit() function here. I.e. if we did descent into
        // the child nodes, we don't additionally wrap the body.
        // Don't wrap if there's a nested function declaration in the body as
        // this might break legacy scope compatibility rules.
        if (isFunctionBody(path) &&
            path.node.body &&
            path.node.body.length &&
            !hasFunctionDeclaration(path.node.body)) {
          thisMutator.callWithProb(path, replaceBlockStatementAndSkip);
        }
      },
      DoWhileStatement: handleInfiniteLoops,
      ExpressionStatement: accessStatement,
      IfStatement: accessStatement,
      LabeledStatement: {
        enter(path) {
          // Apply an extra try-catch around the label of a loop, since we
          // ignore the loop itself if it has a label.
          if (babelTypes.isLoop(path.node.body)) {
            thisMutator.callWithProb(path, replaceAndSkip);
          }
        },
        exit(path) {
          // Apply nested wrapping (is only executed if not skipped above).
          if (babelTypes.isLoop(path.node.body)) {
            replaceAndSkip(path);
          }
        },
      },
      // This covers {While|DoWhile|ForIn|ForOf|For}Statement.
      Loop: accessStatement,
      SwitchStatement: accessStatement,
      VariableDeclaration: {
        enter(path) {
          if (path.node.kind !== 'var' || babelTypes.isLoop(path.parent))
            return;
          thisMutator.callWithProb(path, replaceAndSkip);
        },
        exit(path) {
          if (path.node.kind !== 'var' || babelTypes.isLoop(path.parent))
            return;
          // Apply nested wrapping (is only executed if not skipped above).
          replaceAndSkip(path);
        }
      },
      VariableDeclarator: {
        enter(path) {
          if (skipReplaceVariableDeclarator(path))
            return;
          thisMutator.callWithProb(path, replaceVariableDeclarator);
        },
        exit(path) {
          if (skipReplaceVariableDeclarator(path))
            return;
          // Apply nested wrapping (is only executed if not skipped above).
          replaceVariableDeclarator(path);
        }
      },
      WhileStatement: handleInfiniteLoops,
      WithStatement: accessStatement,
    };
  }
}

module.exports = {
  DEFAULT_SKIP_PROB: DEFAULT_SKIP_PROB,
  DEFAULT_TOPLEVEL_PROB: DEFAULT_TOPLEVEL_PROB,
  IGNORE_DEFAULT_PROB: IGNORE_DEFAULT_PROB,
  PERMISSIVE_WRAPPER_PROB: PERMISSIVE_WRAPPER_PROB,
  AddTryCatchMutator: AddTryCatchMutator,
  wrapTryCatch: wrapTryCatch,
}
