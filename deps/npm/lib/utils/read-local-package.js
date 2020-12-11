const { resolve } = require('path')
const readJson = require('read-package-json-fast')
const npm = require('../npm.js')

async function readLocalPackageName (cb) {
  if (npm.flatOptions.global)
    return

  const filepath = resolve(npm.flatOptions.prefix, 'package.json')
  return (await readJson(filepath)).name
}

module.exports = readLocalPackageName
