// XXX lib/cache.js and this file need to be rewritten.

// commands for packing and unpacking tarballs
// this file is used by lib/cache.js

var npm = require("../npm.js")
  , fs = require("graceful-fs")
  , exec = require("./exec.js")
  , find = require("./find.js")
  , mkdir = require("./mkdir-p.js")
  , asyncMap = require("slide").asyncMap
  , path = require("path")
  , log = require("./log.js")
  , uidNumber = require("./uid-number.js")
  , rm = require("rimraf")
  , readJson = require("./read-json.js")
  , relativize = require("./relativize.js")
  , cache = require("../cache.js")
  , excludes = require("./excludes.js")
  , myUid = process.getuid && process.getuid()
  , myGid = process.getgid && process.getgid()
  , tar = require("tar")
  , zlib = require("zlib")
  , fstream = require("fstream")

exports.pack = pack
exports.unpack = unpack
exports.makeList = makeList

function pack (targetTarball, folder, pkg, dfc, cb) {
  if (typeof cb !== "function") cb = dfc, dfc = true
  folder = path.resolve(folder)

  log.verbose(folder, "pack")

  if (typeof pkg === "function") {
    cb = pkg, pkg = null
    return readJson(path.resolve(folder, "package.json"), function (er, pkg) {
      if (er) return log.er(cb, "Couldn't find package.json in "+folder)(er)
      pack(targetTarball, folder, pkg, dfc, cb)
    })
  }
  log.verbose(folder+" "+targetTarball, "pack")
  var parent = path.dirname(folder)
    , addFolder = path.basename(folder)

  var confEx = npm.config.get("ignore")
  log.silly(folder, "makeList")
  makeList(folder, pkg, dfc, function (er, files, cleanup) {
    if (er) return cb(er)
    // log.silly(files, "files")
    return packFiles(targetTarball, parent, files, pkg, function (er) {
      if (!cleanup || !cleanup.length) return cb(er)
      // try to be a good citizen, even/especially in the event of failure.
      cleanupResolveLinkDep(cleanup, function (er2) {
        if (er || er2) {
          if (er) log(er, "packing tarball")
          if (er2) log(er2, "while cleaning up resolved deps")
        }
        return cb(er || er2)
      })
    })
  })
}

function packFiles (targetTarball, parent, files, pkg, cb_) {

  var p

  files = files.map(function (f) {
    p = f.split(/\/|\\/)[0]
    return path.resolve(parent, f)
  })

  parent = path.resolve(parent, p)

  var called = false
  function cb (er) {
    if (called) return
    called = true
    cb_(er)
  }

  log.verbose(targetTarball, "tarball")
  log.verbose(parent, "parent")
  fstream.Reader({ type: "Directory"
                 , path: parent
                 , filter: function () {
                     // files should *always* get into tarballs
                     // in a user-writable state, even if they're
                     // being installed from some wackey vm-mounted
                     // read-only filesystem.
                     this.props.mode = this.props.mode | 0200
                     var inc = -1 !== files.indexOf(this.path)

                     // WARNING! Hackety hack!
                     // XXX Fix this in a better way.
                     // Rename .gitignore to .npmignore if there is not a
                     // .npmignore file there already, the better to lock
                     // down installed packages with git for deployment.
                     if (this.basename === ".gitignore") {
                       if (this.parent._entries.indexOf(".npmignore") !== -1) {
                         return false
                       }
                       var d = path.dirname(this.path)
                       this.basename = ".npmignore"
                       this.path = path.join(d, ".npmignore")
                     }
                     return inc
                   }
                 })
    .on("error", log.er(cb, "error reading "+parent))
    // By default, npm includes some proprietary attributes in the
    // package tarball.  This is sane, and allowed by the spec.
    // However, npm *itself* excludes these from its own package,
    // so that it can be more easily bootstrapped using old and
    // non-compliant tar implementations.
    .pipe(tar.Pack({ noProprietary: !npm.config.get("proprietary-attribs") }))
    .on("error", log.er(cb, "tar creation error "+targetTarball))
    .pipe(zlib.Gzip())
    .on("error", log.er(cb, "gzip error "+targetTarball))
    .pipe(fstream.Writer({ type: "File", path: targetTarball }))
    .on("error", log.er(cb, "Could not write "+targetTarball))
    .on("close", cb)
}


