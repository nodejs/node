/**
 * @fileoverview Require usage of specified node modules.
 * @author Rich Trott
 */
'use strict';

const path = require('path');

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
  // trim required module names
  var requiredModules = context.options;
  const isESM = context.parserOptions.sourceType === 'module';

  const foundModules = [];

  // If no modules are required we don't need to check the CallExpressions
  if (requiredModules.length === 0) {
    return {};
  }

  /**
   * Function to check if a node is a string literal.
   * @param {ASTNode} node The node to check.
   * @returns {boolean} If the node is a string literal.
   */
  function isString(node) {
    return node && node.type === 'Literal' && typeof node.value === 'string';
  }

  /**
   * Function to check if a node is a require call.
   * @param {ASTNode} node The node to check.
   * @returns {boolean} If the node is a require call.
   */
  function isRequireCall(node) {
    return node.callee.type === 'Identifier' && node.callee.name === 'require';
  }

  /**
   * Function to check if the path is a required module and return its name.
   * @param {String} str The path to check
   * @returns {undefined|String} required module name or undefined
   */
  function getRequiredModuleName(str) {
    var value = path.basename(str);

    // check if value is in required modules array
    return requiredModules.indexOf(value) !== -1 ? value : undefined;
  }

  /**
   * Function to check if a node has an argument that is a required module and
   * return its name.
   * @param {ASTNode} node The node to check
   * @returns {undefined|String} required module name or undefined
   */
  function getRequiredModuleNameFromCall(node) {
    // node has arguments and first argument is string
    if (node.arguments.length && isString(node.arguments[0])) {
      return getRequiredModuleName(node.arguments[0].value.trim());
    }

    return undefined;
  }

  const rules = {
    'Program:exit'(node) {
      if (foundModules.length < requiredModules.length) {
        var missingModules = requiredModules.filter(
          function(module) {
            return foundModules.indexOf(module) === -1;
          }
        );
        missingModules.forEach(function(moduleName) {
          context.report(
            node,
            'Mandatory module "{{moduleName}}" must be loaded.',
            { moduleName: moduleName }
          );
        });
      }
    }
  };

  if (isESM) {
    rules.ImportDeclaration = (node) => {
      var requiredModuleName = getRequiredModuleName(node.source.value);
      if (requiredModuleName) {
        foundModules.push(requiredModuleName);
      }
    };
  } else {
    rules.CallExpression = (node) => {
      if (isRequireCall(node)) {
        var requiredModuleName = getRequiredModuleNameFromCall(node);

        if (requiredModuleName) {
          foundModules.push(requiredModuleName);
        }
      }
    };
  }

  return rules;
};

module.exports.schema = {
  'type': 'array',
  'additionalItems': {
    'type': 'string'
  },
  'uniqueItems': true
};
