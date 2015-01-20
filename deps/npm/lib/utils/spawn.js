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
    cooked.emit("close", code, signal)
  })

  cooked.stdin = raw.stdin
  cooked.stdout = raw.stdout
  cooked.stderr = raw.stderr
  cooked.kill = function (sig) { return raw.kill(sig) }

  return cooked
}
