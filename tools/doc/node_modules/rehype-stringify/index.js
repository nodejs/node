'use strict';

var xtend = require('xtend');
var toHTML = require('hast-util-to-html');

module.exports = stringify;

function stringify(config) {
  var settings = xtend(config, this.data('settings'));

  this.Compiler = compiler;

  function compiler(tree) {
    return toHTML(tree, settings);
  }
}
