var stripAnsi = require('strip-ansi')
var wcwidth = require('wcwidth.js')({ monkeypatch: false, control: 0 })

module.exports = function(str) {
  return wcwidth(stripAnsi(str))
}
