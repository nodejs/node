// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Mutator for differential fuzzing.
 */

'use strict';

const babelTemplate = require('@babel/template').default;
const babelTypes = require('@babel/types');

const common = require('./common.js');
const mutator = require('./mutator.js');
const random = require('../random.js');

// Templates for various statements.
const incCaught = babelTemplate('__caught++;');
const printValue = babelTemplate('print(VALUE);');
const printCaught = babelTemplate('print("Caught: " + __caught);');
const printHash = babelTemplate('print("Hash: " + __hash);');
const prettyPrint = babelTemplate('__prettyPrint(ID);');
const prettyPrintExtra = babelTemplate('__prettyPrintExtra(ID);');
const prettyPrintExtraWrap = babelTemplate('__prettyPrintExtra(ID)');

// This section prefix is expected by v8_foozzie.py. Existing prefixes
// (e.g. from CrashTests) are cleaned up with CLEANED_PREFIX.
const SECTION_PREFIX = 'v8-foozzie source: ';
const CLEANED_PREFIX = 'v***************e: ';

/**
 * Babel statement for calling deep printing from the fuzz library.
 */
function prettyPrintStatement(variable) {
  return prettyPrint({ ID: babelTypes.cloneDeep(variable) });
}

/**
 * As above, but using the "extra" variant, which will reduce printing
 * after too many calls to prevent I/O flooding.
 */
function prettyPrintExtraStatement(variable) {
  return prettyPrintExtra({ ID: babelTypes.cloneDeep(variable) });
}

/**
 * As above, but wrapping an expression.
 */
function wrapPrettyPrintExtra(inner) {
  return prettyPrintExtraWrap({ID: inner}).expression;
}

/**
 * Returns the last node in a block body after which other code can be
 * inserted. I.e. skips over return statements and similar.
 */
function findInsertNodeInBlock(body) {
  return body.findLast(node =>
      !babelTypes.isReturnStatement(node) &&
      !babelTypes.isBreakStatement(node) &&
      !babelTypes.isContinueStatement(node) &&
      !babelTypes.isThrowStatement(node));
}

/**
 * Mutator for suppressing known and/or unfixable issues.
 */
class DifferentialFuzzSuppressions extends mutator.Mutator {
  get visitor() {
    let thisMutator = this;

    return {
      // Clean up strings containing the magic section prefix. Those can come
      // e.g. from CrashTests and would confuse the deduplication in
      // v8_foozzie.py.
      StringLiteral(path) {
        if (path.node.value.startsWith(SECTION_PREFIX)) {
          const postfix = path.node.value.substring(SECTION_PREFIX.length);
          path.node.value = CLEANED_PREFIX + postfix;
          thisMutator.annotate(path.node, 'Replaced magic string');
        }
      },
      // Known precision differences: https://crbug.com/417981349
      AssignmentExpression(path) {
        if (path.node.operator == '**=') {
          path.node.operator = '+=';
          thisMutator.annotate(path.node, 'Replaced **=');
        }
      },
      // Known precision differences: https://crbug.com/380147861
      BinaryExpression(path) {
        if (path.node.operator == '**') {
          path.node.operator = '+';
          thisMutator.annotate(path.node, 'Replaced **');
        }
      },
      // Unsupported language feature: https://crbug.com/1020573
      MemberExpression(path) {
        if (path.node.property.name == "arguments") {
          let replacement = common.randomVariable(path);
          if (!replacement) {
            replacement = babelTypes.thisExpression();
          }
          thisMutator.annotate(replacement, 'Replaced .arguments');
          thisMutator.replaceWithSkip(path, replacement);
        }
      },
    };
  }
}

/**
 * Mutator for tracking original input files and for extra printing.
 */
class DifferentialFuzzMutator extends mutator.Mutator {
  constructor(settings) {
    super();
    this.settings = settings;
  }

  /**
   * Looks for the dummy node that marks the beginning of an input file
   * from the corpus.
   */
  isSectionStart(path) {
    return !!common.getOriginalPath(path.node);
  }

  /**
   * Create print statements for printing the magic section prefix that's
   * expected by v8_foozzie.py to differentiate different source files.
   */
  getSectionHeader(path) {
    const orig = common.getOriginalPath(path.node);
    return printValue({
      VALUE: babelTypes.stringLiteral(SECTION_PREFIX + orig),
    });
  }

  /**
   * Create statements for extra printing at the end of a section. We print
   * the number of caught exceptions, a generic hash of all observed values
   * and the contents of all variables in scope.
   */
  getSectionFooter(path) {
    const variables = common.availableVariables(path);
    const statements = variables.map(prettyPrintStatement);
    statements.unshift(printCaught());
    statements.unshift(printHash());
    const statement = babelTypes.tryStatement(
        babelTypes.blockStatement(statements),
        babelTypes.catchClause(
            babelTypes.identifier('e'),
            babelTypes.blockStatement([])));
    return statement;
  }

  /**
   * Helper for printing the contents of several variables.
   */
  printVariables(path, nodes) {
    const statements = [];
    for (const node of nodes) {
      if (!babelTypes.isIdentifier(node) ||
          !common.isVariableIdentifier(node.name))
        continue;
      statements.push(prettyPrintExtraStatement(node));
    }
    if (statements.length) {
      this.insertAfterSkip(path, statements);
    }
  }

