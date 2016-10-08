/**
 * @fileoverview Prevent variables used in JSX to be marked as unused
 * @author Yannick Croissant
 */
'use strict';

var variableUtil = require('../util/variable');

// ------------------------------------------------------------------------------
// Rule Definition
// ------------------------------------------------------------------------------

module.exports = function(context) {

  return {
    JSXExpressionContainer: function(node) {
      if (node.expression.type !== 'Identifier') {
        return;
      }
      variableUtil.markVariableAsUsed(context, node.expression.name);
    },

    JSXIdentifier: function(node) {
      if (node.parent.type === 'JSXAttribute') {
        return;
      }
      variableUtil.markVariableAsUsed(context, node.name);
    }

  };

};

module.exports.schema = [];
