const util = require('util')
const {stat} = require('fs').promises

async function isNodeGypPackage(path) {
  return await stat(`${path}/binding.gyp`)
    .then(st => st.isFile())
    .catch(() => false)
}

module.exports = {
  isNodeGypPackage,
  defaultGypInstallScript: 'node-gyp rebuild'
}
