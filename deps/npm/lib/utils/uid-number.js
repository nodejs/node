module.exports = uidNumber

// This module calls into bin/npm-get-uid-gid.js, which sets the
// uid and gid to the supplied argument, in order to find out their
// numeric value.  This can't be done in the main node process,
// because otherwise npm would be running as that user.

var exec = require("./exec.js")
  , path = require("path")
  , log = require("./log.js")
  , constants = require("constants")
  , npm = require("../npm.js")
  , uidSupport = process.getuid && process.setuid
  , uidCache = {}
  , gidCache = {}

function uidNumber (uid, gid, cb) {
  if (!uidSupport || npm.config.get("unsafe-perm")) return cb()
  if (typeof cb !== "function") cb = gid, gid = null
  if (typeof cb !== "function") cb = uid, uid = null
  if (gid == null) gid = process.getgid()
  if (uid == null) uid = process.getuid()
  if (!isNaN(gid)) gid = +gid
  if (!isNaN(uid)) uid = +uid

  if (uidCache[uid]) uid = uidCache[uid]
  if (gidCache[gid]) gid = gidCache[gid]

  if (typeof gid === "number" && typeof uid === "number") {
    return cb(null, uid, gid)
  }

  var getter = path.join(__dirname, "..", "..", "bin", "npm-get-uid-gid.js")
  return exec( process.execPath, [getter, uid, gid], process.env, false
             , null, process.getuid(), process.getgid()
             , function (er, code, out, err) {
    if (er) return log.er(cb, "Could not get uid/gid "+err)(er)
    log.silly(out, "output from getuid/gid")
    out = JSON.parse(out+"")
    if (out.error) {
      if (!npm.config.get("unsafe-perm")) {
        var er = new Error(out.error)
        er.errno = out.errno
        return cb(er)
      } else {
        return cb(null, +process.getuid(), +process.getgid())
      }
    }
    if (isNaN(out.uid) || isNaN(out.gid)) return cb(new Error(
      "Could not get uid/gid: "+JSON.stringify(out)))
    uidCache[uid] = out.uid
    uidCache[gid] = out.gid
    cb(null, out.uid, out.gid)
  })
}
