// @fileoverview Rule to require function names to match the name of the variable or property to which they
// are assigned.
// @author Annie Zhang
// @author Pavel Strashkin

'use strict';

//--------------------------------------------------------------------------
// Requirements
//--------------------------------------------------------------------------

const astUtils = require('../eslint/node_modules/eslint/lib/rules/utils/ast-utils');
const esutils = require('../eslint/node_modules/esutils');

//--------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------

/**
 * Determines if a pattern is `module.exports` or `module['exports']`
 * @param {ASTNode} pattern The left side of the AssignmentExpression
 * @returns {boolean} True if the pattern is `module.exports` or `module['exports']`
 */
function isModuleExports(pattern) {
  if (
    pattern.type === 'MemberExpression' &&
    pattern.object.type === 'Identifier' &&
    pattern.object.name === 'module'
  ) {
    // module.exports
    if (
      pattern.property.type === 'Identifier' &&
      pattern.property.name === 'exports'
    ) {
      return true;
    }

    // module['exports']
    if (
      pattern.property.type === 'Literal' &&
      pattern.property.value === 'exports'
    ) {
      return true;
    }
  }
  return false;
}

/**
 * Determines if a string name is a valid identifier
 * @param {string} name The string to be checked
 * @param {number} ecmaVersion The ECMAScript version if specified in the parserOptions config
 * @returns {boolean} True if the string is a valid identifier
 */
function isIdentifier(name, ecmaVersion) {
  if (ecmaVersion >= 2015) {
    return esutils.keyword.isIdentifierES6(name);
  }
  return esutils.keyword.isIdentifierES5(name);
}

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

const alwaysOrNever = { enum: ['always', 'never'] };
const optionsObject = {
  type: 'object',
  properties: {
    considerPropertyDescriptor: {
      type: 'boolean',
    },
    includeCommonJSModuleExports: {
      type: 'boolean',
    },
  },
  additionalProperties: false,
};

