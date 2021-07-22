const crypto = require('crypto')

const { resolve } = require('path')

const cacheInstallDir = ({ npxCache, packages }) => {
  if (!npxCache)
    throw new Error('Must provide a valid npxCache path')

  // only packages not found in ${prefix}/node_modules
  return resolve(npxCache, getHash(packages))
}

const getHash = (packages) =>
  crypto.createHash('sha512')
    .update(packages.sort((a, b) => a.localeCompare(b, 'en')).join('\n'))
    .digest('hex')
    .slice(0, 16)

module.exports = cacheInstallDir
