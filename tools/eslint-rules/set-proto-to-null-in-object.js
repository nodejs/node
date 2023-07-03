'use strict';

module.exports = {
  meta: {
    messages: {
      error: 'Add `__proto__: null` to object',
    },
  },
  create: function(context) {
    return {
      ObjectExpression(node) {
        const properties = node.properties;
        let hasProto = false;

        for (const property of properties) {
          if (property.key && property.key.name === '__proto__') {
            hasProto = true;
            break;
          }
        }

        if (!hasProto) {
          context.report({
            node,
            message: 'Every object must have __proto__: null',
          });
        }
      },
    };
  },
};
