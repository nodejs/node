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

const nonAsciiSelector = 'Literal[value=/[^\n\x20-\x7e]/]';
const nonAsciiRegexPattern = new RegExp(/[^\n\x20-\x7e]/);
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

  const reportError = (node) => {

    const nodeValue = node.value;
    const offendingCharacter = nodeValue.match(nonAsciiRegexPattern)[0];
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
    [nonAsciiSelector]: (node) => reportError(node, context)
  };
};
