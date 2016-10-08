/**
 * @fileoverview Enforce PasalCase for user-defined JSX components
 * @author Jake Marsh
 */

'use strict';

// ------------------------------------------------------------------------------
// Constants
// ------------------------------------------------------------------------------

var PASCAL_CASE_REGEX = /^([A-Z0-9]|[A-Z0-9]+[a-z0-9]+(?:[A-Z0-9]+[a-z0-9]*)*)$/;
var COMPAT_TAG_REGEX = /^[a-z]|\-/;

// ------------------------------------------------------------------------------
// Rule Definition
// ------------------------------------------------------------------------------

module.exports = function(context) {

  return {
    JSXOpeningElement: function(node) {
      switch (node.name.type) {
        case 'JSXIdentifier':
          node = node.name;
          break;
        case 'JSXMemberExpression':
          node = node.name.object;
          break;
        case 'JSXNamespacedName':
          node = node.name.namespace;
          break;
        default:
          break;
      }

      var isPascalCase = PASCAL_CASE_REGEX.test(node.name);
      var isCompatTag = COMPAT_TAG_REGEX.test(node.name);

      if (!isPascalCase && !isCompatTag) {
        context.report({
          node: node,
          message: 'Imported JSX component ' + node.name + ' must be in PascalCase'
        });
      }
    }
  };

};
