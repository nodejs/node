const inferOwner = require('infer-owner')
const mkdirp = require('mkdirp')
const {promisify} = require('util')
const chownr = promisify(require('chownr'))

const platform = process.env.__TESTING_MKDIRP_INFER_OWNER_PLATFORM__
  || process.platform
const isWindows = platform === 'win32'
const isRoot = process.getuid && process.getuid() === 0
const doChown = !isWindows && isRoot

module.exports = !doChown ? (path, opts) => mkdirp(path, opts)
  : (path, opts) => inferOwner(path).then(({uid, gid}) =>
    mkdirp(path, opts).then(made =>
      uid !== 0 || gid !== process.getgid()
      ? chownr(made || path, uid, gid).then(() => made)
      : made))

module.exports.sync = !doChown ? (path, opts) => mkdirp.sync(path, opts)
  : (path, opts) => {
    const {uid, gid} = inferOwner.sync(path)
    const made = mkdirp.sync(path)
    if (uid !== 0 || gid !== process.getgid())
      chownr.sync(made || path, uid, gid)
    return made
  }
