'use strict';

const CANONICAL_INDEX = /^(?:0|[1-9]\d*)$/;
const PASSTHROUGH_METHODS = new Set(['catch', 'finally']);

function isIndexOrLengthKey(key, computed) {
  if (!computed) {
    if (key.type === 'Identifier') return key.name === 'length';
    if (key.type !== 'Literal') return false;
  }
  if (key.type !== 'Literal') {
    // Computed keys such as `values[i]` are assumed to be indices, unless they
    // look like a well-known symbol (e.g. `values[SymbolIterator]`).
    return !(key.type === 'Identifier' && key.name.startsWith('Symbol'));
  }
  return key.value === 'length' || CANONICAL_INDEX.test(String(key.value));
}

function isUndefined(node) {
  return node == null || (node.type === 'Identifier' && node.name === 'undefined');
}

function isFunction(node) {
  return node?.type === 'ArrowFunctionExpression' || node?.type === 'FunctionExpression';
}

function isFunctionWithoutParams(node) {
  return isFunction(node) && node.params.length === 0;
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

// Returns whether the value of `node` is only used to read its elements or its
// `length`, so it never needs to be a genuine Array.
function isOnlyReadByIndex(node, sourceCode) {
  const { parent } = node;
  switch (parent.type) {
    case 'MemberExpression':
      return parent.object === node &&
        isIndexOrLengthKey(parent.property, parent.computed) &&
        // `values[0]()` would expose the array as `this`.
        !(parent.parent.type === 'CallExpression' && parent.parent.callee === parent);
    case 'VariableDeclarator':
      return parent.init === node && isArrayLikeCompatiblePattern(parent.id, parent, sourceCode);
    default:
      return false;
  }
}

function isArrayLikeCompatiblePattern(pattern, declaringNode, sourceCode) {
  switch (pattern.type) {
    case 'AssignmentPattern':
      return isArrayLikeCompatiblePattern(pattern.left, declaringNode, sourceCode);
    case 'ObjectPattern':
      return pattern.properties.every((property) =>
        property.type === 'RestElement' ||
        isIndexOrLengthKey(property.key, property.computed));
    case 'Identifier': {
      const variable = sourceCode.getDeclaredVariables(declaringNode)
        .find(({ identifiers }) => identifiers.includes(pattern));
      return variable != null && variable.references.every((reference) => {
        if (reference.init) return true;
        return !reference.isWrite() && isOnlyReadByIndex(reference.identifier, sourceCode);
      });
    }
    default:
      return false;
  }
}

function isFulfillValueOnlyReadByIndex(node, sourceCode) {
  const { parent } = node;
  if (parent.type === 'AwaitExpression') {
    return isOnlyReadByIndex(parent, sourceCode);
  }

  let onFulfilled;
  if (parent.type === 'CallExpression' && parent.arguments[0] === node &&
      parent.callee.type === 'Identifier' && parent.callee.name === 'PromisePrototypeThen') {
    onFulfilled = parent.arguments[1];
  } else if (parent.type === 'MemberExpression' && parent.object === node && !parent.computed &&
             parent.property.name === 'then' && parent.parent.type === 'CallExpression' &&
             parent.parent.callee === parent) {
    onFulfilled = parent.parent.arguments[0];
  }
  // A then-callback without parameters ignores the value, which is reported
  // by `no-unused-promise-all-result`.
  return isFunction(onFulfilled) && onFulfilled.params.length > 0 &&
    isArrayLikeCompatiblePattern(onFulfilled.params[0], onFulfilled, sourceCode);
}

module.exports = {
  meta: {
    type: 'suggestion',
    hasSuggestions: true,
    schema: [],
    messages: {
      unusedResult: 'The fulfill value of `{{name}}` is not used, use `SafePromiseAllReturnVoid` instead.',
      useReturnVoid: 'Replace `{{name}}` with `SafePromiseAllReturnVoid`.',

      preferArrayLike: 'The fulfill value of `SafePromiseAll` is only read by index, ' +
        'use `SafePromiseAllReturnArrayLike` instead.',
      useArrayLike: 'Replace `SafePromiseAll` with `SafePromiseAllReturnArrayLike`.',
    },
  },

  create(context) {
    const { sourceCode } = context;

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

      'CallExpression[callee.name=/^(Safe)?PromiseAll$/]'(node) {
        const { callee } = node;
        if (!isFulfillValueOnlyReadByIndex(node, sourceCode)) return;

        context.report({
          node,
          messageId: 'preferArrayLike',
          suggest: [{
            messageId: 'useArrayLike',
            fix: (fixer) => fixer.replaceText(callee, 'SafePromiseAllReturnArrayLike'),
          }],
        });
      },
    };
  },
};
