// if the thing isn't there, skip it
// if there's a non-symlink there already, eexist
// if there's a symlink already, pointing somewhere else, eexist
// if there's a symlink already, pointing into our pkg, remove it first
// then create the symlink

const { promisify } = require('util')
const { resolve, dirname } = require('path')
const mkdirp = require('mkdirp-infer-owner')
const fs = require('fs')
const symlink = promisify(fs.symlink)
const readlink = promisify(fs.readlink)
const lstat = promisify(fs.lstat)
const throwNonEnoent = er => { if (er.code !== 'ENOENT') throw er }

// even in --force mode, we never create a link over a link we've
// already created.  you can have multiple packages in a tree trying
// to contend for the same bin, or the same manpage listed multiple times,
// which creates a race condition and nondeterminism.
const seen = new Set()

// disable glob in our rimraf calls
const rimraf = promisify(require('rimraf'))
const rm = path => rimraf(path, { glob: false })

const SKIP = Symbol('skip - missing or already installed')
const CLOBBER  = Symbol('clobber - ours or in forceful mode')

const linkGently = async ({path, to, from, absFrom, force}) => {
  if (seen.has(to))
    return true
  seen.add(to)

  // if the script or manpage isn't there, just ignore it.
  // this arguably *should* be an install error of some sort,
  // or at least a warning, but npm has always behaved this
  // way in the past, so it'd be a breaking change
  return Promise.all([
    lstat(absFrom).catch(throwNonEnoent),
    lstat(to).catch(throwNonEnoent),
  ]).then(([stFrom, stTo]) => {
    // not present in package, skip it
    if (!stFrom)
      return SKIP

    // exists! maybe clobber if we can
    if (stTo) {
      if (!stTo.isSymbolicLink())
        return force && rm(to).then(() => CLOBBER)

      return readlink(to).then(target => {
        if (target === from)
          return SKIP // skip it, already set up like we want it.

        target = resolve(dirname(to), target)
        if (target.indexOf(path) === 0 || force)
          return rm(to).then(() => CLOBBER)
      })
    } else {
      // doesn't exist, dir might not either
      return mkdirp(dirname(to))
    }
  })
  .then(skipOrClobber => {
    if (skipOrClobber === SKIP)
      return false
    return symlink(from, to, 'file').catch(er => {
      if (skipOrClobber === CLOBBER || force)
        return rm(to).then(() => symlink(from, to, 'file'))
      throw er
    }).then(() => true)
  })
}

const resetSeen = () => {
  for (const p of seen) {
    seen.delete(p)
  }
}

module.exports = Object.assign(linkGently, { resetSeen })
