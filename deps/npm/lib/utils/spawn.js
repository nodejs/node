module.exports = spawn

var _spawn = require("child_process").spawn
var EventEmitter = require("events").EventEmitter

function spawn (cmd, args, options) {
  var raw = _spawn(cmd, args, options)
  var cooked = new EventEmitter()

  raw.on("error", function (er) {
    er.file = cmd
    cooked.emit("error", er)
  }).on("close", function (code, signal) {
    // Create ENOENT error because Node.js v0.8 will not emit
    // an `error` event if the command could not be found.
    if (code === 127) {
      var er = new Error('spawn ENOENT')
      er.code = 'ENOENT'
      er.errno = 'ENOENT'
      er.syscall = 'spawn'
      er.file = cmd
      cooked.emit('error', er)
    } else {
      cooked.emit("close", code, signal)
    }
  })

  cooked.stdin = raw.stdin
  cooked.stdout = raw.stdout
  cooked.stderr = raw.stderr
  cooked.kill = function (sig) { return raw.kill(sig) }

  return cooked
}
