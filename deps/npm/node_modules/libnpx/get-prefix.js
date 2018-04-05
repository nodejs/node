'use strict'

const promisify = require('./util.js').promisify

const path = require('path')
const statAsync = promisify(require('fs').stat)

module.exports = getPrefix
function getPrefix (root) {
  const original = root = path.resolve(root)
  while (path.basename(root) === 'node_modules') {
    root = path.dirname(root)
  }
  if (original !== root) {
    return Promise.resolve(root)
  } else {
    return Promise.resolve(getPrefixFromTree(root))
  }
}

function getPrefixFromTree (current) {
  if (isRootPath(current, process.platform)) {
    return false
  } else {
    return Promise.all([
      fileExists(path.join(current, 'package.json')),
      fileExists(path.join(current, 'node_modules'))
    ]).then(args => {
      const hasPkg = args[0]
      const hasModules = args[1]
      if (hasPkg || hasModules) {
        return current
      } else {
        return getPrefixFromTree(path.dirname(current))
      }
    })
  }
}

module.exports._fileExists = fileExists
function fileExists (f) {
  return statAsync(f).catch(err => {
    if (err.code !== 'ENOENT') {
      throw err
    }
  })
}

module.exports._isRootPath = isRootPath
function isRootPath (p, platform) {
  return platform === 'win32'
    ? p.match(/^[a-z]+:[/\\]?$/i)
    : p === '/'
}
