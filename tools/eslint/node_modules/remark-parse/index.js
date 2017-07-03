'use strict';

var unherit = require('unherit');
var xtend = require('xtend');
var Parser = require('./lib/parser.js');

module.exports = parse;
parse.Parser = Parser;

function parse(options) {
  var Local = unherit(Parser);
  Local.prototype.options = xtend(Local.prototype.options, this.data('settings'), options);
  this.Parser = Local;
}
