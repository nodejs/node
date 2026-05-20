/**
 * @file Prevent internal code from using public CryptoKey accessors.
 */
'use strict';

const { isRequireCall, isString } = require('./rules-utils.js');

const CRYPTO_KEYS_MODULE = 'internal/crypto/keys';
const WEBCRYPTO_MODULE = 'internal/crypto/webcrypto';

const accessors = new Map([
  ['type', 'getCryptoKeyType(key)'],
  ['extractable', 'getCryptoKeyExtractable(key)'],
  ['algorithm', 'getCryptoKeyAlgorithm(key)'],
  ['usages', 'getCryptoKeyUsages(key)'],
]);

const cryptoKeyClassNames = new Set([
  'CryptoKey',
  'InternalCryptoKey',
]);

function isCryptoKeyModuleRequire(node) {
  return node?.type === 'CallExpression' &&
         isRequireCall(node) &&
         isString(node.arguments[0]) &&
         (node.arguments[0].value === CRYPTO_KEYS_MODULE ||
          node.arguments[0].value === WEBCRYPTO_MODULE);
}

function getPropertyName(node) {
  if (!node) return undefined;
  if (node.computed) {
    return node.property.type === 'Literal' ? node.property.value : undefined;
  }
  return node.property.name;
}

function getIdentifierArgument(node) {
  const arg = node.arguments[0];
  return arg?.type === 'Identifier' ? arg.name : undefined;
}

function isNodeWithin(node, ancestor) {
  return node.range[0] >= ancestor.range[0] &&
         node.range[1] <= ancestor.range[1];
}

function exits(statement) {
  if (!statement) return false;
  switch (statement.type) {
    case 'BlockStatement':
      return statement.body.length > 0 && exits(statement.body.at(-1));
    case 'ReturnStatement':
    case 'ThrowStatement':
      return true;
    default:
      return false;
  }
}

function findStatementInBlock(node) {
  let current = node;
  while (current?.parent) {
    if ((current.parent.type === 'BlockStatement' ||
         current.parent.type === 'Program') &&
        current.parent.body.includes(current)) {
      return { block: current.parent, statement: current };
    }
    current = current.parent;
  }
}

function isWebIDLCryptoKeyConverter(node) {
  if (node?.type !== 'CallExpression') return false;
  if (node.callee.type !== 'MemberExpression') return false;
  if (getPropertyName(node.callee) !== 'CryptoKey') return false;

  const converter = node.callee.object;
  return converter?.type === 'MemberExpression' &&
         getPropertyName(converter) === 'converters';
}

