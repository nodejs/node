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

        if (!hasProto) {
          context.report({
            node,
            message: 'Every object must have __proto__: null',
            fix: function (fixer) {
              // Generate the fix suggestion to add __proto__: null
              const sourceCode = context.getSourceCode();
              const lastProperty = properties[properties.length - 1];
              const lastPropertyToken = sourceCode.getLastToken(lastProperty);
              const fixText = `__proto__: null`;

              // Insert the fix suggestion after the last property
              return fixer.insertTextAfter(lastPropertyToken, `, ${fixText}`);
            },
          });
        }
      },
    };
  },
};
