/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:stringify:visitors:thematic-break
 * @fileoverview Stringify a thematic-break.
 */

'use strict';

/* Dependencies. */
var repeat = require('repeat-string');

/* Expose. */
module.exports = thematic;

/**
 * Stringify a `thematic-break`.
 *
 * The character used is configurable through `rule`: (`'_'`)
 *
 *     ___
 *
 * The number of repititions is defined through
 * `ruleRepetition`: (`6`)
 *
 *     ******
 *
 * Whether spaces delimit each character, is configured
 * through `ruleSpaces`: (`true`)
 *
 *     * * *
 *
 * @return {string} - Markdown thematic break.
 */
function thematic() {
  var options = this.options;
  var rule = repeat(options.rule, options.ruleRepetition);
  return options.ruleSpaces ? rule.split('').join(' ') : rule;
}
