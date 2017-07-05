/**
 * @fileoverview Prohibit the use of non-ascii characters
 * @author Kalon Hinds
 */

/* eslint no-control-regex:0 */

'use strict';

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

const nonAsciiPattern = new RegExp('([^\x00-\x7F])', 'g');
const suggestions = {
  '’': '\'',
  '—': '-'
};
const reportError = ({line, column, character}, node, context) => {
  const suggestion = suggestions[character];

  let message = `Non-ASCII character ${character} detected.`;

  message = suggestion ?
    `${message} Consider replacing with: ${suggestion}` : message;

  context.report({
    node,
    message,
    loc: {
      line,
      column
    }
  });
};

module.exports = {
  create: (context) => {
    return {
      Program: (node) => {
        const source = context.getSourceCode();
        const sourceTokens = source.getTokens(node);
        const commentTokens = source.getAllComments();
        const tokens = sourceTokens.concat(commentTokens);

        tokens.forEach((token) => {
          const { value } = token;
          const matches = value.match(nonAsciiPattern);

          if (!matches) return;

          const { loc } = token;
          const character = matches[0];
          const column = loc.start.column + value.indexOf(character);

          reportError({
            line: loc.start.line,
            column,
            character
          }, node, context);
        });
      }
    };
  }
};
