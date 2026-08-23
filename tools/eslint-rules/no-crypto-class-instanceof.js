/**
 * @file Prevent internal code from brand-checking crypto classes with
 *   instanceof.
 */
'use strict';

const { isRequireCall, isString } = require('./rules-utils.js');

const moduleClasses = new Map([
  ['internal/crypto/keys', new Set([
    'KeyObject',
    'SecretKeyObject',
    'AsymmetricKeyObject',
    'PublicKeyObject',
    'PrivateKeyObject',
    'CryptoKey',
    'InternalCryptoKey',
  ])],
  ['internal/crypto/webcrypto', new Set(['CryptoKey'])],
  ['internal/crypto/x509', new Set([
    'X509Certificate',
    'InternalX509Certificate',
  ])],
]);

const knownClassNames = new Set();
for (const classes of moduleClasses.values()) {
  for (const name of classes) knownClassNames.add(name);
}

function getRequiredModule(node) {
  if (node?.type !== 'CallExpression' ||
      !isRequireCall(node) ||
      !isString(node.arguments[0])) {
    return undefined;
  }
  return node.arguments[0].value;
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
      noCryptoClassInstanceof:
        'Do not use `instanceof` to brand-check {{name}}; use its native or private brand.',
      noCryptoKeyInstanceof:
        'Use `isCryptoKey(value)` instead of `value instanceof CryptoKey`.',
      noKeyObjectInstanceof:
        'Use `isKeyObject(value)` instead of `value instanceof KeyObject`.',
      noX509CertificateInstanceof:
        'Use `isX509Certificate(value)` instead of `value instanceof X509Certificate`.',
    },
    schema: [],
  },

  create(context) {
    const sourceCode = context.sourceCode;
    const constructorNames = new Map();
    const namespaceClasses = new Map();

    function registerVariable(node, name, map, value) {
      const variables = sourceCode.scopeManager.getDeclaredVariables(node);
      for (const variable of variables) {
        if (variable.name === name) map.set(variable, value);
      }
    }

    function resolveVariable(node) {
      let scope = sourceCode.getScope(node);
      while (scope !== null) {
        const variable = scope.set.get(node.name);
        if (variable !== undefined) return variable;
        scope = scope.upper;
      }
      return undefined;
    }

    function isGlobalReference(node) {
      const variable = resolveVariable(node);
      return variable === undefined || variable.defs.length === 0;
    }

    function registerRequire(node) {
      const module = getRequiredModule(node.init);
      const classes = moduleClasses.get(module);
      if (classes === undefined) return;

      if (node.id.type === 'Identifier') {
        registerVariable(node, node.id.name, namespaceClasses, classes);
        return;
      }

      if (node.id.type !== 'ObjectPattern') return;
      for (const property of node.id.properties) {
        if (property.type !== 'Property' ||
            property.value.type !== 'Identifier') {
          continue;
        }
        const importedName = property.key.name ?? property.key.value;
        if (classes.has(importedName)) {
          registerVariable(
            node, property.value.name, constructorNames, importedName);
        }
      }
    }

    function declarationName(variable) {
      if (!knownClassNames.has(variable.name)) return undefined;
      for (const definition of variable.defs) {
        if (definition.type === 'ClassName' ||
            definition.type === 'FunctionName') {
          return variable.name;
        }
      }
      return undefined;
    }

    function constructorName(node) {
      if (node.type === 'Identifier') {
        const variable = resolveVariable(node);
        if (variable !== undefined) {
          const name = constructorNames.get(variable) ??
            declarationName(variable);
          if (name !== undefined) return name;
        }
        if (node.name === 'CryptoKey' && isGlobalReference(node)) {
          return 'CryptoKey';
        }
        return undefined;
      }
      if (node.type !== 'MemberExpression') return undefined;

      const property = getPropertyName(node);
      if (node.object.type !== 'Identifier') return undefined;
      if (node.object.name === 'globalThis' &&
          property === 'CryptoKey' &&
          isGlobalReference(node.object)) {
        return 'CryptoKey';
      }

      const variable = resolveVariable(node.object);
      const classes = namespaceClasses.get(variable);
      return classes?.has(property) ? property : undefined;
    }

    return {
      VariableDeclarator: registerRequire,

      BinaryExpression(node) {
        if (node.operator !== 'instanceof') return;

        const name = constructorName(node.right);
        if (name === undefined) return;

        let messageId = 'noCryptoClassInstanceof';
        if (name === 'CryptoKey' || name === 'InternalCryptoKey') {
          messageId = 'noCryptoKeyInstanceof';
        } else if (name.endsWith('KeyObject')) {
          messageId = 'noKeyObjectInstanceof';
        } else if (name.endsWith('X509Certificate')) {
          messageId = 'noX509CertificateInstanceof';
        }

        context.report({
          node,
          messageId,
          data: { name },
        });
      },
    };
  },
};
