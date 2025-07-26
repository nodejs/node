// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Validate all the snippets in the DB by executing them with
 * a test file. The new, validated index can replace the existing one with
 * hardened assumptions.
 */

const fs = require('fs');
const fsPath = require('path');
const program = require('commander');
const tempfile = require('tempfile');

const { execSync } = require("child_process");

const babelTraverse = require('@babel/traverse').default;
const babelTypes = require('@babel/types');

const corpus = require('./corpus.js');
const db = require('./db.js');
const sourceHelpers = require('./source_helpers.js');

const { CrossOverMutator } = require('./mutators/crossover_mutator.js');

/**
 * Return templates to test the expression on.
 */
function templates(expression) {
  if (expression.needsSuper) {
    return [
      'cross_over_template_class_constructor.js',
      'cross_over_template_class_method.js',
    ];
  } else {
    return [
      'cross_over_template_sloppy.js',
      'cross_over_template_strict.js',
    ];
  }
}

/**
 * Execute code and return the error message on failure or undefined on
 * success.
 */
function execute(code) {
  const output_file = tempfile('.js');
  fs.writeFileSync(output_file, code);
  try {
    execSync("node " + output_file, {stdio: ['pipe']});
    return undefined;
  } catch(e) {
    return e.message;
  }
}

/**
 * Insert an expression into a template, generate and execute the resulting
 * code. Return the error message on failure or undefined on success.
 */
function validate(crossover, tmplName, expression) {
  const tmpl = sourceHelpers.loadSource(
    sourceHelpers.BASE_CORPUS, fsPath.join('resources', tmplName));
  try {
    let done = false;
    babelTraverse(tmpl.ast, {
      ExpressionStatement(path) {
        if (done || !path.node.expression ||
            !babelTypes.isCallExpression(path.node.expression)) {
          return;
        }
        // Avoid infinite loops if there's an expression statement in the
        // inserted expression.
        done = true;
        path.insertAfter(crossover.createInsertion(path, expression));
      }
    });
  } catch (e) {
    console.log(`Could not validate expression: ${expression.source}`);
    return 'SyntaxError';
  }
  return execute(sourceHelpers.generateCode(tmpl, []));
}

function main() {
  program
    .version('0.0.1')
    .option('-i, --input_dir <path>', 'DB directory.')
    .option('-o, --output_index <path>', 'path to new index.')
    .parse(process.argv);

  if (!program.input_dir || !program.output_index) {
    console.log('Need to specify DB dir and output index.');
    return;
  }

  const mutateDb = new db.MutateDb(program.input_dir);
  const crossover = new CrossOverMutator(undefined, mutateDb);

  result = [];
  for (const record of mutateDb.all) {
    const expression = db.loadExpression(program.input_dir, record);

    let errors = false;
    let syntaxErrors = false;
    for (const tmplName of templates(expression)) {
      const error = validate(crossover, tmplName, expression);
      errors |= Boolean(error);
      syntaxErrors |= Boolean(error?.includes("SyntaxError"))
    }
    // Discard records causing syntax errors.
    if (syntaxErrors) {
      continue;
    }
    // Mark records that will require try-catch wrapping.
    if (errors) {
      record["tc"] = true;
    }
    result.push(record);
  }
  db.writeIndexFile(program.output_index, result);
}

main();
