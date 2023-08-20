const { inspect } = require('util')
const log = require('./logging.js')

module.exports = tree => log.info(inspect(tree.toJSON(), { depth: Infinity }))
