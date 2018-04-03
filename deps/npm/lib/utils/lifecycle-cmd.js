exports = module.exports = cmd

var npm = require('../npm.js')
var usage = require('./usage.js')

function cmd (stage) {
  function CMD (args, cb) {
    npm.commands['run-script']([stage].concat(args), cb)
  }
  CMD.usage = usage(stage, 'npm ' + stage + ' [-- <args>]')
  var installedShallow = require('./completion/installed-shallow.js')
  CMD.completion = function (opts, cb) {
    installedShallow(opts, function (d) {
      return d.scripts && d.scripts[stage]
    }, cb)
  }
  return CMD
}
