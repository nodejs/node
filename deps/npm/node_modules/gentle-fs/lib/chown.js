'use strict'

// A module for chowning things we just created, to preserve
// ownership of new links and directories.

const chownr = require('chownr')

const selfOwner = {
  uid: process.getuid && process.getuid(),
  gid: process.getgid && process.getgid()
}

module.exports = (path, uid, gid, cb) => {
  if (selfOwner.uid !== 0 ||
      uid === undefined || gid === undefined ||
      (selfOwner.uid === uid && selfOwner.gid === gid)) {
    // don't need to, or can't chown anyway, so just leave it.
    // this also handles platforms where process.getuid is undefined
    return cb()
  }
  chownr(path, uid, gid, cb)
}

module.exports.selfOwner = selfOwner
