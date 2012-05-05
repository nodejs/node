// npm version <newver>

module.exports = version

var exec = require("./utils/exec.js")
  , readJson = require("./utils/read-json.js")
  , semver = require("semver")
  , path = require("path")
  , fs = require("graceful-fs")
  , chain = require("slide").chain
  , log = require("./utils/log.js")
  , npm = require("./npm.js")

version.usage = "npm version <newversion> [--message commit-message]"
              + "\n(run in package dir)\n"
              + "'npm -v' or 'npm --version' to print npm version "
              + "("+npm.version+")\n"
              + "'npm view <pkg> version' to view a package's "
              + "published version\n"
              + "'npm ls' to inspect current package/dependency versions"

function version (args, cb) {
  if (args.length !== 1) return cb(version.usage)
  readJson(path.join(process.cwd(), "package.json"), function (er, data) {
    if (er) return log.er(cb, "No package.json found")(er)
		var newVer = semver.valid(args[0])
		if (!newVer) newVer = semver.inc(data.version, args[0])
		if (!newVer) return cb(version.usage)
    if (data.version === newVer) return cb(new Error("Version not changed"))
    data.version = newVer
    Object.keys(data).forEach(function (k) {
      if (k.charAt(0) === "_") delete data[k]
    })
    readJson.unParsePeople(data)
    fs.stat(path.join(process.cwd(), ".git"), function (er, s) {
      var doGit = !er && s.isDirectory()
      if (!doGit) return write(data, cb)
      else checkGit(data, cb)
    })
  })
}
function checkGit (data, cb) {
  exec( npm.config.get("git"), ["status", "--porcelain"], process.env, false
      , function (er, code, stdout, stderr) {
    var lines = stdout.trim().split("\n").filter(function (line) {
      return line.trim() && !line.match(/^\?\? /)
    })
    if (lines.length) return cb(new Error(
      "Git working directory not clean.\n"+lines.join("\n")))
    write(data, function (er) {
      if (er) return cb(er)
      var message = npm.config.get("message").replace(/%s/g, data.version)
      chain
        ( [ [ exec, npm.config.get("git")
            , ["add","package.json"], process.env, false ]
          , [ exec, npm.config.get("git")
            , ["commit", "-m", message ], process.env, false ]
          , [ exec, npm.config.get("git")
            , ["tag", "v"+data.version], process.env, false ] ]
        , cb )
    })
  })
}
function write (data, cb) {
  fs.writeFile( path.join(process.cwd(), "package.json")
              , new Buffer(JSON.stringify(data, null, 2))
              , cb )
}
