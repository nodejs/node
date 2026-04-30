'use strict';

const message =
  'Assertions must be wrapped into `common.mustSucceed`, `common.mustCall` or `common.mustCallAtLeast`';

const requireCall = 'CallExpression[callee.name="require"]';
const assertModuleSpecifier = '/^(node:)?assert(.strict)?$/';

const isObjectKeysOrEntries = (node) =>
  node.type === 'CallExpression' &&
  node.callee.type === 'MemberExpression' &&
  node.callee.object.type === 'Identifier' &&
  node.callee.object.name === 'Object' &&
  node.callee.property.type === 'Identifier' &&
  ['keys', 'entries'].includes(node.callee.property.name);

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
        (
          node.parent.callee.object.type === 'ArrayExpression' ||
          node.parent.callee.object.type === 'Identifier' ||
          isObjectKeysOrEntries(node.parent.callee.object)
        ) &&
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
        '[callee.type="Identifier"][callee.name=/^mustCall(AtLeast)?$/]',
        '[callee.type="Identifier"][callee.name="assert"]',
        '[callee.object.name="assert"][callee.property.name!="fail"]',
        '[callee.object.name="common"][callee.property.name=/^mustCall(AtLeast)?$/]',
      ].join(',')})`]: (node) => {
        const enclosingFn = findEnclosingFunction(node);
        const parent = enclosingFn?.parent;
        if (!parent) return; // Top-level
        if (parent.type === 'BinaryExpression' && parent.operator === '+') {
          // Function is stringified, e.g. to be passed to a child process or a worker.
          return;
        }
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

      [`CallExpression[callee.property.name="then"][arguments.length=1]>CallExpression:matches(${[
        '[callee.name="mustCall"]',
        '[callee.object.name="common"][callee.property.name="mustCall"]',
      ].join(',')})[arguments.length=1]>:has(ReturnStatement)`]: (node) => {
        context.report({
          node,
          message: 'Cannot mix `common.mustCall` and return statement inside a `.then` chain',
        });
      },

      'ExpressionStatement>CallExpression[callee.property.name="then"]': (node) => {
        const { arguments: { length, 0: arg }, callee: { object } } = node;

        if (object.type === 'CallExpression' &&
            object.callee.type === 'Identifier' &&
            object.callee.name === 'checkSupportReusePort') {
          return;
        }

        const defaultMessage = 'Last item of `.then` list must use `mustCall()` ' +
                               'or `mustNotCall("expected never settling promise")`';

        if (length !== 1) {
          context.report({
            node,
            message: 'Expected exactly one argument for `.then`, consider using ' +
                     '`assert.rejects` or remove the second argument',
          });
        } else if (arg.type !== 'CallExpression') {
          context.report({
            node: arg,
            message: defaultMessage,
          });
        } else {
          const message = {
            mustCall(args) {
              switch (args.length) {
                case 0: return false;
                case 1: {
                  const [arg] = args;
                  if (arg.async || arg.body.type !== 'BlockStatement')
                    return 'Add an additional `.then(common.mustCall())` to detect if resulting promise settles';
                  return false;
                }
                default: return 'do not use more than one argument';
              }
            },
            mustNotCall(args) {
              if (args.length === 1 && args[0].type === 'Literal' && args[0].value.includes('never settling')) {
                return false;
              }
              return 'Unexpected `common.mustNotCall`, either add a label mention ' +
                     '"never settling promise" or use `common.mustCall`';
            },
          }[arg.callee.type === 'MemberExpression' && arg.callee.object.name === 'common' ?
            arg.callee.property.name :
            arg.callee.name]?.(arg.arguments) ?? defaultMessage;
          if (message) {
            context.report({ node: arg, message });
          }
        }

        node = node.callee.object;
        // Walk down .then chain
        while (
          node?.type === 'CallExpression' &&
          node.callee.type === 'MemberExpression' &&
          node.callee.property.name === 'then'
        ) {
          const { arguments: { 0: arg } } = node;
          if (arg &&
          arg.type === 'CallExpression' &&
          (arg.callee.type === 'MemberExpression' && arg.callee.object.name === 'common' ?
            arg.callee.property.name :
            arg.callee.name
          ) === 'mustCall') {
            context.report({
              node,
              message: 'in a `.then` chain, only use mustCall as the last node',
            });
          }
          // Go deeper: find next "left" call in chain
          node = node.callee.object;
        }
      },
    };
  },
};
