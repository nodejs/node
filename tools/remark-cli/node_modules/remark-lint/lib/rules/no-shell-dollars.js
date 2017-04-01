/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-shell-dollars
 * @fileoverview
 *   Warn when shell code is prefixed by dollar-characters.
 *
 *   Ignored indented code blocks and fenced code blocks without language
 *   flag.
 *
 * @example {"name": "valid.md"}
 *
 *   ```sh
 *   echo a
 *   echo a > file
 *   ```
 *
 *   ```zsh
 *   $ echo a
 *   a
 *   $ echo a > file
 *   ```
 *
 * @example {"name": "invalid.md", "label": "input"}
 *
 *   ```bash
 *   $ echo a
 *   $ echo a > file
 *   ```
 *
 * @example {"name": "invalid.md", "label": "output"}
 *
 *   1:1-4:4: Do not use dollar signs before shell-commands
 */

'use strict';

/* Dependencies. */
var visit = require('unist-util-visit');
var position = require('unist-util-position');

/* Expose. */
module.exports = noShellDollars;

/* List of shell script file extensions (also used as code
 * flags for syntax highlighting on GitHub):
 * https://github.com/github/linguist/blob/5bf8cf5/lib/
 * linguist/languages.yml#L3002. */
var flags = [
  'sh',
  'bash',
  'bats',
  'cgi',
  'command',
  'fcgi',
  'ksh',
  'tmux',
  'tool',
  'zsh'
];

/**
 * Warn when shell code is prefixed by dollar-characters.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 */
function noShellDollars(ast, file) {
  visit(ast, 'code', function (node) {
    var language = node.lang;
    var value = node.value;
    var warn;

    if (!language || position.generated(node)) {
      return;
    }

    /* Check both known shell-code and unknown code. */
    if (flags.indexOf(language) !== -1) {
      warn = value.length && value.split('\n').every(function (line) {
        return Boolean(!line.trim() || line.match(/^\s*\$\s*/));
      });

      if (warn) {
        file.message('Do not use dollar signs before shell-commands', node);
      }
    }
  });
}
