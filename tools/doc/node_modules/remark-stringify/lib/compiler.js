/**
 * @author Titus Wormer
 * @copyright 2015 Titus Wormer
 * @license MIT
 * @module remark:stringify
 * @fileoverview Markdown compiler
 */

'use strict';

/* Dependencies. */
var xtend = require('xtend');
var toggle = require('state-toggle');

/* Expose. */
module.exports = Compiler;

/**
 * Construct a new compiler.
 *
 * @constructor
 * @class {Compiler}
 * @param {File} file - Virtual file.
 */
function Compiler(tree, file) {
  this.inLink = false;
  this.inTable = false;
  this.tree = tree;
  this.file = file;
  this.options = xtend(this.options);
  this.setOptions({});
}

/* Cache prototype. */
var proto = Compiler.prototype;

/* Enter and exit helpers. */
proto.enterLink = toggle('inLink', false);
proto.enterTable = toggle('inTable', false);
proto.enterLinkReference = require('./util/enter-link-reference');

/* Configuration. */
proto.options = require('./defaults');
proto.setOptions = require('./set-options');

proto.compile = require('./macro/compile');
proto.visit = require('./macro/one');
proto.all = require('./macro/all');
proto.block = require('./macro/block');
proto.visitOrderedItems = require('./macro/ordered-items');
proto.visitUnorderedItems = require('./macro/unordered-items');

/* Expose visitors. */
proto.visitors = {
  root: require('./visitors/root'),
  text: require('./visitors/text'),
  heading: require('./visitors/heading'),
  paragraph: require('./visitors/paragraph'),
  blockquote: require('./visitors/blockquote'),
  list: require('./visitors/list'),
  listItem: require('./visitors/list-item'),
  inlineCode: require('./visitors/inline-code'),
  yaml: require('./visitors/yaml'),
  code: require('./visitors/code'),
  html: require('./visitors/html'),
  thematicBreak: require('./visitors/thematic-break'),
  strong: require('./visitors/strong'),
  emphasis: require('./visitors/emphasis'),
  break: require('./visitors/break'),
  delete: require('./visitors/delete'),
  link: require('./visitors/link'),
  linkReference: require('./visitors/link-reference'),
  imageReference: require('./visitors/image-reference'),
  definition: require('./visitors/definition'),
  image: require('./visitors/image'),
  footnote: require('./visitors/footnote'),
  footnoteReference: require('./visitors/footnote-reference'),
  footnoteDefinition: require('./visitors/footnote-definition'),
  table: require('./visitors/table'),
  tableCell: require('./visitors/table-cell')
};