module.exports = {
  meta: {
    messages: {
      noPublicAccessor: 'Use `{{replacement}}` instead of the public CryptoKey `{{property}}` accessor.',
    },
    schema: [],
  },

  create(context) {
    const isCryptoKeyNames = new Set();
    const namespaceNames = new Set();
    const knownCryptoKeyNames = new Set();
    const knownCryptoKeyClassNames = new Set(cryptoKeyClassNames);

    function isIsCryptoKeyCall(node) {
      if (node?.type !== 'CallExpression') return false;

      if (node.callee.type === 'Identifier') {
        return isCryptoKeyNames.has(node.callee.name);
      }

      if (node.callee.type === 'MemberExpression' &&
          !node.callee.computed &&
          node.callee.object.type === 'Identifier' &&
          namespaceNames.has(node.callee.object.name)) {
        return node.callee.property.name === 'isCryptoKey';
      }

      return false;
    }

    function getConsequentCryptoKeys(test) {
      const names = new Set();
      if (isIsCryptoKeyCall(test)) {
        names.add(getIdentifierArgument(test));
      } else if (test?.type === 'LogicalExpression' && test.operator === '&&') {
        for (const name of getConsequentCryptoKeys(test.left)) {
          names.add(name);
        }
        for (const name of getConsequentCryptoKeys(test.right)) {
          names.add(name);
        }
      }
      names.delete(undefined);
      return names;
    }

    function getAlternateCryptoKeys(test) {
      const names = new Set();
      if (test?.type === 'UnaryExpression' &&
          test.operator === '!' &&
          isIsCryptoKeyCall(test.argument)) {
        names.add(getIdentifierArgument(test.argument));
      }
      names.delete(undefined);
      return names;
    }

    function isCryptoKeyFactory(node) {
      if (node?.type === 'NewExpression' &&
          node.callee.type === 'Identifier') {
        return knownCryptoKeyClassNames.has(node.callee.name);
      }

      return isWebIDLCryptoKeyConverter(node);
    }

    function isInCryptoKeyBranch(name, node) {
      for (let current = node.parent; current; current = current.parent) {
        if (current.type !== 'IfStatement') continue;
        if (isNodeWithin(node, current.consequent) &&
            getConsequentCryptoKeys(current.test).has(name)) {
          return true;
        }
        if (current.alternate &&
            isNodeWithin(node, current.alternate) &&
            getAlternateCryptoKeys(current.test).has(name)) {
          return true;
        }
      }
      return false;
    }

    function followsExitingCryptoKeyGuard(name, node) {
      const location = findStatementInBlock(node);
      if (!location) return false;
      const index = location.block.body.indexOf(location.statement);
      for (let i = 0; i < index; i++) {
        const statement = location.block.body[i];
        if (statement.type === 'IfStatement' &&
            exits(statement.consequent) &&
            getAlternateCryptoKeys(statement.test).has(name)) {
          return true;
        }
      }
      return false;
    }

    function followsLogicalCryptoKeyCheck(name, node) {
      for (let current = node; current.parent; current = current.parent) {
        const parent = current.parent;
        if (parent.type !== 'LogicalExpression' || parent.operator !== '&&') {
          continue;
        }
        if (parent.right === current &&
            getConsequentCryptoKeys(parent.left).has(name)) {
          return true;
        }
      }
      return false;
    }

    function isInsideCryptoKeyClass(node) {
      for (let current = node.parent; current; current = current.parent) {
        if (current.type !== 'ClassDeclaration' &&
            current.type !== 'ClassExpression') {
          continue;
        }

        const className = current.id?.name;
        const superName = current.superClass?.type === 'Identifier' ?
          current.superClass.name : undefined;
        return knownCryptoKeyClassNames.has(className) ||
               knownCryptoKeyClassNames.has(superName);
      }
      return false;
    }

    function isKnownCryptoKey(node) {
      if (node.type === 'ThisExpression') {
        return isInsideCryptoKeyClass(node);
      }

      if (node.type !== 'Identifier') return false;
      return knownCryptoKeyNames.has(node.name) ||
             isInCryptoKeyBranch(node.name, node) ||
             followsLogicalCryptoKeyCheck(node.name, node) ||
             followsExitingCryptoKeyGuard(node.name, node);
    }

    return {
      VariableDeclarator(node) {
        if (isCryptoKeyModuleRequire(node.init)) {
          if (node.id.type === 'Identifier') {
            namespaceNames.add(node.id.name);
            return;
          }

          if (node.id.type !== 'ObjectPattern') return;

          for (const property of node.id.properties) {
            if (property.type !== 'Property') continue;
            const keyName = property.key.name ?? property.key.value;
            if (property.value.type !== 'Identifier') continue;
            const localName = property.value.name;
            if (keyName === 'isCryptoKey') {
              isCryptoKeyNames.add(localName);
            } else if (cryptoKeyClassNames.has(keyName)) {
              knownCryptoKeyClassNames.add(localName);
            }
          }
          return;
        }

        if (node.id.type === 'Identifier' && isCryptoKeyFactory(node.init)) {
          knownCryptoKeyNames.add(node.id.name);
        }
      },

      AssignmentExpression(node) {
        if (node.left.type === 'Identifier' && isCryptoKeyFactory(node.right)) {
          knownCryptoKeyNames.add(node.left.name);
        }
      },

      MemberExpression(node) {
        const property = getPropertyName(node);
        const replacement = accessors.get(property);
        if (replacement === undefined) return;
        if (!isKnownCryptoKey(node.object)) return;

        context.report({
          node: node.property,
          messageId: 'noPublicAccessor',
          data: {
            property,
            replacement,
          },
        });
      },

    };
  },
};
