const { inspect } = require('util')
const { quiet } = require('./options.js')

module.exports = quiet ? () => {}
  : tree => console.log(inspect(tree.toJSON(), { depth: Infinity }))
