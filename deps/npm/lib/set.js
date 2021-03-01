const npm = require('./npm.js')
const config = require('./config.js')

const usage = 'npm set <key>=<value> [<key>=<value> ...] (See `npm config`)'

const completion = config.completion

const cmd = (args, cb) => {
  if (!args.length)
    return cb(usage)
  npm.commands.config(['set'].concat(args), cb)
}

module.exports = Object.assign(cmd, { usage, completion })
