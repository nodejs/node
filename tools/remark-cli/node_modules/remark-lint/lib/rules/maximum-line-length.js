/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module maximum-line-length
 * @fileoverview
 *   Warn when lines are too long.
 *
 *   Options: `number`, default: `80`.
 *
 *   Ignores nodes which cannot be wrapped, such as heasings, tables,
 *   code, link, images, and definitions.
 *
 * @example {"name": "valid.md", "setting": 80, "config": {"positionless": true}}
 *
 *   This line is simply not toooooooooooooooooooooooooooooooooooooooooooo
 *   long.
 *
 *   This is also fine: <http://this-long-url-with-a-long-domain.co.uk/a-long-path?query=variables>
 *
 *   <http://this-link-is-fine.com>
 *
 *   [foo](http://this-long-url-with-a-long-domain-is-valid.co.uk/a-long-path?query=variables)
 *
 *   <http://this-long-url-with-a-long-domain-is-valid.co.uk/a-long-path?query=variables>
 *
 *   ![foo](http://this-long-url-with-a-long-domain-is-valid.co.uk/a-long-path?query=variables)
 *
 *   | An | exception | is | line | length | in | long | tables | because | those | can’t | just |
 *   | -- | --------- | -- | ---- | ------ | -- | ---- | ------ | ------- | ----- | ----- | ---- |
 *   | be | helped    |    |      |        |    |      |        |         |       |       | .    |
 *
 *   The following is also fine, because there is no white-space.
 *
 *   <http://this-long-url-with-a-long-domain-is-invalid.co.uk/a-long-path?query=variables>.
 *
 *   In addition, definitions are also fine:
 *
 *   [foo]: <http://this-long-url-with-a-long-domain-is-invalid.co.uk/a-long-path?query=variables>
 *
 * @example {"name": "invalid.md", "setting": 80, "label": "input", "config": {"positionless": true}}
 *
 *   This line is simply not tooooooooooooooooooooooooooooooooooooooooooooooooooooooo
 *   long.
 *
 *   Just like thiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiis one.
 *
 *   And this one is also very wrong: because the link starts aaaaaaafter the column: <http://line.com>
 *
 *   <http://this-long-url-with-a-long-domain-is-invalid.co.uk/a-long-path?query=variables> and such.
 *
 * @example {"name": "invalid.md", "setting": 80, "label": "output", "config": {"positionless": true}}
 *
 *   4:86: Line must be at most 80 characters
 *   6:99: Line must be at most 80 characters
 *   8:97: Line must be at most 80 characters
 */

'use strict';

/* Dependencies. */
var visit = require('unist-util-visit');
var position = require('unist-util-position');

/* Expose. */
module.exports = maximumLineLength;

/* Methods. */
var start = position.start;
var end = position.end;

/**
 * Warn when lines are too long.  This rule is forgiving
 * about lines which cannot be wrapped, such as code,
 * tables, and headings, or links at the enc of a line.
 *
 * @param {Node} ast - Root node.
 * @param {File} file - Virtual file.
 * @param {number?} [preferred=80] - Maximum line length.
 */
function maximumLineLength(ast, file, preferred) {
  var style = preferred && preferred !== true ? preferred : 80;
  var content = file.toString();
  var matrix = content.split('\n');
  var index = -1;
  var length = matrix.length;
  var lineLength;

  /* Next, white list nodes which cannot be wrapped. */
  visit(ast, function (node) {
    var applicable = isIgnored(node);
    var initial = applicable && start(node).line;
    var final = applicable && end(node).line;

    if (!applicable || position.generated(node)) {
      return;
    }

    whitelist(initial - 1, final);
  });

  visit(ast, 'link', validateLink);
  visit(ast, 'image', validateLink);

  /* Iterate over every line, and warn for
   * violating lines. */
  while (++index < length) {
    lineLength = matrix[index].length;

    if (lineLength > style) {
      file.message('Line must be at most ' + style + ' characters', {
        line: index + 1,
        column: lineLength + 1
      });
    }
  }

  return;

  /**
   * Whitelist from `initial` to `final`, zero-based.
   *
   * @param {number} initial - Start.
   * @param {number} final - End.
   */
  function whitelist(initial, final) {
    initial--;

    while (++initial < final) {
      matrix[initial] = '';
    }
  }

  /**
   * Finally, whitelist URLs, but only if they occur at
   * or after the wrap.  However, when they do, and
   * there’s white-space after it, they are not
   * whitelisted.
   *
   * @param {Node} node - Node.
   * @param {number} pos - Position of `node` in `parent`.
   * @param {Node} parent - Parent of `node`.
   */
  function validateLink(node, pos, parent) {
    var next = parent.children[pos + 1];
    var initial = start(node);
    var final = end(node);

    /* Nothing to whitelist when generated. */
    if (position.generated(node)) {
      return;
    }

    /* No whitelisting when starting after the border,
     * or ending before it. */
    if (initial.column > style || final.column < style) {
      return;
    }

    /* No whitelisting when there’s white-space after
     * the link. */
    if (
      next &&
      start(next).line === initial.line &&
      (!next.value || /^(.+?[ \t].+?)/.test(next.value))
    ) {
      return;
    }

    whitelist(initial.line - 1, final.line);
  }
}

/**
 * Check if `node` is applicable, as in, if it should be
 * ignored.
 *
 * @param {Node} node - Node to test.
 * @return {boolean} - Whether or not `node` should be
 *   ignored.
 */
function isIgnored(node) {
  return node.type === 'heading' ||
    node.type === 'table' ||
    node.type === 'code' ||
    node.type === 'definition';
}
