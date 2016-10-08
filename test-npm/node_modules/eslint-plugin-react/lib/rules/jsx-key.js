/**
 * @fileoverview Report missing `key` props in iterators/collection literals.
 * @author Ben Mosher
 */
'use strict';

// var Components = require('../util/Components');

// ------------------------------------------------------------------------------
// Rule Definition
// ------------------------------------------------------------------------------

module.exports = function(context) {

  function hasKeyProp(node) {
    return node.openingElement.attributes.some(function(decl) {
      if (decl.type === 'JSXSpreadAttribute') {
        return false;
      }
      return (decl.name.name === 'key');
    });
  }

  function checkIteratorElement(node) {
    if (node.type === 'JSXElement' && !hasKeyProp(node)) {
      context.report({
        node: node,
        message: 'Missing "key" prop for element in iterator'
      });
    }
  }

  function getReturnStatement(body) {
    return body.filter(function(item) {
      return item.type === 'ReturnStatement';
    })[0];
  }

  return {
    JSXElement: function(node) {
      if (hasKeyProp(node)) {
        return;
      }

      if (node.parent.type === 'ArrayExpression') {
        context.report({
          node: node,
          message: 'Missing "key" prop for element in array'
        });
      }
    },

    // Array.prototype.map
    CallExpression: function (node) {
      if (node.callee && node.callee.type !== 'MemberExpression') {
        return;
      }

      if (node.callee && node.callee.property && node.callee.property.name !== 'map') {
        return;
      }

      var fn = node.arguments[0];
      var isFn = fn && fn.type === 'FunctionExpression';
      var isArrFn = fn && fn.type === 'ArrowFunctionExpression';

      if (isArrFn && fn.body.type === 'JSXElement') {
        checkIteratorElement(fn.body);
      }

      if (isFn || isArrFn) {
        if (fn.body.type === 'BlockStatement') {
          var returnStatement = getReturnStatement(fn.body.body);
          if (returnStatement && returnStatement.argument) {
            checkIteratorElement(returnStatement.argument);
          }
        }
      }
    }
  };
};

module.exports.schema = [];
