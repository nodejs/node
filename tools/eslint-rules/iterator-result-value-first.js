'use strict';

// V8 has a fast path for `{ value, done }` literals, but it must be that order.
// https://source.chromium.org/chromium/chromium/src/+/main:v8/src/init/bootstrapper.cc;l=746-754;drc=e8ac4914ee6b24442aed773d3a041451692b6a9c
// Also, the ES spec technically requires that order.
// https://tc39.es/ecma262/multipage/abstract-operations.html#sec-createiterresultobject
const MESSAGE = 'Iterator result objects should place `value` before `done`.';

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

        if (doneProperty && valueProperty && doneProperty.range[0] < valueProperty.range[0]) {
          context.report({
            node: doneProperty,
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
