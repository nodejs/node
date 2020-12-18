const npm = require('./npm.js')
const usageUtil = require('./utils/usage.js')

const usage = usageUtil(
  'get',
  'npm get [<key> ...] (See `npm config`)'
)

const completion = npm.commands.config.completion

const cmd = (args, cb) =>
  npm.commands.config(['get'].concat(args), cb)

module.exports = Object.assign(cmd, { usage, completion })
