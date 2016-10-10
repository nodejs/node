/**
 * @fileoverview Enforce ES5 or ES6 class for returning value in render function.
 * @author Mark Orel
 */
'use strict';

var Components = require('../util/Components');

// ------------------------------------------------------------------------------
// Rule Definition
// ------------------------------------------------------------------------------

module.exports = Components.detect(function(context) {
  /**
   * Helper for checking if parent node is render method.
   * @param {ASTNode} node to check
   * @returns {boolean} If previous node is method and name is render returns true, otherwise false;
   */
  function checkParent(node) {
    return (node.parent.type === 'Property' || node.parent.type === 'MethodDefinition')
        && node.parent.key
        && node.parent.key.name === 'render';
  }

  /**
   * Helper for checking if child node exists and if it's ReturnStatement
   * @param {ASTNode} node to check
   * @returns {boolean} True if ReturnStatement exists, otherwise false
   */
  function checkReturnStatementExistence(node) {
    if (!node.body && !node.body.body && !node.body.body.length) {
      return false;
    }

    var hasReturnStatement = node.body.body.some(function(elem) {
      return elem.type === 'ReturnStatement';
    });

    return hasReturnStatement;
  }


  return {
    FunctionExpression: function(node) {
      if (checkParent(node) && !checkReturnStatementExistence(node)) {
        context.report({
          node: node,
          message: 'Your render method should have return statement'
        });
      }
    }
  };
});

module.exports.schema = [{}];
