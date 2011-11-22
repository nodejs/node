
var log = require("./log.js")
  , fs = require("graceful-fs")
  , path = require("path")
  , npm = require("../npm.js")
  , exec = require("./exec.js")
  , uidNumber = require("./uid-number.js")
  , umask = process.umask()
  , umaskOrig = umask
  , addedUmaskExit = false
  , mkdirCache = {}

module.exports = mkdir
function mkdir (ensure, mode, uid, gid, noChmod, cb_) {
  if (typeof cb_ !== "function") cb_ = noChmod, noChmod = null
  if (typeof cb_ !== "function") cb_ = gid, gid = null
  if (typeof cb_ !== "function") cb_ = uid, uid = null
  if (typeof cb_ !== "function") cb_ = mode, mode = npm.modes.exec

  if (mode & umask) {
    log.verbose(mode.toString(8), "umasking from "+umask.toString(8))
    process.umask(umask = 0)
    if (!addedUmaskExit) {
      addedUmaskExit = true
      process.on("exit", function () { process.umask(umask = umaskOrig) })
    }
  }

  ensure = path.resolve(ensure).replace(/\/+$/, '')

  // mkdir("/") should not do anything, since that always exists.
  if (!ensure
      || ( process.platform === "win32"
         && ensure.match(/^[a-zA-Z]:(\\|\/)?$/))) {
    return cb_()
  }

  if (mkdirCache.hasOwnProperty(ensure)) {
    return mkdirCache[ensure].push(cb_)
  }
  mkdirCache[ensure] = [cb_]

  function cb (er) {
    var cbs = mkdirCache[ensure]
    delete mkdirCache[ensure]
    cbs.forEach(function (c) { c(er) })
  }

  if (uid === null && gid === null) {
    return mkdir_(ensure, mode, uid, gid, noChmod, cb)
  }

  uidNumber(uid, gid, function (er, uid, gid) {
    if (er) return cb(er)
    mkdir_(ensure, mode, uid, gid, noChmod, cb)
  })
}

function mkdir_ (ensure, mode, uid, gid, noChmod, cb) {
  // if it's already a dir, then just check the bits and owner.
  fs.stat(ensure, function (er, s) {
    if (s && s.isDirectory()) {
      // check mode, uid, and gid.
      if ((noChmod || (s.mode & mode) === mode)
          && (typeof uid !== "number" || s.uid === uid)
          && (typeof gid !== "number" || s.gid === gid)) return cb()
      return done(ensure, mode, uid, gid, noChmod, cb)
    }
    return walkDirs(ensure, mode, uid, gid, noChmod, cb)
  })
}

function done (ensure, mode, uid, gid, noChmod, cb) {
  // now the directory has been created.
  // chown it to the desired uid/gid
  // Don't chown the npm.root dir, though, in case we're
  // in unsafe-perm mode.
  log.verbose("done: "+ensure+" "+mode.toString(8), "mkdir")

  // only chmod if noChmod isn't set.
  var d = done_(ensure, mode, uid, gid, cb)
  if (noChmod) return d()
  fs.chmod(ensure, mode, d)
}

function done_ (ensure, mode, uid, gid, cb) {
  return function (er) {
    if (er
        || ensure === npm.dir
        || typeof uid !== "number"
        || typeof gid !== "number"
        || npm.config.get("unsafe-perm")) return cb(er)
    uid = Math.floor(uid)
    gid = Math.floor(gid)
    fs.chown(ensure, uid, gid, cb)
  }
}

var pathSplit = process.platform === "win32" ? /\/|\\/ : "/"
function walkDirs (ensure, mode, uid, gid, noChmod, cb) {
  var dirs = ensure.split(pathSplit)
    , walker = []
    , foundUID = null
    , foundGID = null

  // gobble the "/" or C: first
  walker.push(dirs.shift())

  // The loop that goes through and stats each dir.
  ;(function S (d) {
    // no more directory steps left.
    if (d === undefined) {
      // do the chown stuff
      return done(ensure, mode, uid, gid, noChmod, cb)
    }

    // get the absolute dir for the next piece being stat'd
    walker.push(d)
    var dir = walker.join(path.SPLIT_CHAR)

    // stat callback lambda
    fs.stat(dir, function STATCB (er, s) {
      if (er) {
        // the stat failed - directory does not exist.

        log.verbose(er.message, "mkdir (expected) error")

        // use the same uid/gid as the nearest parent, if not set.
        if (foundUID !== null) uid = foundUID
        if (foundGID !== null) gid = foundGID

        // make the directory
        fs.mkdir(dir, mode, function MKDIRCB (er) {
          // since stat and mkdir are done as two separate syscalls,
          // operating on a path rather than a file descriptor, it's
          // possible that the directory didn't exist when we did
          // the stat, but then *did* exist when we go to to the mkdir.
          // If we didn't care about uid/gid, we could just mkdir
          // repeatedly, failing on any error other than "EEXIST".
          if (er && er.message.indexOf("EEXIST") === 0) {
            return fs.stat(dir, STATCB)
          }

          // any other kind of error is not saveable.
          if (er) return cb(er)

          // at this point, we've just created a new directory successfully.

          // if we care about permissions
          if (!npm.config.get("unsafe-perm") // care about permissions
              // specified a uid and gid
              && uid !== null
              && gid !== null ) {
            // set the proper ownership
            return fs.chown(dir, uid, gid, function (er) {
              if (er) return cb(er)
              // attack the next portion of the path.
              S(dirs.shift())
            })
          } else {
            // either we don't care about ownership, or it's already right.
            S(dirs.shift())
          }
        }) // mkdir

      } else {
        // the stat succeeded.
        if (s.isDirectory()) {
          // if it's a directory, that's good.
          // if the uid and gid aren't already set, then try to preserve
          // the ownership on up the tree.  Things in ~ remain owned by
          // the user, things in / remain owned by root, etc.
          if (uid === null && typeof s.uid === "number") foundUID = s.uid
          if (gid === null && typeof s.gid === "number") foundGID = s.gid

          // move onto next portion of path
          S(dirs.shift())

        } else {
          // the stat succeeded, but it's not a directory
          log.verbose(dir, "mkdir exists")
          log.silly(s, "stat("+dir+")")
          log.verbose(s.isDirectory(), "isDirectory()")
          cb(new Error("Failed to mkdir "+dir+": File exists"))
        }// if (isDirectory) else
      } // if (stat failed) else
    }) // stat

  // start the S function with the first item in the list of directories.
  })(dirs.shift())
}
