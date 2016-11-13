/**
 * @fileoverview Disallow useless escape in regex character class
 * Based on 'no-useless-escape' rule
 */
'use strict';

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

const REGEX_CHARCLASS_ESCAPES = new Set('\\bcdDfnrsStvwWxu0123456789]');

/**
 * Parses a regular expression into a list of regex character class list.
 * @param {string} regExpText raw text used to create the regular expression
 * @returns {Object[]} A list of character classes tokens with index and
 *   escape info
 * @example
 *
 * parseRegExpCharClass('a\\b[cd-]')
 *
 * returns:
 * [
 *   {
 *     empty: false,
 *     start: 4,
 *     end: 6,
 *     chars: [
 *       {text: 'c', index: 4, escaped: false},
 *       {text: 'd', index: 5, escaped: false},
 *       {text: '-', index: 6, escaped: false}
 *     ]
 *   }
 * ]
 */

function parseRegExpCharClass(regExpText) {
  const charList = [];
  let charListIdx = -1;
  const initState = {
    escapeNextChar: false,
    inCharClass: false,
    startingCharClass: false
  };

  regExpText.split('').reduce((state, char, index) => {
    if (!state.escapeNextChar) {
      if (char === '\\') {
        return Object.assign(state, { escapeNextChar: true });
      }
      if (char === '[' && !state.inCharClass) {
        charListIdx += 1;
        charList.push({ start: index + 1, chars: [], end: -1 });
        return Object.assign(state, {
          inCharClass: true,
          startingCharClass: true
        });
      }
      if (char === ']' && state.inCharClass) {
        const charClass = charList[charListIdx];
        charClass.empty = charClass.chars.length === 0;
        if (charClass.empty) {
          charClass.start = charClass.end = -1;
        } else {
          charList[charListIdx].end = index - 1;
        }
        return Object.assign(state, {
          inCharClass: false,
          startingCharClass: false
        });
      }
    }
    if (state.inCharClass) {
      charList[charListIdx].chars.push({
        text: char,
        index, escaped:
        state.escapeNextChar
      });
    }
    return Object.assign(state, {
      escapeNextChar: false,
      startingCharClass: false
    });
  }, initState);

  return charList;
}

module.exports = {
  meta: {
    docs: {
      description: 'disallow unnecessary regex characer class escape sequences',
      category: 'Best Practices',
      recommended: false
    },
    fixable: 'code',
    schema: [{
      'type': 'object',
      'properties': {
        'override': {
          'type': 'array',
          'items': { 'type': 'string' },
          'uniqueItems': true
        }
      },
      'additionalProperties': false
    }]
  },

  create(context) {
    const overrideSet = new Set(context.options.length
                                  ? context.options[0].override || []
                                  : []);

    /**
     * Reports a node
     * @param {ASTNode} node The node to report
     * @param {number} startOffset The backslash's offset
     *   from the start of the node
     * @param {string} character The uselessly escaped character
     *   (not including the backslash)
     * @returns {void}
     */
    function report(node, startOffset, character) {
      context.report({
        node,
        loc: {
          line: node.loc.start.line,
          column: node.loc.start.column + startOffset
        },
        message: 'Unnecessary regex escape in character' +
                 ' class: \\{{character}}',
        data: { character },
        fix: (fixer) => {
          const start = node.range[0] + startOffset;
          return fixer.replaceTextRange([start, start + 1], '');
        }
      });
    }

    /**
     * Checks if a node has superflous escape character
     *   in regex character class.
     *
     * @param {ASTNode} node - node to check.
     * @returns {void}
     */
    function check(node) {
      if (node.regex) {
        parseRegExpCharClass(node.regex.pattern)
          .forEach((charClass) => {
            charClass
              .chars
              // The '-' character is a special case if is not at
              // either edge of the character class. To account for this,
              // filter out '-' characters that appear in the middle of a
              // character class.
              .filter((charInfo) => !(charInfo.text === '-' &&
                                      (charInfo.index !== charClass.start &&
                                       charInfo.index !== charClass.end)))

              // The '^' character is a special case if it's at the beginning
              // of the character class. To account for this, filter out '^'
              // characters that appear at the start of a character class.
              //
              .filter((charInfo) => !(charInfo.text === '^' &&
                                    charInfo.index === charClass.start))

              // Filter out characters that aren't escaped.
              .filter((charInfo) => charInfo.escaped)

              // Filter out characters that are valid to escape, based on
              // their position in the regular expression.
              .filter((charInfo) => !REGEX_CHARCLASS_ESCAPES.has(charInfo.text))

              // Filter out overridden character list.
              .filter((charInfo) => !overrideSet.has(charInfo.text))

              // Report all the remaining characters.
              .forEach((charInfo) =>
                report(node, charInfo.index, charInfo.text));
          });
      }
    }

    return {
      Literal: check
    };
  }
};
