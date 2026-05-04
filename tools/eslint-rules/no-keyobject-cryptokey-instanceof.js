/**
 * @file Prevent internal code from brand-checking keys with instanceof.
 */
'use strict';

const { isRequireCall, isString } = require('./rules-utils.js');

const CRYPTO_KEYS_MODULE = 'internal/crypto/keys';
const WEBCRYPTO_MODULE = 'internal/crypto/webcrypto';

const keyObjectClassNames = new Set([
  'KeyObject',
  'SecretKeyObject',
  'AsymmetricKeyObject',
  'PublicKeyObject',
  'PrivateKeyObject',
]);

const cryptoKeyClassNames = new Set([
  'CryptoKey',
  'InternalCryptoKey',
]);

function isKeyModuleRequire(node) {
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

module.exports = {
  meta: {
    messages: {
      noKeyObjectInstanceof: 'Use `isKeyObject(value)` instead of `value instanceof KeyObject`.',
      noCryptoKeyInstanceof: 'Use `isCryptoKey(value)` instead of `value instanceof CryptoKey`.',
    },
    schema: [],
  },

  create(context) {
    const namespaceNames = new Set();
    const keyObjectConstructorNames = new Set();
    const cryptoKeyConstructorNames = new Set(['CryptoKey']);

    function registerRequire(node) {
      if (!isKeyModuleRequire(node.init)) return;

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
        if (keyObjectClassNames.has(keyName)) {
          keyObjectConstructorNames.add(localName);
        } else if (cryptoKeyClassNames.has(keyName)) {
          cryptoKeyConstructorNames.add(localName);
        }
      }
    }

    function constructorKind(node) {
      if (node.type === 'Identifier') {
        if (keyObjectConstructorNames.has(node.name)) return 'KeyObject';
        if (cryptoKeyConstructorNames.has(node.name)) return 'CryptoKey';
        return undefined;
      }

      if (node.type !== 'MemberExpression') return undefined;

      const property = getPropertyName(node);
      if (node.object.type === 'Identifier') {
        if (namespaceNames.has(node.object.name)) {
          if (keyObjectClassNames.has(property)) return 'KeyObject';
          if (cryptoKeyClassNames.has(property)) return 'CryptoKey';
        }
        if (node.object.name === 'globalThis' &&
            cryptoKeyClassNames.has(property)) {
          return 'CryptoKey';
        }
      }

      return undefined;
    }

    return {
      VariableDeclarator: registerRequire,

      BinaryExpression(node) {
        if (node.operator !== 'instanceof') return;

        const kind = constructorKind(node.right);
        if (kind === 'KeyObject') {
          context.report({
            node,
            messageId: 'noKeyObjectInstanceof',
          });
        } else if (kind === 'CryptoKey') {
          context.report({
            node,
            messageId: 'noCryptoKeyInstanceof',
          });
        }
      },
    };
  },
};
