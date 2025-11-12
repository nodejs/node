const util = require('util')
const fs = require('fs')
const { stat } = fs.promises || { stat: util.promisify(fs.stat) }

async function isNodeGypPackage (path) {
  return await stat(`${path}/binding.gyp`)
    .then(st => st.isFile())
    .catch(() => false)
}

module.exports = {
  isNodeGypPackage,
  defaultGypInstallScript: 'node-gyp rebuild',
}
