var which = require('which')

exports.npmPath = process.env.npm || which.sync('npm')

var spawn = require('child_process').spawn
exports.npm = function (args, opts) {
  var cmd = exports.npmPath
  if (exports.npmPath.match(/\.js$/)) {
    args = [exports.npmPath].concat(args)
    cmd = process.execPath
  }
  if (!opts) opts = {}
  if (!opts.env) opts.env = process.env
  opts.env.LC_CTYPE = 'UTF-8'
  return spawn(cmd, args, opts)
}

if (module === require.main) {
  console.log('1..1')
  console.log('ok - nothing to see here')
}
