var path = require('path');
var common = require('./common');

//@
//@ ### pwd()
//@ Returns the current directory.
function _pwd() {
  var pwd = path.resolve(process.cwd());
  return common.ShellString(pwd);
}
module.exports = _pwd;
