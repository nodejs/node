var parser = require('./parser.generated.js').parser

module.exports = function(argument) {
  return parser.parse(argument) }
