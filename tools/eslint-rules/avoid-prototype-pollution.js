'use strict';

function checkProperties(context, node) {
  if (
    node.type === 'CallExpression' &&
    node.callee.name === 'ObjectGetOwnPropertyDescriptors'
  ) {
    context.report({
      node,
      message:
        'Property descriptors inherits from the Object prototype, therefore are subject to prototype pollution',
    });
  }
  if (node.type !== 'ObjectExpression') return;
  for (const { key, value } of node.properties) {
    if (
      key != null && value != null &&
      !(key.type === 'Identifier' && key.name === '__proto__') &&
      !(key.type === 'Literal' && key.value === '__proto__')
    ) {
      checkPropertyDescriptor(context, value);
    }
  }
}

function checkPropertyDescriptor(context, node) {
  if (
    node.type === 'CallExpression' &&
    (node.callee.name === 'ObjectGetOwnPropertyDescriptor' ||
     node.callee.name === 'ReflectGetOwnPropertyDescriptor')
  ) {
    context.report({
      node,
      message:
        'Property descriptors inherits from the Object prototype, therefore are subject to prototype pollution',
      suggest: [{
        desc: 'Wrap the property descriptor in a null-prototype object',
        fix(fixer) {
          return [
            fixer.insertTextBefore(node, '{ __proto__: null,...'),
            fixer.insertTextAfter(node, ' }'),
          ];
        },
      }],
    });
  }
  if (node.type !== 'ObjectExpression') return;

  for (const { key, value } of node.properties) {
    if (
      key != null && value != null &&
      ((key.type === 'Identifier' && key.name === '__proto__') ||
        (key.type === 'Literal' && key.value === '__proto__')) &&
      value.type === 'Literal' && value.value === null
    ) {
      return true;
    }
  }

  context.report({
    node,
    message: 'Must use null-prototype object for property descriptors',
  });
}

function createUnsafeStringMethodReport(context, name, lookedUpProperty) {
  return {
    [`${CallExpression}[expression.callee.name=${JSON.stringify(name)}]`](node) {
      context.report({
        node,
        message: `${name} looks up the ${lookedUpProperty} property on the first argument`,
      });
    }
  };
}

const CallExpression = 'ExpressionStatement[expression.type="CallExpression"]';
module.exports = {
  meta: { hasSuggestions: true },
  create(context) {
    return {
      [`${CallExpression}[expression.callee.name=${/^(Object|Reflect)DefinePropert(ies|y)$/}]`](
        node
      ) {
        switch (node.expression.callee.name) {
          case 'ObjectDefineProperties':
            checkProperties(context, node.expression.arguments[1]);
            break;
          case 'ReflectDefineProperty':
          case 'ObjectDefineProperty':
            checkPropertyDescriptor(context, node.expression.arguments[2]);
            break;
          default:
            throw new Error('Unreachable');
        }
      },

      [`${CallExpression}[expression.callee.name="ObjectCreate"][expression.arguments.length=2]`](node) {
        checkProperties(context, node.expression.arguments[1]);
      },
      [`${CallExpression}[expression.callee.name="RegExpPrototypeTest"]`](node) {
        context.report({
          node,
          message: '%RegExp.prototype.test% looks up the "exec" property of `this` value',
          suggest: [{
            desc: 'Use RegexpPrototypeExec instead',
            fix(fixer) {
              const testRange = { ...node.range };
              testRange.start = testRange.start + 'RegexpPrototype'.length;
              testRange.end = testRange.start + 'Test'.length;
              return [
                fixer.replaceTextRange(node.range, 'Exec'),
                fixer.insertTextAfter(node, ' !== null'),
              ];
            }
          }]
        });
      },
      [`${CallExpression}[expression.callee.name=${/^RegExpPrototypeSymbol(Match|MatchAll|Search)$/}]`](node) {
        context.report({
          node,
          message: node.expression.callee.name + ' looks up the "exec" property of `this` value',
        });
      },
      ...createUnsafeStringMethodReport(context, 'StringPrototypeMatch', 'Symbol.match'),
      ...createUnsafeStringMethodReport(context, 'StringPrototypeMatchAll', 'Symbol.matchAll'),
      ...createUnsafeStringMethodReport(context, 'StringPrototypeReplace', 'Symbol.replace'),
      ...createUnsafeStringMethodReport(context, 'StringPrototypeReplaceAll', 'Symbol.replace'),
      ...createUnsafeStringMethodReport(context, 'StringPrototypeSearch', 'Symbol.search'),
      ...createUnsafeStringMethodReport(context, 'StringPrototypeSplit', 'Symbol.split'),
    };
  },
};
