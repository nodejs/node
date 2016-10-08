
module.exports = get

get.usage = 'npm get <key> <value> (See `npm config`)'

var npm = require('./npm.js')

get.completion = npm.commands.config.completion

function get (args, cb) {
  npm.commands.config(['get'].concat(args), cb)
}