/** @type {import('../types').Rule.RuleModule} */
module.exports = {
  meta: {
    type: 'suggestion',

    docs: {
      description:
        'Require function names to match the name of the variable or property to which they are assigned',
      recommended: false,
      frozen: true,
      url: 'https://eslint.org/docs/latest/rules/func-name-matching',
    },

    schema: {
      anyOf: [
        {
          type: 'array',
          additionalItems: false,
          items: [alwaysOrNever, optionsObject],
        },
        {
          type: 'array',
          additionalItems: false,
          items: [optionsObject],
        },
      ],
    },

    messages: {
      matchProperty:
        'Function name `{{funcName}}` should match property name `{{name}}`.',
      matchVariable:
        'Function name `{{funcName}}` should match variable name `{{name}}`.',
      notMatchProperty:
        'Function name `{{funcName}}` should not match property name `{{name}}`.',
      notMatchVariable:
        'Function name `{{funcName}}` should not match variable name `{{name}}`.',
    },
  },

  create(context) {
    const options =
      (typeof context.options[0] === 'object' ?
        context.options[0] : context.options[1]) || {};
    const nameMatches =
      typeof context.options[0] === 'string' ?
        context.options[0] : 'always';
    const considerPropertyDescriptor = options.considerPropertyDescriptor;
    const includeModuleExports = options.includeCommonJSModuleExports;
    const ecmaVersion = context.languageOptions.ecmaVersion;

    /**
     * Check whether node is a certain CallExpression.
     * @param {string} objName object name
     * @param {string} funcName function name
     * @param {ASTNode} node The node to check
     * @returns {boolean} `true` if node matches CallExpression
     */
    function isPropertyCall(objName, funcName, node) {
      if (!node) {
        return false;
      }
      return (
        node.type === 'CallExpression' &&
        astUtils.isSpecificMemberAccess(node.callee, objName, funcName)
      );
    }

    function isIdentifierCall(expectedIdentifierName, node) {
      if (!node) {
        return false;
      }
      return node.type === 'CallExpression' &&
        astUtils.isSpecificId(node.callee, expectedIdentifierName);
    }

    /**
     * Compares identifiers based on the nameMatches option
     * @param {string} x the first identifier
     * @param {string} y the second identifier
     * @returns {boolean} whether the two identifiers should warn.
     */
    function shouldWarn(x, y) {
      return (
        (nameMatches === 'always' && x !== y) ||
        (nameMatches === 'never' && x === y)
      );
    }

    /**
     * Reports
     * @param {ASTNode} node The node to report
     * @param {string} name The variable or property name
     * @param {string} funcName The function name
     * @param {boolean} isProp True if the reported node is a property assignment
     * @returns {void}
     */
    function report(node, name, funcName, isProp) {
      let messageId;

      if (nameMatches === 'always' && isProp) {
        messageId = 'matchProperty';
      } else if (nameMatches === 'always') {
        messageId = 'matchVariable';
      } else if (isProp) {
        messageId = 'notMatchProperty';
      } else {
        messageId = 'notMatchVariable';
      }
      context.report({
        node,
        messageId,
        data: {
          name,
          funcName,
        },
      });
    }

    /**
     * Determines whether a given node is a string literal
     * @param {ASTNode} node The node to check
     * @returns {boolean} `true` if the node is a string literal
     */
    function isStringLiteral(node) {
      return node.type === 'Literal' && typeof node.value === 'string';
    }

    //--------------------------------------------------------------------------
    // Public
    //--------------------------------------------------------------------------

    return {
      'VariableDeclarator[init.type="FunctionExpression"][id.type="Identifier"]'(node) {
        if (
          node.init.id &&
          shouldWarn(node.id.name, node.init.id.name)
        ) {
          report(node, node.id.name, node.init.id.name, false);
        }
      },

      'AssignmentExpression[right.type="FunctionExpression"][left.type=/^(Identifier|MemberExpression)$/]'(node) {
        if (
          (node.left.computed && node.left.property.type !== 'Literal') ||
          (!includeModuleExports && isModuleExports(node.left))
        ) {
          return;
        }

        const isProp = node.left.type === 'MemberExpression';
        const name = isProp ? astUtils.getStaticPropertyName(node.left) : node.left.name;

        if (
          node.right.id &&
          name &&
          isIdentifier(name) &&
          shouldWarn(name, node.right.id.name)
        ) {
          report(node, name, node.right.id.name, isProp);
        }
      },

      'Property[value.type="FunctionExpression"][value.id]'(node) {

        if (node.key.type === 'Identifier' && !node.computed) {
          const functionName = node.value.id.name;
          let propertyName = node.key.name;

          if (
            considerPropertyDescriptor &&
            propertyName === 'value' &&
            node.parent.type === 'ObjectExpression'
          ) {
            if (
              isPropertyCall(
                'Object',
                'defineProperty',
                node.parent.parent,
              ) ||
              isPropertyCall(
                'Reflect',
                'defineProperty',
                node.parent.parent,
              ) ||
              isIdentifierCall('ObjectDefineProperty', node.parent.parent) ||
              isIdentifierCall('ReflectDefineProperty', node.parent.parent)
            ) {
              const property = node.parent.parent.arguments[1];

              if (
                isStringLiteral(property) &&
                shouldWarn(property.value, functionName)
              ) {
                report(
                  node,
                  property.value,
                  functionName,
                  true,
                );
              }
            } else if (
              isPropertyCall(
                'Object',
                'defineProperties',
                node.parent.parent.parent.parent,
              ) ||
              isIdentifierCall('ObjectDefineProperties', node.parent.parent.parent.parent)
            ) {
              propertyName = node.parent.parent.key.name;
              if (
                !node.parent.parent.computed &&
                shouldWarn(propertyName, functionName)
              ) {
                report(node, propertyName, functionName, true);
              }
            } else if (
              isPropertyCall(
                'Object',
                'create',
                node.parent.parent.parent.parent,
              )
            ) {
              propertyName = node.parent.parent.key.name;
              if (
                !node.parent.parent.computed &&
                shouldWarn(propertyName, functionName)
              ) {
                report(node, propertyName, functionName, true);
              }
            } else if (shouldWarn(propertyName, functionName)) {
              report(node, propertyName, functionName, true);
            }
          } else if (shouldWarn(propertyName, functionName)) {
            report(node, propertyName, functionName, true);
          }
          return;
        }

        if (
          isStringLiteral(node.key) &&
          isIdentifier(node.key.value, ecmaVersion) &&
          shouldWarn(node.key.value, node.value.id.name)
        ) {
          report(node, node.key.value, node.value.id.name, true);
        }
      },
    };
  },
};
