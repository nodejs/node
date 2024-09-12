'use strict';

function create(context) {
  const sourceCode = context.getSourceCode();
  const nonAsciiPattern = /[^\r\n\x20-\x7e]/g;

  return {
    Program(node) {
      sourceCode.getTokens(node).forEach((token) => {
        const match = token.value.match(nonAsciiPattern);
        if (match) {
          context.report({
            node,
            loc: token.loc.start,
            message: `Non-ASCII character "${match[0]}" found`,
          });
        }
      });
    },
  };
}

module.exports = { create };
