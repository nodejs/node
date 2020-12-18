
module.exports = set

set.usage = 'npm set <key>=<value> [<key>=<value> ...] (See `npm config`)'

var npm = require('./npm.js')

set.completion = npm.commands.config.completion

function set (args, cb) {
  if (!args.length)
    return cb(set.usage)
  npm.commands.config(['set'].concat(args), cb)
}
