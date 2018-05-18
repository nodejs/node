/**
 * @fileoverview Look for regex literal in set of repl and its dependency files
 * @author Prince J Wesley
 */
'use strict';

//------------------------------------------------------------------------------
// Rule Definition
//------------------------------------------------------------------------------

// Exclude file list
// Make sure that all exclude files has RegExp instance
//  from require('internal/util').getInternalGlobal()

// this linter rule prevents accidental access to global regexp literal

const fileNames = [
  'lib/internal/repl.js',
  'lib/internal/readline.js',
  'lib/internal/tty.js',
  'lib/repl.js',
  'lib/readline.js',
  'lib/tty.js'
];

const isExcludedFile = (name) =>
  !!fileNames.find((n) => name.indexOf(n) !== -1);

module.exports = function(context) {
  const sourceCode = context.getSourceCode();
  const filename = context.getFilename();
  const excluded = isExcludedFile(filename);

  function report(node) {
    context.report({
      node,
      message: `Global RegExp literal is not allowed in ${filename}`,
      fix(fixer) {
        const regex = node.regex;
        const pattern = regex.pattern.replace(/\\/g, '\\\\');
        let replaceText = `new RegExp('${pattern}'`;
        replaceText += regex.flags ? `, '${regex.flags}')` : ')';
        return fixer.replaceText(node, replaceText);
      }
    });
  }

  function checkLiteral(node) {
    if (!excluded) return;

    const { type } = sourceCode.getFirstToken(node);
    if (type === 'RegularExpression') {
      report(node);
    }

  }

  return {
    Literal: checkLiteral
  };
};
