/**
 * @fileoverview Prevent usage of setState
 * @author Mark Dalgleish
 */
'use strict';

// ------------------------------------------------------------------------------
// Rule Definition
// ------------------------------------------------------------------------------

module.exports = function(context) {

  // --------------------------------------------------------------------------
  // Public
  // --------------------------------------------------------------------------

  return {

    CallExpression: function(node) {
      var callee = node.callee;
      if (callee.type !== 'MemberExpression') {
        return;
      }
      if (callee.object.type !== 'ThisExpression' || callee.property.name !== 'setState') {
        return;
      }
      var ancestors = context.getAncestors(callee);
      for (var i = 0, j = ancestors.length; i < j; i++) {
        if (ancestors[i].type === 'Property' || ancestors[i].type === 'MethodDefinition') {
          context.report({
            node: callee,
            message: 'Do not use setState'
          });
          break;
        }
      }
    }
  };

};

module.exports.schema = [];
