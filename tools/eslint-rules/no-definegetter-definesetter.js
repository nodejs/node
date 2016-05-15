/**
 * @fileoverview Rule to flag usage of __defineGetter__ and __defineSetter__
 * @author Rich Trott
 */

'use strict';

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

module.exports = {
  create: function(context) {
    const disallowed = ['__defineGetter__', '__defineSetter__'];

    return {
      MemberExpression: function(node) {
        var prop;
        if (node.property) {
          if (node.property.type === 'Identifier' && !node.computed) {
            prop = node.property.name;
          } else if (node.property.type === 'Literal') {
            prop = node.property.value;
          }
          if (disallowed.includes(prop)) {
            context.report(node, `The ${prop} property is deprecated.`);
          }
        }
      }
    };
  }
};
