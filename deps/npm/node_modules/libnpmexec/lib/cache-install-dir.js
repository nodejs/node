const crypto = require('crypto')

const { resolve } = require('path')

const cacheInstallDir = ({ cache, packages }) => {
  if (!cache)
    throw new Error('Must provide a valid cache path')

  // only packages not found in ${prefix}/node_modules
  return resolve(cache, '_npx', getHash(packages))
}

const getHash = (packages) =>
  crypto.createHash('sha512')
    .update(packages.sort((a, b) => a.localeCompare(b, 'en')).join('\n'))
    .digest('hex')
    .slice(0, 16)

module.exports = cacheInstallDir
