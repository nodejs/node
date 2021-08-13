'use strict';

var rule = require('unified-lint-rule');

module.exports = rule('remark-lint:no-trailing-spaces', noTrailingSpaces);

/**
 * Lines that are just space characters are not present in
 * the AST, which is why we loop through lines manually.
 */

function noTrailingSpaces(ast, file) {
  var lines = file.toString().split(/\r?\n/);
  for (var i = 0; i < lines.length; i++) {
    var currentLine = lines[i];
    var lineIndex = i + 1;
    if (/\s$/.test(currentLine)) {
      file.message('Remove trailing whitespace', {
        position: {
          start: { line: lineIndex, column: currentLine.length + 1 },
          end: { line: lineIndex }
        }
      });
    }
  }
}
