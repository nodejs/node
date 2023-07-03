'use strict';

module.exports = {
  meta: {
    messages: {
      error: 'Add `__proto__: null` to object',
    },
    fixable: 'code',
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

        if (!hasProto && properties.length > 0) {
          // If the object has properties but no __proto__ property
          context.report({
            node,
            message: 'Every object must have __proto__: null',
            fix: function(fixer) {
              // Generate the fix suggestion to add __proto__: null
              const sourceCode = context.getSourceCode();
              const firstProperty = properties[0];
              const firstPropertyToken = sourceCode.getFirstToken(firstProperty);
              const fixText = '__proto__: null, ';

              // Insert the fix suggestion before the first property
              return fixer.insertTextBefore(firstPropertyToken, fixText);
            },
          });
        } else if (!hasProto && properties.length === 0) {
          // If the object is empty and missing __proto__
          context.report({
            node,
            message: 'Every empty object must have __proto__: null',
            fix: function(fixer) {
              // Generate the fix suggestion to create the object with __proto__: null
              const fixText = '{ __proto__: null }';

              // Replace the empty object with the fix suggestion
              return fixer.replaceText(node, fixText);
            },
          });
        }
      },
    };
  },
};
