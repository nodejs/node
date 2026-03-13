'use strict';
require('sys');  // Builtin should not show up in module.children array.
require('./b');  // This should.
require('./b');  // This should not.
module.exports = module.children.slice();
