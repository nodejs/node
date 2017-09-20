var parser = require('./parser').parser

module.exports = function (argument) {
  return parser.parse(argument)
}
