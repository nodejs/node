'use strict'

const mkdirp = require('mkdirp')
const inferOwner = require('infer-owner')
const chown = require('./chown.js')

module.exports = (path, cb) => {
  // don't bother chowning if we can't anyway
  if (process.platform === 'win32' || chown.selfOwner.uid !== 0) {
    return mkdirp(path, cb)
  }

  inferOwner(path).then(owner => {
    mkdirp(path, (er, made) => {
      if (er || !made) {
        cb(er, made)
      } else {
        chown(made || path, owner.uid, owner.gid, cb)
      }
    })
  }, cb)
}
