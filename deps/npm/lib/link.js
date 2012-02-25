
// link with no args: symlink the folder to the global location
// link with package arg: symlink the global to the local

var npm = require("./npm.js")
  , symlink = require("./utils/link.js")
  , fs = require("graceful-fs")
  , log = require("./utils/log.js")
  , asyncMap = require("slide").asyncMap
  , chain = require("slide").chain
  , path = require("path")
  , relativize = require("./utils/relativize.js")
  , rm = require("rimraf")
  , output = require("./utils/output.js")
  , build = require("./build.js")

module.exports = link

link.usage = "npm link (in package dir)"
           + "\nnpm link <pkg> (link global into local)"

link.completion = function (opts, cb) {
  var dir = npm.globalDir
  fs.readdir(dir, function (er, files) {
    cb(er, files.filter(function (f) {
      return f.charAt(0) !== "."
    }))
  })
}

function link (args, cb) {
  if (process.platform === "win32") {
    var e = new Error("npm link not supported on windows")
    e.code = "ENOTSUP"
    e.errno = require("constants").ENOTSUP
    return cb(e)
  }

  if (npm.config.get("global")) {
    return cb(new Error("link should never be --global.\n"
                       +"Please re-run this command with --local"))
  }
  if (args.length === 1 && args[0] === ".") args = []
  if (args.length) return linkInstall(args, cb)
  linkPkg(npm.prefix, cb)
}

function linkInstall (pkgs, cb) {
  asyncMap(pkgs, function (pkg, cb) {
    function n (er, data) {
      if (er) return cb(er, data)
      // install returns [ [folder, pkgId], ... ]
      // but we definitely installed just one thing.
      var d = data.filter(function (d) { return !d[3] })
      pp = d[0][1]
      pkg = path.basename(pp)
      target = path.resolve(npm.dir, pkg)
      next()
    }

    var t = path.resolve(npm.globalDir, "..")
      , pp = path.resolve(npm.globalDir, pkg)
      , rp = null
      , target = path.resolve(npm.dir, pkg)

    // if it's a folder or a random not-installed thing, then
    // link or install it first
    if (pkg.indexOf("/") !== -1 || pkg.indexOf("\\") !== -1) {
      return fs.lstat(path.resolve(pkg), function (er, st) {
        if (er || !st.isDirectory()) {
          npm.commands.install(t, pkg, n)
        } else {
          rp = path.resolve(pkg)
          linkPkg(rp, n)
        }
      })
    }

    fs.lstat(pp, function (er, st) {
      if (er) {
        rp = pp
        return npm.commands.install(t, pkg, n)
      } else if (!st.isSymbolicLink()) {
        rp = pp
        next()
      } else {
        return fs.realpath(pp, function (er, real) {
          if (er) log.warn(pkg, "invalid symbolic link")
          else rp = real
          next()
        })
      }
    })

    function next () {
      chain
        ( [ [npm.commands, "unbuild", [target]]
          , [log.verbose, "symlinking " + pp + " to "+target, "link"]
          , [symlink, pp, target]
          // do run lifecycle scripts - full build here.
          , rp && [build, [target]]
          , [ resultPrinter, pkg, pp, target, rp ] ]
        , cb )
    }
  }, cb)
}

function linkPkg (folder, cb_) {
  var me = folder || npm.prefix
    , readJson = require("./utils/read-json.js")
  readJson( path.resolve(me, "package.json")
          , { dev: true }
          , function (er, d) {
    function cb (er) {
      return cb_(er, [[d && d._id, target, null, null]])
    }
    if (er) return cb(er)
    var target = path.resolve(npm.globalDir, d.name)
    rm(target, function (er) {
      if (er) return cb(er)
      symlink(me, target, function (er) {
        if (er) return cb(er)
        log.verbose(target, "link: build target")
        // also install missing dependencies.
        npm.commands.install(me, [], function (er, installed) {
          if (er) return cb(er)
          // build the global stuff.  Don't run *any* scripts, because
          // install command already will have done that.
          build([target], true, build._noLC, true, function (er) {
            if (er) return cb(er)
            resultPrinter(path.basename(me), me, target, cb)
          })
        })
      })
    })
  })
}

function resultPrinter (pkg, src, dest, rp, cb) {
  if (typeof cb !== "function") cb = rp, rp = null
  var where = relativize(dest, path.resolve(process.cwd(),"x"))
  rp = (rp || "").trim()
  src = (src || "").trim()
  // XXX If --json is set, then look up the data from the package.json
  if (npm.config.get("parseable")) {
    return parseableOutput(dest, rp || src, cb)
  }
  if (rp === src) rp = null
  output.write(where+" -> " + src
              +(rp ? " -> " + rp: ""), cb)
}

function parseableOutput (dest, rp, cb) {
  // XXX this should match ls --parseable and install --parseable
  // look up the data from package.json, format it the same way.
  //
  // link is always effectively "long", since it doesn't help much to
  // *just* print the target folder.
  // However, we don't actually ever read the version number, so
  // the second field is always blank.
  output.write(dest + "::" + rp, cb)
}
