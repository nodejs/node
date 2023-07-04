'use strict';

module.exports = {
  meta: {
    messages: {
      error: 'Add `__proto__: null` to object',
    },
    fixable: 'code',
  },
  create: function (context) {
    return {
      ObjectExpression(node) {
        // Not adding __proto__ to module.exports as it will break a lot of libraries
        if (isModuleExportsObject(node)) {
          return;
        }

        const properties = node.properties;
        let hasProto = false;

        for (const property of properties) {

          if (!property.key) {
            continue;
          }

          if (property.key.type === 'Identifier' && property.key.name === '__proto__') {
            hasProto = true;
            break;
          }

          if (property.key.type === 'Literal' && property.key.value === '__proto__') {
            hasProto = true;
            break;
          }
        }

        if (hasProto) {
          return;
        }

        if (properties.length > 0) {
          // If the object has properties but no __proto__ property
          context.report({
            node,
            message: 'Every object must have __proto__: null',
            fix: function (fixer) {
              // Generate the fix suggestion to add __proto__: null
              const sourceCode = context.getSourceCode();
              const firstProperty = properties[0];
              const firstPropertyToken = sourceCode.getFirstToken(firstProperty);

              let fixText = `__proto__: null`;

              if (properties.length > 1) {
                fixText += ',';

                if (properties[0].loc.start.line !== properties[1].loc.start.line) {
                  fixText += '\n';
                } else {
                  fixText += ' ';
                }
              }

              // Insert the fix suggestion before the first property
              return fixer.insertTextBefore(firstPropertyToken, fixText);
            },
          });
        }

        if (properties.length === 0) {
          // If the object is empty and missing __proto__
          context.report({
            node,
            message: 'Every empty object must have __proto__: null',
            fix: function (fixer) {
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

// Helper function to check if the object is `module.exports`
function isModuleExportsObject(node) {
  return (
    node.parent &&
    node.parent.type === 'AssignmentExpression' &&
    node.parent.left &&
    node.parent.left.type === 'MemberExpression' &&
    node.parent.left.object &&
    node.parent.left.object.name === 'module' &&
    node.parent.left.property &&
    node.parent.left.property.name === 'exports'
  );
}
