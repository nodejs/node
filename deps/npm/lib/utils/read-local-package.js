const { resolve } = require('path')
const readJson = require('read-package-json-fast')
async function readLocalPackageName (npm) {
  if (npm.config.get('global'))
    return

  const filepath = resolve(npm.prefix, 'package.json')
  const json = await readJson(filepath)
  return json.name
}

module.exports = readLocalPackageName
