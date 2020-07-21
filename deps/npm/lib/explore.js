// npm explore <pkg>[@<version>]
// open a subshell to the package folder.

module.exports = explore
explore.usage = 'npm explore <pkg> [ -- <command>]'
explore.completion = require('./utils/completion/installed-shallow.js')

var npm = require('./npm.js')
var spawn = require('./utils/spawn')
var path = require('path')
var fs = require('graceful-fs')
var isWindows = require('./utils/is-windows.js')
var escapeExecPath = require('./utils/escape-exec-path.js')
var escapeArg = require('./utils/escape-arg.js')
var output = require('./utils/output.js')
var log = require('npmlog')

function explore (args, cb) {
  if (args.length < 1 || !args[0]) return cb(explore.usage)
  var p = args.shift()

  var cwd = path.resolve(npm.dir, p)
  var opts = {cwd: cwd, stdio: 'inherit'}

  var shellArgs = []
  if (args.length) {
    if (isWindows) {
      var execCmd = escapeExecPath(args.shift())
      var execArgs = [execCmd].concat(args.map(escapeArg))
      opts.windowsVerbatimArguments = true
      shellArgs = ['/d', '/s', '/c'].concat(execArgs)
    } else {
      shellArgs = ['-c', args.map(escapeArg).join(' ').trim()]
    }
  }

  var sh = npm.config.get('shell')
  fs.stat(cwd, function (er, s) {
    if (er || !s.isDirectory()) {
      return cb(new Error(
        "It doesn't look like " + p + ' is installed.'
      ))
    }

    if (!shellArgs.length) {
      output(
        '\nExploring ' + cwd + '\n' +
          "Type 'exit' or ^D when finished\n"
      )
    }

    log.silly('explore', {sh, shellArgs, opts})
    var shell = spawn(sh, shellArgs, opts)
    shell.on('close', function (er) {
      // only fail if non-interactive.
      if (!shellArgs.length) return cb()
      cb(er)
    })
  })
}
