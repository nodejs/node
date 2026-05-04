/**
 * @file Prevent internal code from using public KeyObject accessors.
 */
'use strict';

const { isRequireCall, isString } = require('./rules-utils.js');

const KEYOBJECT_MODULE = 'internal/crypto/keys';

const accessors = new Map([
  ['type', 'getKeyObjectType(key)'],
  ['symmetricKeySize', 'getKeyObjectSymmetricKeySize(key)'],
  ['asymmetricKeyType', 'getKeyObjectAsymmetricKeyType(key)'],
  ['asymmetricKeyDetails', 'getKeyObjectAsymmetricKeyDetails(key)'],
  ['equals', 'getKeyObjectType(key) and getKeyObjectHandle(key)'],
]);

const keyObjectClassNames = new Set([
  'KeyObject',
  'SecretKeyObject',
  'AsymmetricKeyObject',
  'PublicKeyObject',
  'PrivateKeyObject',
]);

const keyObjectFactoryNames = new Set([
  'createSecretKey',
  'createPublicKey',
  'createPrivateKey',
]);

function isInternalCryptoKeysRequire(node) {
  return node?.type === 'CallExpression' &&
         isRequireCall(node) &&
         isString(node.arguments[0]) &&
         node.arguments[0].value === KEYOBJECT_MODULE;
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

module.exports = {
  meta: {
    messages: {
      noPublicAccessor: 'Use `{{replacement}}` instead of the public KeyObject `{{property}}` accessor.',
    },
    schema: [],
  },

  create(context) {
    const isKeyObjectNames = new Set();
    const namespaceNames = new Set();
    const knownKeyObjectNames = new Set();
    const knownKeyObjectClassNames = new Set(keyObjectClassNames);

    function isIsKeyObjectCall(node) {
      if (node?.type !== 'CallExpression') return false;

      if (node.callee.type === 'Identifier') {
        return isKeyObjectNames.has(node.callee.name);
      }

      if (node.callee.type === 'MemberExpression' &&
          !node.callee.computed &&
          node.callee.object.type === 'Identifier' &&
          namespaceNames.has(node.callee.object.name)) {
        return node.callee.property.name === 'isKeyObject';
      }

      return false;
    }

    function getConsequentKeyObjects(test) {
      const names = new Set();
      if (isIsKeyObjectCall(test)) {
        names.add(getIdentifierArgument(test));
      } else if (test?.type === 'LogicalExpression' && test.operator === '&&') {
        for (const name of getConsequentKeyObjects(test.left)) {
          names.add(name);
        }
        for (const name of getConsequentKeyObjects(test.right)) {
          names.add(name);
        }
      }
      names.delete(undefined);
      return names;
    }

    function getAlternateKeyObjects(test) {
      const names = new Set();
      if (test?.type === 'UnaryExpression' &&
          test.operator === '!' &&
          isIsKeyObjectCall(test.argument)) {
        names.add(getIdentifierArgument(test.argument));
      }
      names.delete(undefined);
      return names;
    }

    function isKeyObjectFactory(node) {
      if (node?.type === 'NewExpression' &&
          node.callee.type === 'Identifier') {
        return knownKeyObjectClassNames.has(node.callee.name);
      }

      if (node?.type !== 'CallExpression') return false;

      if (node.callee.type === 'Identifier') {
        return keyObjectFactoryNames.has(node.callee.name);
      }

      if (node.callee.type !== 'MemberExpression') return false;
      const object = node.callee.object;
      const property = getPropertyName(node.callee);
      if (object.type === 'Identifier' &&
          knownKeyObjectClassNames.has(object.name)) {
        return property === 'from';
      }
      return object.type === 'Identifier' &&
             namespaceNames.has(object.name) &&
             keyObjectFactoryNames.has(property);
    }

    function isInKeyObjectBranch(name, node) {
      for (let current = node.parent; current; current = current.parent) {
        if (current.type !== 'IfStatement') continue;
        if (isNodeWithin(node, current.consequent) &&
            getConsequentKeyObjects(current.test).has(name)) {
          return true;
        }
        if (current.alternate &&
            isNodeWithin(node, current.alternate) &&
            getAlternateKeyObjects(current.test).has(name)) {
          return true;
        }
      }
      return false;
    }

    function followsExitingKeyObjectGuard(name, node) {
      const location = findStatementInBlock(node);
      if (!location) return false;
      const index = location.block.body.indexOf(location.statement);
      for (let i = 0; i < index; i++) {
        const statement = location.block.body[i];
        if (statement.type === 'IfStatement' &&
            exits(statement.consequent) &&
            getAlternateKeyObjects(statement.test).has(name)) {
          return true;
        }
      }
      return false;
    }

    function followsLogicalKeyObjectCheck(name, node) {
      for (let current = node; current.parent; current = current.parent) {
        const parent = current.parent;
        if (parent.type !== 'LogicalExpression' || parent.operator !== '&&') {
          continue;
        }
        if (parent.right === current &&
            getConsequentKeyObjects(parent.left).has(name)) {
          return true;
        }
      }
      return false;
    }

    function isInsideKeyObjectClass(node) {
      for (let current = node.parent; current; current = current.parent) {
        if (current.type !== 'ClassDeclaration' &&
            current.type !== 'ClassExpression') {
          continue;
        }

        const className = current.id?.name;
        const superName = current.superClass?.type === 'Identifier' ?
          current.superClass.name : undefined;
        return knownKeyObjectClassNames.has(className) ||
               knownKeyObjectClassNames.has(superName);
      }
      return false;
    }

    function isKnownKeyObject(node) {
      if (node.type === 'ThisExpression') {
        return isInsideKeyObjectClass(node);
      }

      if (node.type !== 'Identifier') return false;
      return knownKeyObjectNames.has(node.name) ||
             isInKeyObjectBranch(node.name, node) ||
             followsLogicalKeyObjectCheck(node.name, node) ||
             followsExitingKeyObjectGuard(node.name, node);
    }

    return {
      VariableDeclarator(node) {
        if (isInternalCryptoKeysRequire(node.init)) {
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
            if (keyName === 'isKeyObject') {
              isKeyObjectNames.add(localName);
            } else if (keyObjectClassNames.has(keyName)) {
              knownKeyObjectClassNames.add(localName);
            }
          }
          return;
        }

        if (node.id.type === 'Identifier' && isKeyObjectFactory(node.init)) {
          knownKeyObjectNames.add(node.id.name);
        }
      },

      MemberExpression(node) {
        const property = getPropertyName(node);
        const replacement = accessors.get(property);
        if (replacement === undefined) return;
        if (!isKnownKeyObject(node.object)) return;

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
