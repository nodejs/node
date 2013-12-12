// npm version <newver>

module.exports = version

var exec = require("child_process").execFile
  , semver = require("semver")
  , path = require("path")
  , fs = require("graceful-fs")
  , chain = require("slide").chain
  , log = require("npmlog")
  , which = require("which")
  , npm = require("./npm.js")

version.usage = "npm version [<newversion> | major | minor | patch]\n"
              + "\n(run in package dir)\n"
              + "'npm -v' or 'npm --version' to print npm version "
              + "("+npm.version+")\n"
              + "'npm view <pkg> version' to view a package's "
              + "published version\n"
              + "'npm ls' to inspect current package/dependency versions"

function version (args, silent, cb_) {
  if (typeof cb_ !== "function") cb_ = silent, silent = false
  if (args.length > 1) return cb_(version.usage)
  fs.readFile(path.join(process.cwd(), "package.json"), function (er, data) {
    if (!args.length) {
      var v = {}
      Object.keys(process.versions).forEach(function (k) {
        v[k] = process.versions[k]
      })
      v.npm = npm.version
      try {
        data = JSON.parse(data.toString())
      } catch (er) {
        data = null
      }
      if (data && data.name && data.version) {
        v[data.name] = data.version
      }
      console.log(v)
      return cb_()
    }

    if (er) {
      log.error("version", "No package.json found")
      return cb_(er)
    }

    try {
      data = JSON.parse(data)
    } catch (er) {
      log.error("version", "Bad package.json data")
      return cb_(er)
    }

		var newVer = semver.valid(args[0])
		if (!newVer) newVer = semver.inc(data.version, args[0])
		if (!newVer) return cb_(version.usage)
    if (data.version === newVer) return cb_(new Error("Version not changed"))
    data.version = newVer

    fs.stat(path.join(process.cwd(), ".git"), function (er, s) {
      function cb (er) {
        if (!er && !silent) console.log("v" + newVer)
        cb_(er)
      }

      var tags = npm.config.get('git-tag-version')
      var doGit = !er && s.isDirectory() && tags
      if (!doGit) return write(data, cb)
      else checkGit(data, cb)
    })
  })
}

function checkGit (data, cb) {
  var git = npm.config.get("git")
  var args = [ "status", "--porcelain" ]
  var env = process.env

  // check for git
  which(git, function (err) {
    if (err) {
      err.code = "ENOGIT"
      return cb(err)
    }

    gitFound()
  })

  function gitFound () {
    exec(git, args, {env: env}, function (er, stdout, stderr) {
      var lines = stdout.trim().split("\n").filter(function (line) {
        return line.trim() && !line.match(/^\?\? /)
      }).map(function (line) {
        return line.trim()
      })
      if (lines.length) return cb(new Error(
        "Git working directory not clean.\n"+lines.join("\n")))
      write(data, function (er) {
        if (er) return cb(er)
        var message = npm.config.get("message").replace(/%s/g, data.version)
          , sign = npm.config.get("sign-git-tag")
          , flag = sign ? "-sm" : "-am"
        chain
          ( [ [ exec, git, [ "add", "package.json" ], {env: process.env} ]
            , [ exec, git, [ "commit", "-m", message ], {env: process.env} ]
            , [ exec, git, [ "tag", "v" + data.version, flag, message ]
              , {env: process.env} ] ]
          , cb )
      })
    })
  }
}

function write (data, cb) {
  fs.writeFile( path.join(process.cwd(), "package.json")
              , new Buffer(JSON.stringify(data, null, 2) + "\n")
              , cb )
}
