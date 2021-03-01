const { usage, completion } = require('./ls.js')
const npm = require('./npm.js')

const cmd = (args, cb) => {
  npm.config.set('long', true)
  return npm.commands.ls(args, cb)
}

module.exports = Object.assign(cmd, { usage, completion })
