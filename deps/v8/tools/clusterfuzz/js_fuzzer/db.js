// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Mutation Db.
 */

const assert = require('assert');
const crypto = require('crypto');
const fs = require('fs');
const fsPath = require('path');

const babelGenerator = require('@babel/generator').default;
const babelTemplate = require('@babel/template').default;
const babelTraverse = require('@babel/traverse').default;
const babelTypes = require('@babel/types');
const globals = require('globals');

const random = require('./random.js');
const sourceHelpers = require('./source_helpers.js');

// The probabiliy to choose a snippet with the super keyword in places
// where it can be inserted.
const CHOOSE_SUPER_PROB = 0.2;

const MAX_SNIPPET_SIZE = 64;

const globalIdentifiers = new Set(Object.keys(globals.builtin));
const propertyNames = new Set([
    // Parsed from https://github.com/tc39/ecma262/blob/master/spec.html
    'add',
    'anchor',
    'apply',
    'big',
    'bind',
    'blink',
    'bold',
    'buffer',
    'byteLength',
    'byteOffset',
    'BYTES_PER_ELEMENT',
    'call',
    'catch',
    'charAt',
    'charCodeAt',
    'clear',
    'codePointAt',
    'compile',
    'concat',
    'constructor',
    'copyWithin',
    '__defineGetter__',
    '__defineSetter__',
    'delete',
    'endsWith',
    'entries',
    'every',
    'exec',
    'fill',
    'filter',
    'find',
    'findIndex',
    'fixed',
    'flags',
    'fontcolor',
    'fontsize',
    'forEach',
    'get',
    'getDate',
    'getDay',
    'getFloat32',
    'getFloat64',
    'getFullYear',
    'getHours',
    'getInt16',
    'getInt32',
    'getInt8',
    'getMilliseconds',
    'getMinutes',
    'getMonth',
    'getSeconds',
    'getTime',
    'getTimezoneOffset',
    'getUint16',
    'getUint32',
    'getUint8',
    'getUTCDate',
    'getUTCDay',
    'getUTCFullYear',
    'getUTCHours',
    'getUTCMilliseconds',
    'getUTCMinutes',
    'getUTCMonth',
    'getUTCSeconds',
    'getYear',
    'global',
    'has',
    'hasInstance',
    'hasOwnProperty',
    'ignoreCase',
    'includes',
    'indexOf',
    'isConcatSpreadable',
    'isPrototypeOf',
    'italics',
    'iterator',
    'join',
    'keys',
    'lastIndexOf',
    'length',
    'link',
    'localeCompare',
    '__lookupGetter__',
    '__lookupSetter__',
    'map',
    'match',
    'match',
    'message',
    'multiline',
    'name',
    'next',
    'normalize',
    'padEnd',
    'padStart',
    'pop',
    'propertyIsEnumerable',
    '__proto__',
    'prototype',
    'push',
    'reduce',
    'reduceRight',
    'repeat',
    'replace',
    'replace',
    'return',
    'reverse',
    'search',
    'search',
    'set',
    'set',
    'setDate',
    'setFloat32',
    'setFloat64',
    'setFullYear',
    'setHours',
    'setInt16',
    'setInt32',
    'setInt8',
    'setMilliseconds',
    'setMinutes',
    'setMonth',
    'setSeconds',
    'setTime',
    'setUint16',
    'setUint32',
    'setUint8',
    'setUTCDate',
    'setUTCFullYear',
    'setUTCHours',
    'setUTCMilliseconds',
    'setUTCMinutes',
    'setUTCMonth',
    'setUTCSeconds',
    'setYear',
    'shift',
    'size',
    'slice',
    'slice',
    'small',
    'some',
    'sort',
    'source',
    'species',
    'splice',
    'split',
    'split',
    'startsWith',
    'sticky',
    'strike',
    'sub',
    'subarray',
    'substr',
    'substring',
    'sup',
    'test',
    'then',
    'throw',
    'toDateString',
    'toExponential',
    'toFixed',
    'toGMTString',
    'toISOString',
    'toJSON',
    'toLocaleDateString',
    'toLocaleLowerCase',
    'toLocaleString',
    'toLocaleTimeString',
    'toLocaleUpperCase',
    'toLowerCase',
    'toPrecision',
    'toPrimitive',
    'toString',
    'toStringTag',
    'toTimeString',
    'toUpperCase',
    'toUTCString',
    'trim',
    'unicode',
    'unscopables',
    'unshift',
    'valueOf',
    'values',
]);

