const util = require('util')
const fs = require('fs')
const readdir = util.promisify(fs.readdir)

async function isNodeGypPackage(path) {
  const files = await readdir(path)
  return files.some(f => /.*\.gyp$/.test(f))
}

module.exports = {
  isNodeGypPackage,
  defaultGypInstallScript: 'node-gyp rebuild'
}
