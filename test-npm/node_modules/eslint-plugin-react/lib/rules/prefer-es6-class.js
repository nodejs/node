/**
 * @fileoverview Enforce ES5 or ES6 class for React Components
 * @author Dan Hamilton
 */
'use strict';

var Components = require('../util/Components');

// ------------------------------------------------------------------------------
// Rule Definition
// ------------------------------------------------------------------------------

module.exports = Components.detect(function(context, components, utils) {
  var configuration = context.options[0] || 'always';

  return {
    ObjectExpression: function(node) {
      if (utils.isES5Component(node) && configuration === 'always') {
        context.report({
          node: node,
          message: 'Component should use es6 class instead of createClass'
        });
      }
    },
    ClassDeclaration: function(node) {
      if (utils.isES6Component(node) && configuration === 'never') {
        context.report({
          node: node,
          message: 'Component should use createClass instead of es6 class'
        });
      }
    }
  };
});

module.exports.schema = [{
  enum: ['always', 'never']
}];
