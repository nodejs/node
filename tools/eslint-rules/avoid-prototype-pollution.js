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
    };
  },
};
