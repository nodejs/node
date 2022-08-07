'use strict';

const { isTopLevelRequireCall } = require('./rules-utils.js');

const getRequiredPropertiesFromRequireCallExpression = (node) => {
  if (node?.parent?.id?.type === 'ObjectPattern') {
    return node.parent.id.properties;
  }
};

const isPrimordialsDeclarator = (node) => {
  return (
    node?.type === 'VariableDeclarator' && node?.init?.name === 'primordials'
  );
};

const getRequiredPropertiesFromPrimordialsDeclarator = (node) => {
  if (node?.id?.type === 'ObjectPattern') {
    return node.id.properties;
  }
};

const getPropertyValueName = (property) => property.value.name;
const compareProperty = (a, b) => (getPropertyValueName(a) > getPropertyValueName(b) ? 1 : -1);

const findOutOfOrderIndex = (properties) => {
  for (let i = 0; i < properties.length - 1; i++) {
    if (compareProperty(properties[i], properties[i + 1]) > 0) return i;
  }
  return -1;
};

module.exports = {
  meta: {
    fixable: 'code',
  },
  create(context) {
    const sourceCode = context.getSourceCode();
    const hasComment = (nodeOrToken) =>
      sourceCode.getCommentsBefore(nodeOrToken).length ||
    sourceCode.getCommentsAfter(nodeOrToken).length;

    const reportOutOfOrder = (target, requiredProperties, outOfOrderIndex) => {
      const before = requiredProperties[outOfOrderIndex];
      const after = requiredProperties[outOfOrderIndex + 1];
      context.report({
        node: target,
        message: `${after.value.name} should occur before ${before.value.name}`,
        fix(fixer) {
          if (requiredProperties.some(hasComment)) {
            return null;
          }
          return fixer.replaceTextRange([
            requiredProperties[0].range[0],
            requiredProperties[requiredProperties.length - 1].range[1],
          ], requiredProperties
          .slice()  // to avoid mutation
          .sort(compareProperty)
          .reduce((text, property, index) => {
            const suffix =
              index === requiredProperties.length - 1 ?
                '' :
                sourceCode
                    .getText()
                    .slice(
                      requiredProperties[index].range[1],
                      requiredProperties[index + 1].range[0]
                    );
            return text + sourceCode.getText(property) + suffix;
          }, ''));
        },
      });
    };

    return {
      CallExpression(node) {
        if (!isTopLevelRequireCall(node)) return;
        const requiredProperties =
          getRequiredPropertiesFromRequireCallExpression(node);
        if (!requiredProperties) return;
        const outOfOrderIndex = findOutOfOrderIndex(requiredProperties);
        if (outOfOrderIndex === -1) return;
        reportOutOfOrder(node, requiredProperties, outOfOrderIndex);
      },
      VariableDeclarator(node) {
        if (!isPrimordialsDeclarator(node)) return;
        const requiredProperties =
          getRequiredPropertiesFromPrimordialsDeclarator(node);
        if (!requiredProperties) return;
        const outOfOrderIndex = findOutOfOrderIndex(requiredProperties);
        if (outOfOrderIndex === -1) return;
        reportOutOfOrder(node, requiredProperties, outOfOrderIndex);
      },
    };
  },
};
