/**
 * @fileoverview Enforce no duplicate props
 * @author Markus Ånöstam
 */

'use strict';

// ------------------------------------------------------------------------------
// Rule Definition
// ------------------------------------------------------------------------------

module.exports = function (context) {

  var configuration = context.options[0] || {};
  var ignoreCase = configuration.ignoreCase || false;

  return {
    JSXOpeningElement: function (node) {
      var props = {};

      node.attributes.forEach(function(decl) {
        if (decl.type === 'JSXSpreadAttribute') {
          return;
        }

        var name = decl.name.name;

        if (ignoreCase) {
          name = name.toLowerCase();
        }

        if (props.hasOwnProperty(name)) {
          context.report({
            node: decl,
            message: 'No duplicate props allowed'
          });
        } else {
          props[name] = 1;
        }
      });
    }
  };
};

module.exports.schema = [{
  type: 'object',
  properties: {
    ignoreCase: {
      type: 'boolean'
    }
  },
  additionalProperties: false
}];
