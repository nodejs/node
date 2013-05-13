// npm explore <pkg>[@<version>]
// open a subshell to the package folder.

module.exports = explore
explore.usage = "npm explore <pkg> [ -- <cmd>]"
explore.completion = require("./utils/completion/installed-shallow.js")

var npm = require("./npm.js")
  , spawn = require("child_process").spawn
  , path = require("path")
  , fs = require("graceful-fs")

function explore (args, cb) {
  if (args.length < 1 || !args[0]) return cb(explore.usage)
  var p = args.shift()
  args = args.join(" ").trim()
  if (args) args = ["-c", args]
  else args = []

  var cwd = path.resolve(npm.dir, p)
  var sh = npm.config.get("shell")
  fs.stat(cwd, function (er, s) {
    if (er || !s.isDirectory()) return cb(new Error(
      "It doesn't look like "+p+" is installed."))
    if (!args.length) console.log(
      "\nExploring "+cwd+"\n"+
      "Type 'exit' or ^D when finished\n")

    var shell = spawn(sh, args, { cwd: cwd, customFds: [0, 1, 2] })
    shell.on("close", function (er) {
      // only fail if non-interactive.
      if (!args.length) return cb()
      cb(er)
    })
  })
}
