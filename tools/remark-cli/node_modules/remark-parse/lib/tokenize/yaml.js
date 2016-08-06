/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:parse:tokenize:yaml
 * @fileoverview Tokenise YAML.
 */

'use strict';

/* Expose. */
module.exports = yaml;
yaml.onlyAtStart = true;

/* Constants */
var FENCE = '---';
var C_DASH = '-';
var C_NEWLINE = '\n';

/**
 * Tokenise YAML.
 *
 * @property {Function} locator.
 * @param {function(string)} eat - Eater.
 * @param {string} value - Rest of content.
 * @param {boolean?} [silent] - Whether this is a dry run.
 * @return {Node?|boolean} - `yaml` node.
 */
function yaml(eat, value, silent) {
  var self = this;
  var subvalue;
  var content;
  var index;
  var length;
  var character;
  var queue;

  if (
    !self.options.yaml ||
    value.charAt(0) !== C_DASH ||
    value.charAt(1) !== C_DASH ||
    value.charAt(2) !== C_DASH ||
    value.charAt(3) !== C_NEWLINE
  ) {
    return;
  }

  subvalue = FENCE + C_NEWLINE;
  content = queue = '';
  index = 3;
  length = value.length;

  while (++index < length) {
    character = value.charAt(index);

    if (
      character === C_DASH &&
      (queue || !content) &&
      value.charAt(index + 1) === C_DASH &&
      value.charAt(index + 2) === C_DASH
    ) {
      /* istanbul ignore if - never used (yet) */
      if (silent) {
        return true;
      }

      subvalue += queue + FENCE;

      return eat(subvalue)({
        type: 'yaml',
        value: content
      });
    }

    if (character === C_NEWLINE) {
      queue += character;
    } else {
      subvalue += queue + character;
      content += queue + character;
      queue = '';
    }
  }
}
