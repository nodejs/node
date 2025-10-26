'use strict';

const message =
  'Assertions must be wrapped into `common.mustSucceed`, `common.mustCall` or `common.mustCallAtLeast`';


const requireCall = 'CallExpression[callee.name="require"]';
const assertModuleSpecifier = '/^(node:)?assert(.strict)?$/';

const isPromiseAllCallArg = (node) =>
  node.parent?.type === 'CallExpression' &&
  node.parent.callee.type === 'MemberExpression' &&
  node.parent.callee.object.type === 'Identifier' && node.parent.callee.object.name === 'Promise' &&
  node.parent.callee.property.type === 'Identifier' && node.parent.callee.property.name === 'all' &&
  node.parent.arguments.length === 1 && node.parent.arguments[0] === node;

function findEnclosingFunction(node) {
  while (true) {
    node = node.parent;
    if (!node) break;

    if (node.type !== 'ArrowFunctionExpression' && node.type !== 'FunctionExpression') continue;

    if (node.parent?.type === 'CallExpression') {
      if (node.parent.callee === node) continue; // IIFE

      if (
        node.parent.callee.type === 'MemberExpression' &&
        (node.parent.callee.object.type === 'ArrayExpression' || node.parent.callee.object.type === 'Identifier') &&
        (
          node.parent.callee.property.name === 'forEach' ||
          (node.parent.callee.property.name === 'map' && isPromiseAllCallArg(node.parent))
        )
      ) continue; // `[].forEach()` call
    } else if (node.parent?.type === 'NewExpression') {
      if (node.parent.callee.type === 'Identifier' && node.parent.callee.name === 'Promise') continue;
    } else if (node.parent?.type === 'Property') {
      const ancestor = node.parent.parent?.parent;
      if (ancestor?.type === 'CallExpression' &&
          ancestor.callee.type === 'Identifier' &&
          /^spawnSyncAnd(Exit(WithoutError)?|Assert)$/.test(ancestor.callee.name)) {
        continue;
      }
    }
    break;
  }
  return node;
}

function isMustCallOrMustCallAtLeast(str) {
  return str === 'mustCall' || str === 'mustCallAtLeast' || str === 'mustSucceed';
}

function isMustCallOrTest(str) {
  return str === 'test' || str === 'it' || isMustCallOrMustCallAtLeast(str);
}

module.exports = {
  meta: {
    fixable: 'code',
  },
  create: function(context) {
    return {
      [`:function CallExpression:matches(${[
        '[callee.type="Identifier"][callee.value=/^mustCall(AtLeast)?$/]',
        '[callee.object.name="assert"][callee.property.name!="fail"]',
        '[callee.object.name="common"][callee.property.name=/^mustCall(AtLeast)?$/]',
      ].join(',')})`]: (node) => {
        const enclosingFn = findEnclosingFunction(node);
        const parent = enclosingFn?.parent;
        if (!parent) return; // Top-level
        if (parent.type === 'CallExpression') {
          switch (parent.callee.type) {
            case 'MemberExpression':
              if (
                parent.callee.property.name === 'then' ||
                {
                  assert: (name) => name === 'rejects' || name === 'throws', // assert.throws or assert.rejects
                  common: isMustCallOrMustCallAtLeast, // common.mustCall or common.mustCallAtLeast
                  process: (name) =>
                    (name === 'nextTick' && enclosingFn === parent.arguments[0]) || // process.nextTick
                    ( // process.on('exit', …)
                      (name === 'on' || name === 'once') &&
                      enclosingFn === parent.arguments[1] &&
                      parent.arguments[0].type === 'Literal' &&
                      parent.arguments[0].value === 'exit'
                    ),
                  t: (name) => name === 'test', // t.test
                }[parent.callee.object.name]?.(parent.callee.property.name)
              ) {
                return;
              }
              break;

            case 'Identifier':
              if (isMustCallOrTest(parent.callee.name)) return;
              break;
          }
        }
        context.report({
          node,
          message,
        });
      },

      [[
        `ImportDeclaration[source.value=${assertModuleSpecifier}]:not(${[
          'length=1',
          '0.type=/^Import(Default|Namespace)Specifier$/',
          '0.local.name="assert"',
        ].map((selector) => `[specifiers.${selector}]`).join('')})`,
        `:not(VariableDeclarator[id.name="assert"])>${requireCall}[arguments.0.value=${assertModuleSpecifier}]`,
      ].join(',')]: (node) => {
        context.report({
          node,
          message: 'Only assign `node:assert` to `assert`',
        });
      },
    };
  },
};
