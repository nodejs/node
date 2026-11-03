'use strict';

const MESSAGE = 'Iterator result objects should place `done` before `value`.';

function getStaticPropertyName(property) {
  const { key } = property;

  if (!key) {
    return;
  }

  if (key.type === 'Identifier' && !property.computed) {
    return key.name;
  }

  if (key.type === 'Literal') {
    return key.value;
  }
}

module.exports = {
  meta: {
    type: 'suggestion',
    fixable: 'code',
    schema: [],
  },

  create(context) {
    const sourceCode = context.sourceCode;

    return {
      ObjectExpression(node) {
        let doneProperty;
        let valueProperty;

        for (const property of node.properties) {
          if (property.type !== 'Property') {
            continue;
          }

          switch (getStaticPropertyName(property)) {
            case 'done':
              doneProperty ??= property;
              break;
            case 'value':
              valueProperty ??= property;
              break;
          }
        }

        if (doneProperty && valueProperty && valueProperty.range[0] < doneProperty.range[0]) {
          context.report({
            node: valueProperty,
            message: MESSAGE,
            fix(fixer) {
              return [
                fixer.replaceText(valueProperty, sourceCode.getText(doneProperty)),
                fixer.replaceText(doneProperty, sourceCode.getText(valueProperty)),
              ];
            },
          });
        }
      },
    };
  },
};
