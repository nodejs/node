const { resolve } = require('path')
const readJson = require('read-package-json-fast')
async function readLocalPackageName (prefix) {
  const filepath = resolve(prefix, 'package.json')
  const json = await readJson(filepath)
  return json.name
}

module.exports = readLocalPackageName
