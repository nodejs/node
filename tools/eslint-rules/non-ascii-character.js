/**
 * @fileOverview Any non-ASCII characters in lib/ will increase the size
 *               of the compiled node binary. This linter rule ensures that
 *               any such character is reported.
 * @author Sarat Addepalli <sarat.addepalli@gmail.com>
 */

'use strict';

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

const nonAsciiRegexPattern = new RegExp(/[^\r\n\x20-\x7e]/);
const suggestions = {
  '’': '\'',
  '‛': '\'',
  '‘': '\'',
  '“': '"',
  '‟': '"',
  '”': '"',
  '«': '"',
  '»': '"',
  '—': '-'
};

module.exports = (context) => {

  const reportIfError = (node, token) => {

    const { value } = token;
    const matches = value.match(nonAsciiRegexPattern);

    if (!matches) return;

    const offendingCharacter = matches[0];
    const suggestion = suggestions[offendingCharacter];

    let message = `Non-ASCII character '${offendingCharacter}' detected.`;

    message = suggestion ?
      `${message} Consider replacing with: ${suggestion}` :
      message;

    context.report({
      node,
      message,
      fix: (fixer) => {
        return fixer.replaceText(
          node,
          suggestion ? `${suggestion}` : ''
        );
      }
    });
  };

  return {
    Program: (node) => {
      const source = context.getSourceCode();
      const sourceTokens = source.getTokens(node);
      const commentTokens = source.getAllComments();
      const tokens = sourceTokens.concat(commentTokens);

      tokens.forEach((token) => reportIfError(node, token));
    }
  };
};
