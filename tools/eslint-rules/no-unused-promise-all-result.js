'use strict';

const PASSTHROUGH_METHODS = new Set(['catch', 'finally']);

function isUndefined(node) {
  return node == null || (node.type === 'Identifier' && node.name === 'undefined');
}

function isFunctionWithoutParams(node) {
  return (node.type === 'ArrowFunctionExpression' || node.type === 'FunctionExpression') &&
    node.params.length === 0;
}

// `onFulfilled` is the then-callback when `promise` is passed to
// `PromisePrototypeThen` or used as `promise.then(...)`.
function isFulfillValueIgnoredByThen(onFulfilled, thenCall) {
  if (isUndefined(onFulfilled)) return isValueIgnored(thenCall);
  return isFunctionWithoutParams(onFulfilled);
}

function isValueIgnored(node) {
  const { parent } = node;
  switch (parent.type) {
    case 'ExpressionStatement':
      return true;
    case 'AwaitExpression':
    case 'ChainExpression':
      return isValueIgnored(parent);
    case 'UnaryExpression':
      return parent.operator === 'void';
    case 'SequenceExpression':
      return parent.expressions.at(-1) !== node || isValueIgnored(parent);
    case 'ConditionalExpression':
      return parent.test !== node && isValueIgnored(parent);
    case 'LogicalExpression':
      return parent.right === node && isValueIgnored(parent);
    case 'CallExpression': {
      if (parent.arguments[0] !== node || parent.callee.type !== 'Identifier') return false;
      const { name } = parent.callee;
      if (name === 'PromisePrototypeThen') {
        return isFulfillValueIgnoredByThen(parent.arguments[1], parent);
      }
      return name === 'SafePromisePrototypeFinally' && isValueIgnored(parent);
    }
    case 'MemberExpression': {
      const call = parent.parent;
      if (parent.object !== node || parent.computed ||
          call.type !== 'CallExpression' || call.callee !== parent) {
        return false;
      }
      const { name } = parent.property;
      if (name === 'then') {
        return isFulfillValueIgnoredByThen(call.arguments[0], call);
      }
      return PASSTHROUGH_METHODS.has(name) && isValueIgnored(call);
    }
    default:
      return false;
  }
}

module.exports = {
  meta: {
    type: 'suggestion',
    hasSuggestions: true,
    schema: [],
    messages: {
      unusedResult: 'The fulfill value of `{{name}}` is not used, use `SafePromiseAllReturnVoid` instead.',
      useReturnVoid: 'Replace `{{name}}` with `SafePromiseAllReturnVoid`.',
    },
  },

  create(context) {
    return {
      'CallExpression[callee.name=/^(Safe)?PromiseAll(ReturnArrayLike)?$/]'(node) {
        const { callee } = node;
        if (!isValueIgnored(node)) return;

        const { name } = callee;
        // `PromiseAll` accepts any iterable, `SafePromiseAllReturnVoid` only array-likes.
        const canSuggest = name !== 'PromiseAll' || node.arguments[0]?.type === 'ArrayExpression';
        context.report({
          node,
          messageId: 'unusedResult',
          data: { name },
          suggest: canSuggest ? [{
            messageId: 'useReturnVoid',
            data: { name },
            fix: (fixer) => fixer.replaceText(callee, 'SafePromiseAllReturnVoid'),
          }] : [],
        });
      },
    };
  },
};
