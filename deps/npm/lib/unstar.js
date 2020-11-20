const { usage, completion } = require('./star.js')
const npm = require('./npm.js')

const unstar = (args, cb) => {
  npm.config.set('star.unstar', true)
  return npm.commands.star(args, cb)
}

module.exports = Object.assign(unstar, { usage, completion })
