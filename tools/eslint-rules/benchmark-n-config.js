'use strict';

// Requires benchmarks that declare an `n` iteration count in their configs
// to actually read that value in the benchmark main function.
//
// This prevents regressions like nodejs/node#59872, where a config key was
// renamed to `n` while main() kept reading the old key, silently producing
// a near-zero result.

function walk(node, visit) {
  if (node === null || typeof node !== 'object') return;
  if (Array.isArray(node)) {
    for (const item of node) walk(item, visit);
    return;
  }
  if (typeof node.type !== 'string') return;
  visit(node);
  for (const key of Object.keys(node)) {
    if (key === 'parent' || key === 'loc' || key === 'range') continue;
    walk(node[key], visit);
  }
}

// Collects the property names read by an ObjectPattern: `{ num: n }` reads
// `num`, `{ n }` reads `n`.
function objectPatternNames(pattern, out) {
  for (const prop of pattern.properties) {
    if (prop.type !== 'Property' || prop.computed) continue;
    if (prop.key.type === 'Identifier') out.add(prop.key.name);
    else if (prop.key.type === 'Literal' && typeof prop.key.value === 'string') {
      out.add(prop.key.value);
    }
    if (prop.value && prop.value.type === 'ObjectPattern') {
      objectPatternNames(prop.value, out);
    }
  }
}

// Returns false when the object literal is not fully static.
function objectExpressionKeys(node, out) {
  for (const prop of node.properties) {
    if (prop.type === 'SpreadElement') return false;
    if (prop.type !== 'Property' || prop.computed) continue;
    if (prop.key.type === 'Identifier') out.add(prop.key.name);
    else if (prop.key.type === 'Literal' && typeof prop.key.value === 'string') {
      out.add(prop.key.value);
    }
  }
  return true;
}

module.exports = {
  meta: {
    type: 'problem',
    docs: {
      description: 'require benchmark main to read `n` when configs declare it',
    },
    schema: [],
  },

  create(context) {
    const { sourceCode } = context;

    let state = null;

    return {
      CallExpression(node) {
        if (state !== null) return;
        const callee = node.callee;
        if (callee.type !== 'MemberExpression' || callee.computed) return;
        if (callee.property.name !== 'createBenchmark') return;
        if (node.arguments.length < 2) return;
        state = {
          mainArg: node.arguments[0],
          configArg: node.arguments[1],
        };
      },

      'Program:exit'() {
        if (state === null) return;
        const { mainArg, configArg } = state;

        // Resolve config keys.
        const configKeys = new Set();
        if (configArg.type === 'ObjectExpression') {
          if (!objectExpressionKeys(configArg, configKeys)) return;
        } else if (configArg.type === 'Identifier') {
          let resolved = false;
          walk(sourceCode.ast, (n) => {
            if (resolved) return;
            if (n.type === 'VariableDeclarator' && n.id.type === 'Identifier' &&
                n.id.name === configArg.name && n.init &&
                n.init.type === 'ObjectExpression') {
              resolved = objectExpressionKeys(n.init, configKeys);
            }
          });
          if (!resolved) return;
        } else {
          return;
        }
        if (!configKeys.has('n')) return;

        // Resolve the main function node.
        let fn = null;
        if (mainArg.type === 'Identifier') {
          walk(sourceCode.ast, (n) => {
            if (fn !== null) return;
            if (n.type === 'FunctionDeclaration' && n.id && n.id.name === mainArg.name) {
              fn = n;
            }
            if (fn === null && n.type === 'VariableDeclarator' && n.id.type === 'Identifier' &&
                n.id.name === mainArg.name && n.init &&
                (n.init.type === 'FunctionExpression' ||
                 n.init.type === 'ArrowFunctionExpression')) {
              fn = n.init;
            }
          });
        } else if (mainArg.type === 'FunctionExpression' ||
                   mainArg.type === 'ArrowFunctionExpression') {
          fn = mainArg;
        }
        if (fn === null) return;

        // Collect every name through which `n` could be read in main().
        const paramNames = new Set();
        const readNames = new Set();
        for (const param of fn.params) {
          if (param.type === 'ObjectPattern') {
            objectPatternNames(param, readNames);
          } else if (param.type === 'Identifier') {
            paramNames.add(param.name);
          }
        }
        walk(fn.body, (n) => {
          // `const { a, b } = conf;` inside the body.
          if (n.type === 'VariableDeclarator' && n.id.type === 'ObjectPattern' &&
              n.init && n.init.type === 'Identifier' && paramNames.has(n.init.name)) {
            objectPatternNames(n.id, readNames);
          }
          // `conf.n` member access.
          if (n.type === 'MemberExpression' && !n.computed &&
              n.object.type === 'Identifier' && paramNames.has(n.object.name) &&
              n.property.type === 'Identifier') {
            readNames.add(n.property.name);
          }
        });

        if (!readNames.has('n')) {
          context.report({
            node: fn.id ?? fn,
            message: '`n` is configured but not read by the benchmark main ' +
                     'function (destructure `{ n }` or read `conf.n`)',
          });
        }
      },
    };
  },
};
