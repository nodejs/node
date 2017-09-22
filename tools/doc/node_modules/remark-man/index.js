'use strict';

var compiler = require('./lib/compiler.js');

module.exports = man;

function man(options) {
  this.Compiler = compiler(options || {});
}