function unpack (tarball, unpackTarget, dMode, fMode, uid, gid, cb) {
  if (typeof cb !== "function") cb = gid, gid = null
  if (typeof cb !== "function") cb = uid, uid = null
  if (typeof cb !== "function") cb = fMode, fMode = npm.modes.file
  if (typeof cb !== "function") cb = dMode, dMode = npm.modes.exec

  uidNumber(uid, gid, function (er, uid, gid) {
    if (er) return cb(er)
    unpack_(tarball, unpackTarget, dMode, fMode, uid, gid, cb)
  })
}

function unpack_ ( tarball, unpackTarget, dMode, fMode, uid, gid, cb ) {
  // If the desired target is /path/to/foo,
  // then unpack into /path/to/.foo.npm/{something}
  // rename that to /path/to/foo, and delete /path/to/.foo.npm
  var parent = path.dirname(unpackTarget)
    , base = path.basename(unpackTarget)
    , tmp = path.resolve(parent, "___" + base + ".npm")

  mkdir(tmp, dMode || npm.modes.exec, uid, gid, function (er) {
    log.verbose([uid, gid], "unpack_ uid, gid")
    log.verbose(unpackTarget, "unpackTarget")
    if (er) return log.er(cb, "Could not create "+tmp)(er)
    // cp the gzip of the tarball, pipe the stdout into tar's stdin
    // gzip {tarball} --decompress --stdout \
    //   | tar -mvxpf - --strip-components=1 -C {unpackTarget}
    gunzTarPerm( tarball, tmp
               , dMode, fMode
               , uid, gid
               , function (er, folder) {
      if (er) return cb(er)
      log.verbose(folder, "gunzed")

      rm(unpackTarget, function (er) {
        if (er) return cb(er)
        log.verbose(unpackTarget, "rm'ed")

        moveIntoPlace(folder, unpackTarget, function (er) {
          if (er) return cb(er)
          log.verbose([folder, unpackTarget], "renamed")
          // curse you, nfs!  It will lie and tell you that the
          // mv is done, when in fact, it isn't.  In theory,
          // reading the file should cause it to wait until it's done.
          readJson( path.resolve(unpackTarget, "package.json")
                  , function (er, data) {
            // now we read the json, so we know it's there.
            rm(tmp, function (er2) { cb(er || er2, data) })
          })
        })
      })
    })
  })
}

// on Windows, A/V software can lock the directory, causing this
// to fail with an EACCES.  Try again on failure, for up to 1 second.
// XXX Fix this by not unpacking into a temp directory, instead just
// renaming things on the way out of the tarball.
function moveIntoPlace (folder, unpackTarget, cb) {
  var start = Date.now()
  fs.rename(folder, unpackTarget, function CB (er) {
    if (er
        && process.platform === "win32"
        && er.code === "EACCES"
        && Date.now() - start < 1000) {
      return fs.rename(folder, unpackTarget, CB)
    }
    cb(er)
  })
}


