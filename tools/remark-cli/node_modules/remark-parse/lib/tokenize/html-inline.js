/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:parse:tokenize:html-inline
 * @fileoverview Tokenise inline HTML.
 */

'use strict';

/* Dependencies. */
var locate = require('../locate/tag');
var cdata = require('../util/match-cdata');
var comment = require('../util/match-comment');
var declaration = require('../util/match-declaration');
var instruction = require('../util/match-instruction');
var closing = require('../util/match-tag-closing');
var opening = require('../util/match-tag-opening');

/* Expose. */
module.exports = inlineHTML;
inlineHTML.locator = locate;

/* Constants. */
var EXPRESSION_HTML_LINK_OPEN = /^<a /i;
var EXPRESSION_HTML_LINK_CLOSE = /^<\/a>/i;

/**
 * Tokenise inline HTML.
 *
 * @property {Function} locator.
 * @param {function(string)} eat - Eater.
 * @param {string} value - Rest of content.
 * @param {boolean?} [silent] - Whether this is a dry run.
 * @return {Node?|boolean} - `html` node.
 */
function inlineHTML(eat, value, silent) {
  var self = this;
  var subvalue = comment(value, self.options) ||
    cdata(value) ||
    instruction(value) ||
    declaration(value) ||
    closing(value) ||
    opening(value);

  if (!subvalue) {
    return;
  }

  /* istanbul ignore if - never used (yet) */
  if (silent) {
    return true;
  }

  if (!self.inLink && EXPRESSION_HTML_LINK_OPEN.test(subvalue)) {
    self.inLink = true;
  } else if (self.inLink && EXPRESSION_HTML_LINK_CLOSE.test(subvalue)) {
    self.inLink = false;
  }

  return eat(subvalue)({
    type: 'html',
    value: subvalue
  });
}
