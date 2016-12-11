/**
 * @fileoverview Require usage of specified node modules.
 * @author Rich Trott
 */
'use strict';

var path = require('path');

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
  // trim required module names
  var requiredModules = context.options;

  var foundModules = [];

  // if no modules are required we don't need to check the CallExpressions
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
   * Function to check if a node has an argument that is a required module and
   * return its name.
   * @param {ASTNode} node The node to check
   * @returns {undefined|String} required module name or undefined
   */
  function getRequiredModuleName(node) {
    var moduleName;

    // node has arguments and first argument is string
    if (node.arguments.length && isString(node.arguments[0])) {
      var argValue = path.basename(node.arguments[0].value.trim());

      // check if value is in required modules array
      if (requiredModules.indexOf(argValue) !== -1) {
        moduleName = argValue;
      }
    }

    return moduleName;
  }

  return {
    'CallExpression': function(node) {
      if (isRequireCall(node)) {
        var requiredModuleName = getRequiredModuleName(node);

        if (requiredModuleName) {
          foundModules.push(requiredModuleName);
        }
      }
    },
    'Program:exit': function(node) {
      if (foundModules.length < requiredModules.length) {
        var missingModules = requiredModules.filter(
          function(module) {
            return foundModules.indexOf(module === -1);
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
};

module.exports.schema = {
  'type': 'array',
  'additionalItems': {
    'type': 'string'
  },
  'uniqueItems': true
};