function gunzTarPerm (tarball, tmp, dMode, fMode, uid, gid, cb) {
  if (!dMode) dMode = npm.modes.exec
  if (!fMode) fMode = npm.modes.file
  log.silly([dMode.toString(8), fMode.toString(8)], "gunzTarPerm modes")

  fs.createReadStream(tarball)
    .on("error", log.er(cb, "error reading "+tarball))
    .pipe(zlib.Unzip())
    .on("error", log.er(cb, "unzip error "+tarball))
    .pipe(tar.Extract({ type: "Directory", path: tmp }))
    .on("error", log.er(cb, "Failed unpacking "+tarball))
    .on("close", afterUntar)

  //
  // XXX Do all this in an Extract filter.
  //
  function afterUntar (er) {
    log.silly(er, "afterUntar")
    // if we're not doing ownership management,
    // then we're done now.
    if (er) return log.er(cb, "Failed unpacking "+tarball)(er)

    // HACK skip on windows
    if (npm.config.get("unsafe-perm") && process.platform !== "win32") {
      uid = process.getuid()
      gid = process.getgid()
      if (uid === 0) {
        if (process.env.SUDO_UID) uid = +process.env.SUDO_UID
        if (process.env.SUDO_GID) gid = +process.env.SUDO_GID
      }
    }

    if (process.platform === "win32") {
      return fs.readdir(tmp, function (er, files) {
        files = files.filter(function (f) {
          return f && f.indexOf("\0") === -1
        })
        cb(er, files && path.resolve(tmp, files[0]))
      })
    }

    find(tmp, function (f) {
      return f !== tmp
    }, function (er, files) {
      if (er) return cb(er)
      asyncMap(files, function (f, cb) {
        f = path.resolve(f)
        log.silly(f, "asyncMap in gTP")
        fs.lstat(f, function (er, stat) {

          if (er || stat.isSymbolicLink()) return cb(er)
          if (typeof uid === "number" && typeof gid === "number") {
            fs.chown(f, uid, gid, chown)
          } else chown()

          function chown (er) {
            if (er) return cb(er)
            var mode = stat.isDirectory() ? dMode : fMode
              , oldMode = stat.mode & 0777
              , newMode = (oldMode | mode) & (~npm.modes.umask)
            if (mode && newMode !== oldMode) {
              log.silly(newMode.toString(8), "chmod "+path.basename(f))
              fs.chmod(f, newMode, cb)
            } else cb()
          }
        })
      }, function (er) {

        if (er) return cb(er)
        if (typeof myUid === "number" && typeof myGid === "number") {
          fs.chown(tmp, myUid, myGid, chown)
        } else chown()

        function chown (er) {
          if (er) return cb(er)
          fs.readdir(tmp, function (er, folder) {
            folder = folder && folder.filter(function (f) {
              return f && !f.match(/^\._/)
            })
            cb(er, folder && path.resolve(tmp, folder[0]))
          })
        }
      })
    })
  }
}

function makeList (dir, pkg, dfc, cb) {
  if (typeof cb !== "function") cb = dfc, dfc = true
  if (typeof cb !== "function") cb = pkg, pkg = null
  dir = path.resolve(dir)

  if (!pkg.path) pkg.path = dir

  var name = path.basename(dir)

  // since this is a top-level traversal, get the user and global
  // exclude files, as well as the "ignore" config setting.
  var confIgnore = npm.config.get("ignore").trim()
        .split(/[\n\r\s\t]+/)
        .filter(function (i) { return i.trim() })
    , userIgnore = npm.config.get("userignorefile")
    , globalIgnore = npm.config.get("globalignorefile")
    , userExclude
    , globalExclude

  confIgnore.dir = dir
  confIgnore.name = "confIgnore"

  var defIgnore = ["build/"]
  defIgnore.dir = dir

  // TODO: only look these up once, and cache outside this function
  excludes.parseIgnoreFile( userIgnore, null, dir
                          , function (er, uex) {
    if (er) return cb(er)
    userExclude = uex
    next()
  })

  excludes.parseIgnoreFile( globalIgnore, null, dir
                          , function (er, gex) {
    if (er) return cb(er)
    globalExclude = gex
    next()
  })

  function next () {
    if (!globalExclude || !userExclude) return
    var exList = [ defIgnore, confIgnore, globalExclude, userExclude ]

    makeList_(dir, pkg, exList, dfc, function (er, files, cleanup) {
      if (er) return cb(er)
      var dirLen = dir.replace(/(\/|\\)$/, "").length + 1
      log.silly([dir, dirLen], "dir, dirLen")
      files = files.map(function (file) {
        return path.join(name, file.substr(dirLen))
      })
      return cb(null, files, cleanup)
    })
  }
}