const MAX_DEPENDENCIES = 2;

class Expression {
  constructor(type, source, originalPath, dependencies) {
    this.type = type;
    this.source = source;
    this.path = originalPath;
    this.dependencies = dependencies;
  }
}

function loadExpression(baseDir, record) {
  const path = fsPath.join(baseDir, record.path);
  const expression = JSON.parse(fs.readFileSync(path), 'utf-8');
  expression.needsTryCatch = record.tc;
  expression.needsSuper = record.super;
  return expression;
}

function dedupKey(expression) {
  if (!expression.dependencies) {
    return expression.source;
  }

  let result = expression.source;
  for (let dependency of expression.dependencies) {
    result = result.replace(new RegExp(dependency, 'g'), 'ID');
  }

  return result;
}

function _markSkipped(path) {
  while (path) {
    path.node.__skipped = true;
    path = path.parentPath;
  }
}

/**
 * Returns true if an expression can be applied or false otherwise.
 */
function isValid(expression) {
  const expressionTemplate = babelTemplate(
      expression.source,
      sourceHelpers.BABYLON_REPLACE_VAR_OPTIONS);

  const dependencies = {};
  if (expression.dependencies) {
    for (const dependency of expression.dependencies) {
      dependencies[dependency] = babelTypes.identifier('__v_0');
    }
  }

  try {
    expressionTemplate(dependencies);
  } catch (e) {
    return false;
  }
  return true;
}

function writeIndexFile(path, index) {
  index.sort((a, b) => a.path.localeCompare(b.path));
  fs.writeFileSync(path, JSON.stringify(index, null, 2));
}

class MutateDbWriter {
  constructor(outputDir) {
    this.seen = new Set();
    this.outputDir = fsPath.resolve(outputDir);
    this.index = [];
  }

