const npm = require('./npm.js')
const config = require('./config.js')
const usageUtil = require('./utils/usage.js')

const usage = usageUtil(
  'get',
  'npm get [<key> ...] (See `npm config`)'
)

const completion = config.completion

const cmd = (args, cb) =>
  npm.commands.config(['get'].concat(args), cb)

module.exports = Object.assign(cmd, { usage, completion })
