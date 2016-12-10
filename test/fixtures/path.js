// This is actually more a fixture than a test. It is used to make
var common = require('../common');
// sure that require('./path') and require('path') do different things.
// It has to be in the same directory as the test 'test-module-loading.js'
// and it has to have the same name as an internal module.
exports.path_func = function() {
  return 'path_func';
};