  process(source) {
    let self = this;

    let varIndex = 0;

    // First pass to collect dependency information.
    babelTraverse(source.ast, {
      Super(path) {
        while (path) {
          path.node.__needsSuper = true;
          path = path.parentPath;
        }
      },

      YieldExpression(path) {
        // Don't include yield expressions in DB.
        _markSkipped(path);
      },

      Identifier(path) {
        if (globalIdentifiers.has(path.node.name) &&
            path.node.name != 'eval') {
          // Global name.
          return;
        }

        if (propertyNames.has(path.node.name) &&
            path.parentPath.isMemberExpression() &&
            path.parentKey !== 'object') {
          // Builtin property name.
          return;
        }

        if (path.parentPath.isMemberExpression() &&
            path.parent.property == path.node &&
            babelTypes.isIdentifier(path.parent.object) &&
            globalIdentifiers.has(path.parent.object.name)) {
          // Property access on a global name.
          return;
        }

        let binding = path.scope.getBinding(path.node.name);
        if (!binding) {
          // Unknown dependency. Don't handle this.
          _markSkipped(path);
          return;
        }

        let newName;
        if (path.node.name.startsWith('VAR_')) {
          newName = path.node.name;
        } else if (babelTypes.isFunctionDeclaration(binding.path.node) ||
                   babelTypes.isFunctionExpression(binding.path.node) ||
                   babelTypes.isDeclaration(binding.path.node) ||
                   babelTypes.isFunctionExpression(binding.path.node)) {
          // Unknown dependency. Don't handle this.
          _markSkipped(path);
          return;
        } else {
          newName = 'VAR_' + varIndex++;
          path.scope.rename(path.node.name, newName);
        }

        // Mark all parents as having a dependency.
        while (path) {
          path.node.__idDependencies = path.node.__idDependencies || [];
          if (path.node.__idDependencies.length <= MAX_DEPENDENCIES) {
            path.node.__idDependencies.push(newName);
          }
          path = path.parentPath;
        }
      }
    });

    babelTraverse(source.ast, {
      Expression(path) {
        if (!path.parentPath.isExpressionStatement()) {
          return;
        }

        if (path.node.__skipped ||
            (path.node.__idDependencies &&
             path.node.__idDependencies.length > MAX_DEPENDENCIES)) {
          return;
        }

        if (path.isIdentifier() || path.isMemberExpression() ||
            path.isConditionalExpression() ||
            path.isBinaryExpression() || path.isDoExpression() ||
            path.isLiteral() ||
            path.isObjectExpression() || path.isArrayExpression()) {
          // Skip:
          //   - Identifiers.
          //   - Member expressions (too many and too context dependent).
          //   - Conditional expressions (too many and too context dependent).
          //   - Binary expressions (too many).
          //   - Literals (too many).
          //   - Object/array expressions (too many).
          return;
        }

        if (path.isAssignmentExpression()) {
          if (!babelTypes.isMemberExpression(path.node.left)) {
            // Skip assignments that aren't to properties.
            return;
          }

          if (babelTypes.isIdentifier(path.node.left.object)) {
            if (babelTypes.isNumericLiteral(path.node.left.property)) {
              // Skip VAR[\d+] = ...;
              // There are too many and they generally aren't very useful.
              return;
            }

            if (babelTypes.isStringLiteral(path.node.left.property) &&
                !propertyNames.has(path.node.left.property.value)) {
              // Skip custom properties. e.g.
              // VAR["abc"] = ...;
              // There are too many and they generally aren't very useful.
              return;
            }
          }
        }

        if (path.isCallExpression() &&
            babelTypes.isIdentifier(path.node.callee) &&
            !globalIdentifiers.has(path.node.callee.name)) {
          // Skip VAR(...) calls since there's too much context we're missing.
          return;
        }

        if (path.isUnaryExpression() && path.node.operator == '-') {
          // Skip -... since there are too many.
          return;
        }

        // Make the template.
        let generated = babelGenerator(path.node, { concise: true }).code;
        assert(path.parentPath.isExpressionStatement());
        let expression = new Expression(
            path.node.type,
            generated,
            source.relPath,
            path.node.__idDependencies);

        // Try to de-dupe similar expressions.
        const sha1sum = crypto.createHash('sha1');
        sha1sum.update(dedupKey(expression));
        const key = sha1sum.digest('hex').substring(0, 8);
        if (self.seen.has(key)) {
          return;
        }

        // Ignore large samples.
        if (expression.source.length > MAX_SNIPPET_SIZE) {
          return;
        }

        // Test results.
        if (!isValid(expression)) {
          return;
        }

        // Write results.
        let dirPath = fsPath.join(self.outputDir, expression.type);
        if (!fs.existsSync(dirPath)) {
          fs.mkdirSync(dirPath);
        }

        const filePath = fsPath.join(dirPath, key + '.json');
        fs.writeFileSync(filePath, JSON.stringify(expression));

        const relPath = fsPath.relative(self.outputDir, filePath);

        // Update index.
        self.seen.add(key);
        self.index.push(
            {path: relPath, super: Boolean(path.node.__needsSuper)});
      }
    });
  }

  writeIndex() {
    writeIndexFile(fsPath.join(this.outputDir, 'index.json'), this.index);
  }
}

class MutateDb {
  constructor(outputDir) {
    this.outputDir = fsPath.resolve(outputDir);
    const index = JSON.parse(
        fs.readFileSync(fsPath.join(outputDir, 'index.json'), 'utf-8'));
    this.statements = [];
    this.superStatements = [];
    this.all = [];
    for (const expression of index) {
      if (expression.super) {
        this.superStatements.push(expression);
      } else {
        this.statements.push(expression);
      }
      this.all.push(expression);
    }
  }

  getRandomStatement({canHaveSuper=false} = {}) {
    let choices;
    if (canHaveSuper) {
      choices = random.choose(CHOOSE_SUPER_PROB) ?
          this.superStatements : this.all;
    } else {
      choices = this.statements;
    }

    const record = choices[random.randInt(0, choices.length - 1)];
    return loadExpression(this.outputDir, record);
  }

  *iterateStatements() {
    for (const exp of this.all) {
      const path = fsPath.join(this.outputDir, exp.path);
      yield JSON.parse(fs.readFileSync(path), 'utf-8');
    }
  }
}

module.exports = {
  MutateDb: MutateDb,
  MutateDbWriter: MutateDbWriter,
  loadExpression: loadExpression,
  writeIndexFile: writeIndexFile,
}
