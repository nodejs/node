'use strict'

const fs = require('fs/promises')
const { moveFile: move } = require('@npmcli/fs')
const pinflight = require('promise-inflight')

module.exports = moveFile

async function moveFile (src, dest) {
  const isWindows = process.platform === 'win32'

  // This isn't quite an fs.rename -- the assumption is that
  // if `dest` already exists, and we get certain errors while
  // trying to move it, we should just not bother.
  //
  // In the case of cache corruption, users will receive an
  // EINTEGRITY error elsewhere, and can remove the offending
  // content their own way.
  //
  // Note that, as the name suggests, this strictly only supports file moves.
  try {
    await fs.link(src, dest)
  } catch (err) {
    if (isWindows && err.code === 'EPERM') {
      // XXX This is a really weird way to handle this situation, as it
      // results in the src file being deleted even though the dest
      // might not exist.  Since we pretty much always write files to
      // deterministic locations based on content hash, this is likely
      // ok (or at worst, just ends in a future cache miss).  But it would
      // be worth investigating at some time in the future if this is
      // really what we want to do here.
    } else if (err.code === 'EEXIST' || err.code === 'EBUSY') {
      // file already exists, so whatever
    } else {
      throw err
    }
  }
  try {
    await Promise.all([
      fs.unlink(src),
      !isWindows && fs.chmod(dest, '0444'),
    ])
  } catch (e) {
    return pinflight('cacache-move-file:' + dest, async () => {
      await fs.stat(dest).catch((err) => {
        if (err.code !== 'ENOENT') {
          // Something else is wrong here. Bail bail bail
          throw err
        }
      })
      // file doesn't already exist! let's try a rename -> copy fallback
      // only delete if it successfully copies
      return move(src, dest)
    })
  }
}
