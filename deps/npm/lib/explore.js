// npm expore <pkg>[@<version>]
// open a subshell to the package folder.

module.exports = explore
explore.usage = "npm explore <pkg> [ -- <cmd>]"
explore.completion = require("./utils/completion/installed-shallow.js")

var npm = require("./npm.js")
  , exec = require("./utils/exec.js")
  , path = require("path")
  , fs = require("graceful-fs")

function explore (args, cb) {
  if (args.length < 1 || !args[0]) return cb(explore.usage)
  var p = args.shift()
  args = args.join(" ").trim()
  if (args) args = ["-c", args]
  else args = []

  var editor = npm.config.get("editor")
    , cwd = path.resolve(npm.dir, p)
  fs.stat(cwd, function (er, s) {
    if (er || !s.isDirectory()) return cb(new Error(
      "It doesn't look like "+p+" is installed."))
    if (!args.length) console.log(
      "\nExploring "+cwd+"\n"+
      "Type 'exit' or ^D when finished\n")
    exec(npm.config.get("shell"), args, null, true, cwd, function (er) {
      // only fail if non-interactive.
      if (!args.length) return cb()
      cb(er)
    })
  })
}