// Patterns ending in slashes will only match targets
// ending in slashes.  To implement this, add a / to
// the filename iff it lstats isDirectory()
function readDir (dir, pkg, dfc, cb) {
  fs.readdir(dir, function (er, files) {
    if (er) return cb(er)
    files = files.filter(function (f) {
      return f && f.charAt(0) !== "/" && f.indexOf("\0") === -1
    })
    asyncMap(files, function (file, cb) {
      fs.lstat(path.resolve(dir, file), function (er, st) {
        if (er) return cb(null, [])
        // if it's a directory, then tack "/" onto the name
        // so that it can match dir-only patterns in the
        // include/exclude logic later.
        if (st.isDirectory()) return cb(null, file + "/")

        // if it's a symlink, then we need to do some more
        // complex stuff for GH-691
        if (st.isSymbolicLink()) return readSymlink(dir, file, pkg, dfc, cb)

        // otherwise, just let it on through.
        return cb(null, file)
      })
    }, cb)
  })
}

// just see where this link is pointing, and resolve relative paths.
function shallowReal (link, cb) {
  link = path.resolve(link)
  fs.readlink(link, function (er, t) {
    if (er) return cb(er)
    return cb(null, path.resolve(path.dirname(link), t), t)
  })
}

function readSymlink (dir, file, pkg, dfc, cb) {
  var isNM = dfc
           && path.basename(dir) === "node_modules"
           && path.dirname(dir) === pkg.path
  // see if this thing is pointing outside of the package.
  // external symlinks are resolved for deps, ignored for other things.
  // internal symlinks are allowed through.
  var df = path.resolve(dir, file)
  shallowReal(df, function (er, r, target) {
    if (er) return cb(null, []) // wtf? exclude file.
    if (r.indexOf(dir) === 0) return cb(null, file) // internal
    if (!isNM) return cb(null, []) // external non-dep
    // now the fun stuff!
    fs.realpath(df, function (er, resolved) {
      if (er) return cb(null, []) // can't add it.
      readJson(path.resolve(resolved, "package.json"), function (er) {
        if (er) return cb(null, []) // not a package
        resolveLinkDep(dir, file, resolved, target, pkg, function (er, f, c) {
          cb(er, f, c)
        })
      })
    })
  })
}

// put the link back the way it was.
function cleanupResolveLinkDep (cleanup, cb) {
  // cut it out of the list, so that cycles will be broken.
  if (!cleanup) return cb()

  asyncMap(cleanup, function (d, cb) {
    rm(d[1], function (er) {
      if (er) return cb(er)
      fs.symlink(d[0], d[1], cb)
    })
  }, cb)
}

function resolveLinkDep (dir, file, resolved, target, pkg, cb) {
  // we've already decided that this is a dep that will be bundled.
  // make sure the data reflects this.
  var bd = pkg.bundleDependencies || pkg.bundledDependencies || []
  delete pkg.bundledDependencies
  pkg.bundleDependencies = bd
  var f = path.resolve(dir, file)
    , cleanup = [[target, f, resolved]]

  if (bd.indexOf(file) === -1) {
    // then we don't do this one.
    // just move the symlink out of the way.
    return rm(f, function (er) {
      cb(er, file, cleanup)
    })
  }

  rm(f, function (er) {
    if (er) return cb(er)
    cache.add(resolved, function (er, data) {
      if (er) return cb(er)
      cache.unpack(data.name, data.version, f, function (er, data) {
        if (er) return cb(er)
        // now clear out the cache entry, since it's weird, probably.
        // pass the cleanup object along so that the thing getting the
        // list of files knows what to clean up afterwards.
        cache.clean([data._id], function (er) { cb(er, file, cleanup) })
      })
    })
  })
}

