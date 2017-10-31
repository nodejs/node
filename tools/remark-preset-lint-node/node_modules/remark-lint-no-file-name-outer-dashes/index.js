/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module no-file-name-outer-dashes
 * @fileoverview
 *   Warn when file names contain initial or final dashes.
 *
 * @example {"name": "readme.md"}
 *
 * @example {"name": "-readme.md", "label": "output", "config": {"positionless": true}}
 *
 *   1:1: Do not use initial or final dashes in a file name
 *
 * @example {"name": "readme-.md", "label": "output", "config": {"positionless": true}}
 *
 *   1:1: Do not use initial or final dashes in a file name
 */

'use strict';

var rule = require('unified-lint-rule');

module.exports = rule('remark-lint:no-file-name-outer-dashes', noFileNameOuterDashes);

function noFileNameOuterDashes(ast, file) {
  if (file.stem && /^-|-$/.test(file.stem)) {
    file.message('Do not use initial or final dashes in a file name');
  }
}
