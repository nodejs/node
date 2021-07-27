// XXX this can probably be replaced with gentle-fs.mkdir everywhere it's used
const chownr = require('chownr')
const inflight = require('inflight')
const log = require('npmlog')
const mkdirp = require('mkdirp')
const inferOwner = require('infer-owner')

// retain ownership of the parent dir
// this matches behavior in cacache to infer the cache ownership
// based on the ownership of the cache folder or it is parent.

module.exports = function correctMkdir (path, cb) {
  cb = inflight('correctMkdir: ' + path, cb)
  if (!cb) {
    return log.verbose('correctMkdir', path, 'correctMkdir already in flight; waiting')
  } else {
    log.verbose('correctMkdir', path, 'correctMkdir not in flight; initializing')
  }

  if (!process.getuid) {
    log.verbose('makeCacheDir', 'UID & GID are irrelevant on', process.platform)
    return mkdirp(path, (er, made) => cb(er, { uid: 0, gid: 0 }))
  }

  inferOwner(path).then(owner => {
    mkdirp(path, (er, made) => {
      if (er) {
        log.error('correctMkdir', 'failed to make directory %s', path)
        return cb(er)
      }
      chownr(made || path, owner.uid, owner.gid, (er) => cb(er, owner))
    })
  }, er => {
    log.error('correctMkdir', 'failed to infer path ownership %s', path)
    return cb(er)
  })
}