  /**
   * Helper for printing the variables known by name.
   */
  printVariablesByName(path, variables) {
    const statements = [];
    for (const v of variables) {
      if (!common.isVariableIdentifier(v)) continue;
      statements.push(prettyPrintExtraStatement(babelTypes.identifier(v)));
    }
    if (statements.length) {
      this.insertAfterSkip(path, statements);
    }
  }

  get visitor() {
    const thisMutator = this;
    const settings = this.settings;

    // In complex functions the probability is quite high that we're going to
    // insert side effects somewhere. Therefore we skip the entire function
    // with some probability to ensure we keep some untouched functions.
    const maybeSkipFunctions = {
      enter(path) {
        if (random.choose(settings.DIFF_FUZZ_SKIP_FUNCTIONS)) {
          path.skip()
        }
      },
    };

    return {
      // Print more at the end of blocks.
      BlockScoped(path) {
        // Ignore top-level program and loop headers.
        if (!path.parent ||
            !babelTypes.isBlockStatement(path.parent)) {
          return;
        }

        if (!random.choose(settings.DIFF_FUZZ_BLOCK_PRINT)) {
          return;
        }

        // Skip if there are no interesting variable bindings in this block.
        const scopeBindings = Object.keys(path.scope.bindings);
        if (!scopeBindings.length) {
          return;
        }

        // Skip if there's no position to insert code to.
        const insertNode = findInsertNodeInBlock(path.parent.body);
        if (!insertNode) {
          return;
        }

        // Skip if we for some reason can't find the AST path to the node we
        // want to insert after.
        let insertPath = undefined;
        path.parentPath.get('body').forEach((child) => {
          if (child.node == insertNode) insertPath = child;
        });
        if (!insertPath) {
          return;
        }

        // Ensure the variables to print are available at the position for
        // printing.
        const bindings = [];
        const vars = common.availableVariableNames(insertPath);
        for (const v of scopeBindings) {
          if (vars.has(v)) {
            bindings.push(v);
          }
        }
        if (!bindings.length) {
          return;
        }

        thisMutator.printVariablesByName(insertPath, bindings);
      },
      // Replace existing print statements and print at call sites.
      CallExpression: {
        exit(path) {
          if (babelTypes.isIdentifier(path.node.callee) &&
              path.node.callee.name == 'print') {
            path.node.callee = babelTypes.identifier('__prettyPrintExtra');
          return;
          }
          // Don't try to wrap existing print functions or %Optimize* builtins
          // and similar.
          if (babelTypes.isIdentifier(path.node.callee) && (
              path.node.callee.name.startsWith('__V8Builtin') ||
              path.node.callee.name.startsWith('__prettyPrint'))) {
            return;
          }
          if (!random.choose(settings.DIFF_FUZZ_EXTRA_PRINT)) {
            return;
          }
          thisMutator.replaceWithSkip(path, wrapPrettyPrintExtra(path.node));
        },
      },
      // Either print or track caught exceptions, guarded by a probability.
      CatchClause(path) {
        const probability = random.random();
        if (probability < settings.DIFF_FUZZ_EXTRA_PRINT &&
            path.node.param &&
            babelTypes.isIdentifier(path.node.param)) {
          const statement = prettyPrintExtraStatement(path.node.param);
          path.node.body.body.unshift(statement);
        } else if (probability < settings.DIFF_FUZZ_TRACK_CAUGHT) {
          path.node.body.body.unshift(incCaught());
        }
      },
      // Insert section headers and footers between the contents of two
      // original source files. We detect the dummy no-op nodes that were
      // previously tagged with the original path of the file.
      Noop(path) {
        if (!thisMutator.isSectionStart(path)) {
          return;
        }
        const header = thisMutator.getSectionHeader(path);
        const footer = thisMutator.getSectionFooter(path);
        thisMutator.insertBeforeSkip(path, footer);
        thisMutator.insertBeforeSkip(path, header);
      },
      // Additionally we print one footer in the end.
      Program: {
        exit(path) {
          const footer = thisMutator.getSectionFooter(path);
          path.node.body.push(footer);
        },
      },
      // Print contents of variables after assignments, guarded by a
      // probability.
      ExpressionStatement(path) {
        if (!babelTypes.isAssignmentExpression(path.node.expression) ||
            !random.choose(settings.DIFF_FUZZ_EXTRA_PRINT)) {
          return;
        }
        const left = path.node.expression.left;
        if (babelTypes.isMemberExpression(left)) {
          thisMutator.printVariables(path, [left.object]);
        } else {
          thisMutator.printVariables(path, [left]);
        }
      },
      FunctionDeclaration: maybeSkipFunctions,
      FunctionExpression: maybeSkipFunctions,
      ArrowFunctionExpression: maybeSkipFunctions,
      // Print contents of variables after declaration, guarded by a
      // probability.
      VariableDeclaration(path) {
        if (babelTypes.isLoop(path.parent) ||
            !random.choose(settings.DIFF_FUZZ_EXTRA_PRINT)) {
          return;
        }
        const identifiers = path.node.declarations.map(decl => decl.id);
        thisMutator.printVariables(path, identifiers);
      },
    };
  }
}

module.exports = {
  DifferentialFuzzMutator: DifferentialFuzzMutator,
  DifferentialFuzzSuppressions: DifferentialFuzzSuppressions,
};