// exList is a list of ignore lists.
// Each exList item is an array of patterns of files to ignore
//
function makeList_ (dir, pkg, exList, dfc, cb) {
  var files = null
    , cleanup = null

  readDir(dir, pkg, dfc, function (er, f, c) {
    if (er) return cb(er)
    cleanup = c
    files = f.map(function (f) {
      // no nulls in paths!
      return f.split(/\0/)[0]
    }).filter(function (f) {
      // always remove all source control folders and
      // waf/vim/OSX garbage.  this is a firm requirement.
      return !( f === ".git/"
             || f === ".lock-wscript"
             || f === "CVS/"
             || f === ".svn/"
             || f === ".hg/"
             || f.match(/^\..*\.swp/)
             || f === ".DS_Store"
             || f.match(/^\._/)
             || f === "npm-debug.log"
             || f === ""
             || f.charAt(0) === "/"
              )
    })

    // if (files.length > 0) files.push(".")

    if (files.indexOf("package.json") !== -1 && dir !== pkg.path) {
      // a package.json file starts the whole exclude/include
      // logic all over.  Otherwise, a parent could break its
      // deps with its files list or .npmignore file.
      readJson(path.resolve(dir, "package.json"), function (er, data) {
        if (!er && typeof data === "object") {
          data.path = dir
          return makeList(dir, data, dfc, function (er, files) {
            // these need to be mounted onto the directory now.
            cb(er, files && files.map(function (f) {
              return path.resolve(path.dirname(dir), f)
            }))
          })
        }
        next()
      })
      //next()
    } else next()

    // add a local ignore file, if found.
    if (files.indexOf(".npmignore") === -1
        && files.indexOf(".gitignore") === -1) next()
    else {
      excludes.addIgnoreFile( path.resolve(dir, ".npmignore")
                            , ".gitignore"
                            , exList
                            , dir
                            , function (er, list) {
        if (!er) exList = list
        next(er)
      })
    }
  })

  var n = 2
    , errState = null
  function next (er) {
    if (errState) return
    if (er) return cb(errState = er, [], cleanup)
    if (-- n > 0) return

    if (!pkg) return cb(new Error("No package.json file in "+dir))
    if (pkg.path === dir && pkg.files) {
      pkg.files = pkg.files.filter(function (f) {
        f = f.trim()
        return f && f.charAt(0) !== "#"
      })
      if (!pkg.files.length) pkg.files = null
    }
    if (pkg.path === dir && pkg.files) {
      // stuff on the files list MUST be there.
      // ignore everything, then include the stuff on the files list.
      var pkgFiles = ["*"].concat(pkg.files.map(function (f) {
        return "!" + f
      }))
      pkgFiles.dir = dir
      pkgFiles.packageFiles = true
      exList.push(pkgFiles)
    }

    if (path.basename(dir) === "node_modules"
        && pkg.path === path.dirname(dir)
        && dfc) { // do fancy crap
      files = filterNodeModules(files, pkg)
    } else {
      // If a directory is excluded, we still need to be
      // able to *include* a file within it, and have that override
      // the prior exclusion.
      //
      // This whole makeList thing probably needs to be rewritten
      files = files.filter(function (f) {
        return excludes.filter(dir, exList)(f) || f.slice(-1) === "/"
      })
    }


    asyncMap(files, function (file, cb) {
      // if this is a dir, then dive into it.
      // otherwise, don't.
      file = path.resolve(dir, file)

      // in 0.6.0, fs.readdir can produce some really odd results.
      // XXX: remove this and make the engines hash exclude 0.6.0
      if (file.indexOf(dir) !== 0) {
        return cb(null, [])
      }

      fs.lstat(file, function (er, st) {
        if (er) return cb(er)
        if (st.isDirectory()) {
          return makeList_(file, pkg, exList, dfc, cb)
        }
        return cb(null, file)
      })
    }, function (er, files, c) {
      if (c) cleanup = (cleanup || []).concat(c)
      if (files.length > 0) files.push(dir)
      return cb(er, files, cleanup)
    })
  }
}

// only include node_modules folder that are:
// 1. not on the dependencies list or
// 2. on the "bundleDependencies" list.
function filterNodeModules (files, pkg) {
  var bd = pkg.bundleDependencies || pkg.bundledDependencies || []
    , deps = Object.keys(pkg.dependencies || {})
             .filter(function (key) { return !pkg.dependencies[key].extraneous })
             .concat(Object.keys(pkg.devDependencies || {}))

  delete pkg.bundledDependencies
  pkg.bundleDependencies = bd

  return files.filter(function (f) {
    f = f.replace(/\/$/, "")
    return f.charAt(0) !== "."
           && f.charAt(0) !== "_"
           && bd.indexOf(f) !== -1
  })
}
