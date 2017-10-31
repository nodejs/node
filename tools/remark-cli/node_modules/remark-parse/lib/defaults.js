/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:parse:defaults
 * @fileoverview Default options for `parse`.
 */

'use strict';

/* Expose. */
module.exports = {
  position: true,
  gfm: true,
  yaml: true,
  commonmark: false,
  footnotes: false,
  pedantic: false,
  blocks: require('./block-elements'),
  breaks: false
};
