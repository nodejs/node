/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-file-name-consecutive-dashes
 * @fileoverview
 *   Warn when file names contain consecutive dashes.
 *
 * @example {"name": "plug-ins.md"}
 *
 * @example {"name": "plug--ins.md", "label": "output", "config": {"positionless": true}}
 *
 *   1:1: Do not use consecutive dashes in a file name
 */

'use strict';

var rule = require('unified-lint-rule');

module.exports = rule('remark-lint:no-file-name-consecutive-dashes', noFileNameConsecutiveDashes);

function noFileNameConsecutiveDashes(ast, file) {
  if (file.stem && /-{2,}/.test(file.stem)) {
    file.message('Do not use consecutive dashes in a file name');
  }
}
