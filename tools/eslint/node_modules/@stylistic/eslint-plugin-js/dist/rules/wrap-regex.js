'use strict';

var utils = require('../utils.js');
require('eslint-visitor-keys');
require('espree');

var wrapRegex = utils.createRule({
  name: "wrap-regex",
  package: "js",
  meta: {
    type: "layout",
    docs: {
      description: "Require parenthesis around regex literals"
    },
    schema: [],
    fixable: "code",
    messages: {
      requireParens: "Wrap the regexp literal in parens to disambiguate the slash."
    }
  },
  create(context) {
    const sourceCode = context.sourceCode;
    return {
      Literal(node) {
        const token = sourceCode.getFirstToken(node);
        const nodeType = token.type;
        if (nodeType === "RegularExpression") {
          const beforeToken = sourceCode.getTokenBefore(node);
          const afterToken = sourceCode.getTokenAfter(node);
          const { parent } = node;
          if (parent.type === "MemberExpression" && parent.object === node && !(beforeToken && beforeToken.value === "(" && afterToken && afterToken.value === ")")) {
            context.report({
              node,
              messageId: "requireParens",
              fix: (fixer) => fixer.replaceText(node, `(${sourceCode.getText(node)})`)
            });
          }
        }
      }
    };
  }
});

module.exports = wrapRegex;
