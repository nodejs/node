'use strict';

var utils = require('../utils.js');
require('eslint-visitor-keys');
require('espree');

var newParens = utils.createRule({
  name: "new-parens",
  package: "js",
  meta: {
    type: "layout",
    docs: {
      description: "Enforce or disallow parentheses when invoking a constructor with no arguments"
    },
    fixable: "code",
    schema: [
      {
        type: "string",
        enum: ["always", "never"]
      }
    ],
    messages: {
      missing: "Missing '()' invoking a constructor.",
      unnecessary: "Unnecessary '()' invoking a constructor with no arguments."
    }
  },
  create(context) {
    const options = context.options;
    const always = options[0] !== "never";
    const sourceCode = context.sourceCode;
    return {
      NewExpression(node) {
        if (node.arguments.length !== 0)
          return;
        const lastToken = sourceCode.getLastToken(node);
        const hasLastParen = lastToken && utils.isClosingParenToken(lastToken);
        const tokenBeforeLastToken = sourceCode.getTokenBefore(lastToken);
        const hasParens = hasLastParen && utils.isOpeningParenToken(tokenBeforeLastToken) && node.callee.range[1] < node.range[1];
        if (always) {
          if (!hasParens) {
            context.report({
              node,
              messageId: "missing",
              fix: (fixer) => fixer.insertTextAfter(node, "()")
            });
          }
        } else {
          if (hasParens) {
            context.report({
              node,
              messageId: "unnecessary",
              fix: (fixer) => [
                fixer.remove(tokenBeforeLastToken),
                fixer.remove(lastToken),
                fixer.insertTextBefore(node, "("),
                fixer.insertTextAfter(node, ")")
              ]
            });
          }
        }
      }
    };
  }
});

module.exports = newParens;
