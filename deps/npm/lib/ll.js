const { usage, completion } = require('./ls.js')
const npm = require('./npm.js')
module.exports = Object.assign((args, cb) => {
  npm.config.set('long', true)
  return npm.commands.ls(args, cb)
}, { usage, completion })
