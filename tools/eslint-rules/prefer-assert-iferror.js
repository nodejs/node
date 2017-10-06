/**
 * @fileoverview Prohibit the `if (err) throw err;` pattern
 * @author Teddy Katz
 */

'use strict';

module.exports = {
  create(context) {
    const sourceCode = context.getSourceCode();

    function hasSameTokens(nodeA, nodeB) {
      const aTokens = sourceCode.getTokens(nodeA);
      const bTokens = sourceCode.getTokens(nodeB);

      return aTokens.length === bTokens.length &&
        aTokens.every((token, index) => {
          return token.type === bTokens[index].type &&
            token.value === bTokens[index].value;
        });
    }

    return {
      IfStatement(node) {
        const firstStatement = node.consequent.type === 'BlockStatement' ?
          node.consequent.body[0] :
          node.consequent;
        if (
          firstStatement &&
          firstStatement.type === 'ThrowStatement' &&
          hasSameTokens(node.test, firstStatement.argument)
        ) {
          context.report({
            node: firstStatement,
            message: 'Use assert.ifError({{argument}}) instead.',
            data: { argument: sourceCode.getText(node.test) }
          });
        }
      }
    };
  }
};
