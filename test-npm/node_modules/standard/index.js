// programmatic usage
var Linter = require('standard-engine').linter

var opts = require('./options.js')

module.exports = new Linter(opts)
