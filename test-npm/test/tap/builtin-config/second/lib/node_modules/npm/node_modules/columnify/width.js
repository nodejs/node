var stripAnsi = require('strip-ansi')
var wcwidth = require('wcwidth')

module.exports = function(str) {
  return wcwidth(stripAnsi(str))
}
