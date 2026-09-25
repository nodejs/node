// check to see if a bin is allowed to be overwritten
// either rejects or resolves to nothing.  return value not relevant.
const isWindows = require('./is-windows.js')
const binTarget = require('./bin-target.js')
const { resolve, dirname, sep } = require('path')
const readCmdShim = require('read-cmd-shim')
const { readlink } = require('fs/promises')

const checkBin = async ({ bin, path, top, global, force }) => {
  // always ok to clobber when forced
  // always ok to clobber local bins, or when forced
  if (force || !global || !top) {
    return
  }

  // ok, need to make sure, then
  const target = resolve(binTarget({ path, top }), bin)
  path = resolve(path)
  return isWindows ? checkShim({ target, path }) : checkLink({ target, path })
}

// only enoent is allowed.  anything else is a problem.
const handleReadLinkError = async ({ er, target }) =>
  er.code === 'ENOENT' ? null
  : failEEXIST({ target })

const checkLink = async ({ target, path }) => {
  const current = await readlink(target)
    .catch(er => handleReadLinkError({ er, target }))

  if (!current) {
    return
  }

  const resolved = resolve(dirname(target), current)

  const resolvedLower = resolved.toLowerCase()
  const pathLower = path.toLowerCase()
  if (resolvedLower !== pathLower && !resolvedLower.startsWith(pathLower + sep)) {
    return failEEXIST({ target })
  }
}

const handleReadCmdShimError = ({ er, target }) =>
  er.code === 'ENOENT' ? null
  : failEEXIST({ target })

const failEEXIST = ({ target }) =>
  Promise.reject(Object.assign(new Error('EEXIST: file already exists'), {
    path: target,
    code: 'EEXIST',
  }))

const checkShim = async ({ target, path }) => {
  const shims = [
    target,
    target + '.cmd',
    target + '.ps1',
  ]
  await Promise.all(shims.map(async shim => {
    const current = await readCmdShim(shim)
      .catch(er => handleReadCmdShimError({ er, target: shim }))

    if (!current) {
      return
    }

    const resolved = resolve(dirname(shim), current.replace(/\\/g, '/'))

    const resolvedLower = resolved.toLowerCase()
    const pathLower = path.toLowerCase()
    if (resolvedLower !== pathLower && !resolvedLower.startsWith(pathLower + sep)) {
      return failEEXIST({ target: shim })
    }
  }))
}

module.exports = checkBin
