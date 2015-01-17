// npm version <newver>

module.exports = version

var semver = require("semver")
  , path = require("path")
  , fs = require("graceful-fs")
  , writeFileAtomic = require("write-file-atomic")
  , chain = require("slide").chain
  , log = require("npmlog")
  , npm = require("./npm.js")
  , git = require("./utils/git.js")
  , assert = require("assert")

version.usage = "npm version [<newversion> | major | minor | patch | prerelease | preminor | premajor ]\n"
              + "\n(run in package dir)\n"
              + "'npm -v' or 'npm --version' to print npm version "
              + "("+npm.version+")\n"
              + "'npm view <pkg> version' to view a package's "
              + "published version\n"
              + "'npm ls' to inspect current package/dependency versions"

function version (args, silent, cb_) {
  if (typeof cb_ !== "function") cb_ = silent, silent = false
  if (args.length > 1) return cb_(version.usage)

  var packagePath = path.join(npm.localPrefix, "package.json")
  fs.readFile(packagePath, function (er, data) {
    function cb (er) {
      if (!er && !silent) console.log("v" + data.version)
      cb_(er)
    }

    if (data) data = data.toString()
    try {
      data = JSON.parse(data)
    }
    catch (er) {
      data = null
    }

    if (!args.length) return dump(data, cb_)

    if (er) {
      log.error("version", "No package.json found")
      return cb_(er)
    }

    var newVersion = semver.valid(args[0])
    if (!newVersion) newVersion = semver.inc(data.version, args[0])
    if (!newVersion) return cb_(version.usage)
    if (data.version === newVersion) return cb_(new Error("Version not changed"))
    data.version = newVersion

    checkGit(function (er, hasGit) {
      if (er) return cb_(er)

      write(data, "package.json", function (er) {
        if (er) return cb_(er)

        updateShrinkwrap(newVersion, function (er, hasShrinkwrap) {
          if (er || !hasGit) return cb(er)

          commit(data.version, hasShrinkwrap, cb)
        })
      })
    })
  })
}

function updateShrinkwrap (newVersion, cb) {
  fs.readFile(path.join(npm.localPrefix, "npm-shrinkwrap.json"), function (er, data) {
    if (er && er.code === "ENOENT") return cb(null, false)

    try {
      data = data.toString()
      data = JSON.parse(data)
    }
    catch (er) {
      log.error("version", "Bad npm-shrinkwrap.json data")
      return cb(er)
    }

    data.version = newVersion
    write(data, "npm-shrinkwrap.json", function (er) {
      if (er) {
        log.error("version", "Bad npm-shrinkwrap.json data")
        return cb(er)
      }
      cb(null, true)
    })
  })
}

function dump (data, cb) {
  var v = {}

  if (data && data.name && data.version) v[data.name] = data.version
  v.npm = npm.version
  Object.keys(process.versions).sort().forEach(function (k) {
    v[k] = process.versions[k]
  })

  if (npm.config.get("json")) v = JSON.stringify(v, null, 2)

  console.log(v)
  cb()
}

function checkGit (cb) {
  fs.stat(path.join(npm.localPrefix, ".git"), function (er, s) {
    var doGit = !er && s.isDirectory() && npm.config.get("git-tag-version")
    if (!doGit) {
      if (er) log.verbose("version", "error checking for .git", er)
      log.verbose("version", "not tagging in git")
      return cb(null, false)
    }

    // check for git
    git.whichAndExec(
      [ "status", "--porcelain" ],
      { env : process.env },
      function (er, stdout) {
        if (er && er.code === "ENOGIT") {
          log.warn(
            "version",
            "This is a Git checkout, but the git command was not found.",
            "npm could not create a Git tag for this release!"
          )
          return cb(null, false)
        }

        var lines = stdout.trim().split("\n").filter(function (line) {
          return line.trim() && !line.match(/^\?\? /)
        }).map(function (line) {
          return line.trim()
        })
        if (lines.length) return cb(new Error(
          "Git working directory not clean.\n"+lines.join("\n")
        ))

        cb(null, true)
      }
    )
  })
}

function commit (version, hasShrinkwrap, cb) {
  var options = { env : process.env }
  var message = npm.config.get("message").replace(/%s/g, version)
  var sign = npm.config.get("sign-git-tag")
  var flag = sign ? "-sm" : "-am"
  chain(
    [
      git.chainableExec([ "add", "package.json" ], options),
      hasShrinkwrap && git.chainableExec([ "add", "npm-shrinkwrap.json" ] , options),
      git.chainableExec([ "commit", "-m", message ], options),
      git.chainableExec([ "tag", "v" + version, flag, message ], options)
    ],
    cb
  )
}

function write (data, file, cb) {
  assert(data && typeof data === "object", "must pass data to version write")
  assert(typeof file === "string", "must pass filename to write to version write")

  log.verbose("version.write", "data", data, "to", file)
  writeFileAtomic(
    path.join(npm.localPrefix, file),
    new Buffer(JSON.stringify(data, null, 2) + "\n"),
    cb
  )
}
