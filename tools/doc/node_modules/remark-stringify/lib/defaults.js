/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:stringify:defaults
 * @fileoverview Default options for `stringify`.
 */

'use strict';

module.exports = {
  gfm: true,
  commonmark: false,
  pedantic: false,
  entities: 'false',
  setext: false,
  closeAtx: false,
  looseTable: false,
  spacedTable: true,
  paddedTable: true,
  incrementListMarker: true,
  fences: false,
  fence: '`',
  bullet: '-',
  listItemIndent: 'tab',
  rule: '*',
  ruleSpaces: true,
  ruleRepetition: 3,
  strong: '*',
  emphasis: '_'
};
