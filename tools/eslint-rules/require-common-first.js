/**
 * @fileoverview Require `common` module first in our tests.
 */
'use strict';

const path = require('path');

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = function(context) {
  const requiredModule = 'common';
  const isESM = context.parserOptions.sourceType === 'module';
  const foundModules = [];

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
   * @returns {String} required module name
   */
  function getRequiredModuleName(str) {
    if (str === '../common/index.mjs') {
      return 'common';
    }

    return path.basename(str);
  }

  /**
   * Function to check if a node has an argument that is a required module and
   * return its name.
   * @param {ASTNode} node The node to check
   * @returns {undefined|String} required module name or undefined
   */
  function getRequiredModuleNameFromCall(node) {
    // Node has arguments and first argument is string
    if (node.arguments.length && isString(node.arguments[0])) {
      return getRequiredModuleName(node.arguments[0].value.trim());
    }

    return undefined;
  }

  const rules = {
    'Program:exit'(node) {
      // The common module should be loaded in the first place.
      const notLoadedFirst = foundModules.indexOf(requiredModule) !== 0;
      if (notLoadedFirst) {
        context.report(
          node,
          'Mandatory module "{{moduleName}}" must be loaded ' +
          'before any other modules.',
          { moduleName: requiredModule }
        );
      }
    }
  };

  if (isESM) {
    rules.ImportDeclaration = (node) => {
      const requiredModuleName = getRequiredModuleName(node.source.value);
      if (requiredModuleName) {
        foundModules.push(requiredModuleName);
      }
    };
  } else {
    rules.CallExpression = (node) => {
      if (isRequireCall(node)) {
        const requiredModuleName = getRequiredModuleNameFromCall(node);

        if (requiredModuleName) {
          foundModules.push(requiredModuleName);
        }
      }
    };
  }

  return rules;
};
